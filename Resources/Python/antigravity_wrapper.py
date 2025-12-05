import sys
import subprocess
import os

# This wrapper is essential for running the MCP server in Antigravity on Windows.
# Antigravity's JSON-RPC parser is strict and rejects Windows CRLF (\r\n) line endings.
# This script intercepts the server's stdout and strips all \r bytes, ensuring clean \n output.

# Usage: python antigravity_wrapper.py <TargetServerScript.py>

PYTHON_EXE = sys.executable

def main():
    if len(sys.argv) < 2:
        sys.stderr.write("[AntigravityWrapper] Error: No target script specified.\n")
        sys.exit(1)

    target_script = sys.argv[1]
    script_dir = os.path.dirname(__file__)
    real_server_path = os.path.join(script_dir, target_script)

    if not os.path.exists(real_server_path):
        sys.stderr.write(f"[AntigravityWrapper] Error: Script not found at {real_server_path}\n")
        sys.exit(1)

    # Start the real server
    process = subprocess.Popen(
        [PYTHON_EXE, real_server_path],
        stdin=sys.stdin,       # Forward stdin directly
        stdout=subprocess.PIPE, # Capture stdout for sanitization
        stderr=sys.stderr,     # Forward stderr directly (logs)
        bufsize=0              # Unbuffered
    )

    try:
        while True:
            # Read stdout byte by byte to ensure we catch every \r
            chunk = process.stdout.read(1)
            if not chunk:
                break
            
            # STRIP: Carriage Return (\r = 0x0D)
            if chunk == b'\r':
                continue
            
            # Forward clean bytes to actual stdout
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            
    except Exception as e:
        sys.stderr.write(f"[AntigravityWrapper Error]: {e}\n")
        sys.stderr.flush()

if __name__ == "__main__":
    main()
