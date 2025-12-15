import sys
import subprocess
import os

# Path to the real server script
REAL_SERVER = os.path.join(os.path.dirname(__file__), "UmgMcpServer.py")
PYTHON_EXE = sys.executable

def main():
    # Open a log file in binary mode to capture exact bytes
    # Use absolute path to ensure we find it
    log_path = os.path.join(os.path.dirname(__file__), "debug_stdout.log")
    with open(log_path, "wb") as log_file:
        # Start the real server
        # We use the same python executable that launched this wrapper
        process = subprocess.Popen(
            [PYTHON_EXE, REAL_SERVER],
            stdin=sys.stdin,       # Forward stdin directly
            stdout=subprocess.PIPE, # Capture stdout
            stderr=sys.stderr,     # Forward stderr directly
            bufsize=0              # Unbuffered
        )

        # Read stdout byte by byte (or chunk) and log it
        try:
            while True:
                chunk = process.stdout.read(1)
                if not chunk:
                    break
                
                # Log raw bytes to file for debugging
                log_file.write(chunk)
                log_file.flush()
                
                # FIX: Strip Carriage Return (\r = 0x0D) before writing to stdout
                # This solves the "invalid trailing data" error in Antigravity
                if chunk == b'\r':
                    continue
                
                # Forward clean bytes to actual stdout
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                
        except Exception as e:
            log_file.write(f"\n[WRAPPER ERROR]: {e}\n".encode('utf-8'))

if __name__ == "__main__":
    main()
