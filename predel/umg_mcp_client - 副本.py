import sys
import json
import subprocess
import threading
import queue
import os

# --- Configuration ---
CLIENT_DIR = os.path.dirname(os.path.abspath(__file__))
SERVER_SCRIPT_PATH = os.path.join(CLIENT_DIR, "Resources", "Python", "UmgMcpServer.py")
SERVER_WORKING_DIR = os.path.dirname(SERVER_SCRIPT_PATH)
PYTHON_EXECUTABLE = "python"  # Assumes python is in the PATH

# --- Communication Functions ---

def format_request(payload: dict) -> bytes:
    """Formats a JSON payload into a Language Server Protocol message."""
    json_payload = json.dumps(payload)
    message = f"Content-Length: {len(json_payload)}\r\n\r\n{json_payload}"
    return message.encode('utf-8')

def read_stdout_responses(proc_stdout, response_queue):
    """Reads and decodes LSP responses from the server's stdout."""
    try:
        while not proc_stdout.closed:
            header = proc_stdout.readline().decode('utf-8')
            if not header or not header.startswith("Content-Length"):
                break
            
            content_length = int(header.split(":")[1].strip())
            proc_stdout.readline()  # Consume the blank line
            
            body = proc_stdout.read(content_length).decode('utf-8')
            response_queue.put(json.loads(body))
    except Exception:
        response_queue.put({"jsonrpc": "2.0", "error": {"code": -32099, "message": "Server connection lost (stdout pipe closed)."}})

def log_stderr_output(proc_stderr):
    """Continuously reads and prints the server's stderr output."""
    try:
        for line in iter(proc_stderr.readline, b''):
            print(f"[SERVER_STDERR] {line.decode('utf-8').strip()}", file=sys.stderr)
    except Exception as e:
        print(f"[STDERR_THREAD_EXCEPTION] {e}", file=sys.stderr)

# --- Main Application ---

def main():
    """Main function to run the interactive test client."""
    print("--- Manual MCP Test Client (v2 - with stderr logging) ---")
    
    if not os.path.exists(SERVER_SCRIPT_PATH):
        print(f"FATAL ERROR: Server script not found at '{SERVER_SCRIPT_PATH}'", file=sys.stderr)
        return

    print(f"Starting server: {SERVER_SCRIPT_PATH}")
    print(f"Working Directory: {SERVER_WORKING_DIR}")

    try:
        server_process = subprocess.Popen(
            [PYTHON_EXECUTABLE, SERVER_SCRIPT_PATH],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=SERVER_WORKING_DIR
        )
    except FileNotFoundError:
        print(f"ERROR: Could not start server. Is '{PYTHON_EXECUTABLE}' in your system's PATH?", file=sys.stderr)
        return
    except Exception as e:
        print(f"ERROR: Failed to launch server process: {e}", file=sys.stderr)
        return

    response_queue = queue.Queue()
    
    stdout_thread = threading.Thread(target=read_stdout_responses, args=(server_process.stdout, response_queue))
    stdout_thread.daemon = True
    stdout_thread.start()

    stderr_thread = threading.Thread(target=log_stderr_output, args=(server_process.stderr,))
    stderr_thread.daemon = True
    stderr_thread.start()

    print("\nServer process started. You can now send commands.")
    print("="*40)
    print("Commands:")
    print("  list_tools                          - Lists all available tools.")
    print("  call <tool_name> [json_params]      - Calls a tool. Params are optional.")
    print("  exit                                - Exits the client and terminates the server.")
    print("\nExample:")
    print("  call get_creatable_widget_types")
    print('  call set_target_umg_asset {"asset_path": "/Game/NewWidgetBlueprint.NewWidgetBlueprint"}')
    print("="*40)

    request_id = 1
    try:
        while True:
            if server_process.poll() is not None:
                print("\nServer process terminated unexpectedly.", file=sys.stderr)
                break

            while not response_queue.empty():
                response = response_queue.get()
                print("\n<-- SERVER RESPONSE:")
                print(json.dumps(response, indent=2, ensure_ascii=False))
                if response.get("error", {}).get("code") == -32099:
                    raise RuntimeError("Server connection lost.")

            user_input = input("\nmcp_client> ")
            if not user_input:
                continue

            parts = user_input.strip().split(maxsplit=2)
            command = parts[0].lower()

            if command == "exit":
                break

            payload = { "jsonrpc": "2.0", "id": request_id }
            
            if command == "list_tools":
                payload["method"] = "mcp/tool/list"
                payload["params"] = {}
            elif command == "call" and len(parts) >= 2:
                payload["method"] = "mcp/tool/call"
                tool_name = parts[1]
                params_json = parts[2] if len(parts) > 2 else "{}"
                try:
                    params = json.loads(params_json)
                    payload["params"] = {"name": tool_name, "params": params}
                except json.JSONDecodeError:
                    print("ERROR: Invalid JSON in parameters.", file=sys.stderr)
                    continue
            else:
                print("ERROR: Invalid command.", file=sys.stderr)
                continue

            server_process.stdin.write(format_request(payload))
            server_process.stdin.flush()
            request_id += 1
            threading.Event().wait(0.2)

    except (KeyboardInterrupt, EOFError, RuntimeError) as e:
        print(f"\nClient shutting down: {e}")
    finally:
        print("Cleaning up server process...")
        if server_process.poll() is None:
            server_process.terminate()
            try:
                server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                print("Server did not terminate gracefully, killing.", file=sys.stderr)
                server_process.kill()
        print("Client exited.")

if __name__ == "__main__":
    main()