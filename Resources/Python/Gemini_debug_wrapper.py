import sys
import subprocess
import threading
import time
import os

# Set file path relative to this script
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_FILE = os.path.join(CURRENT_DIR, "Gemini_debug_packet.log")

def log(msg):
    try:
        with open(LOG_FILE, "a", encoding='utf-8') as f:
            f.write(f"[{time.strftime('%H:%M:%S')}] {msg}\n")
    except:
        pass

# Force wrapper's own stdout to be unbuffered
sys.stdout.reconfigure(line_buffering=True)

log("="*40)
log("=== WRAPPER STARTED (Gemini -> Wrapper -> Server) ===")
log("="*40)

# We assume UmgMcpServer.py is in the same directory
server_script = os.path.join(CURRENT_DIR, "UmgMcpServer.py")

cmd = [sys.executable, "-u", server_script] # Force -u here too just in case
log(f"Launching subprocess: {cmd}")

try:
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,    # Use text mode for JSON-RPC lines
        bufsize=0     # Unbuffered pipes
    )
except Exception as e:
    log(f"CRITICAL: Failed to launch subprocess: {e}")
    sys.exit(1)

def forward_stdin():
    """Gemini -> Wrapper -> Server"""
    log("Thread: Stdin forwarding started.")
    while True:
        try:
            # Read line from Gemini (Block here is normal waiting for user input)
            line = sys.stdin.readline()
            if not line:
                log("Gemini sent EOF (Stdin closed). Stopping wrapper.")
                if proc.stdin:
                    proc.stdin.close()
                break
            
            # Log what we got
            content = line.strip()
            if content:
                log(f"REQ [Gemini->Server]: {content}")
            
            # Forward to Server
            if proc.stdin:
                proc.stdin.write(line)
                proc.stdin.flush()
                if content:
                    log("REQ forwarded.")
        except Exception as e:
            log(f"Stdin Error: {e}")
            break

def forward_stdout():
    """Server -> Wrapper -> Gemini"""
    log("Thread: Stdout forwarding started.")
    while True:
        try:
            # Read line from Server
            if not proc.stdout: break
            line = proc.stdout.readline()
            if not line:
                log("Server sent EOF (Stdout closed).")
                break
            
            # Log what we got
            content = line.strip()
            if content:
                log(f"RES [Server->Gemini]: {content}")
            
            # Forward to Gemini
            sys.stdout.write(line)
            sys.stdout.flush()
            if content:
                log("RES forwarded.")

        except Exception as e:
            log(f"Stdout Error: {e}")
            break

def forward_stderr():
    """Server Stderr -> Wrapper Stderr (and Log)"""
    log("Thread: Stderr forwarding started.")
    while True:
        try:
            if not proc.stderr: break
            line = proc.stderr.readline()
            if not line: break
            
            content = line.strip()
            if content:
                log(f"LOG [Server-ERR]: {content}")
            
            sys.stderr.write(line)
            sys.stderr.flush()
        except: 
            break

# Start threads
t1 = threading.Thread(target=forward_stdin, daemon=True)
t2 = threading.Thread(target=forward_stdout, daemon=True)
t3 = threading.Thread(target=forward_stderr, daemon=True)

t1.start()
t2.start()
t3.start()

# Wait for process
exit_code = proc.wait()
log(f"Subprocess exited with {exit_code}")
log("=== WRAPPER ENDED ===")
sys.exit(exit_code)
