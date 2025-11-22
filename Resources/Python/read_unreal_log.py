import os
import sys

def read_last_lines(file_path, num_lines=200):
    """Reads the last N lines of a file efficiently."""
    try:
        with open(file_path, 'rb') as f:
            f.seek(0, os.SEEK_END)
            file_size = f.tell()
            
            block_size = 1024
            data = b''
            lines_found = 0
            
            # Read backwards
            while f.tell() > 0 and lines_found < num_lines:
                seek_pos = max(0, f.tell() - block_size)
                f.seek(seek_pos)
                block = f.read(f.tell() - seek_pos)
                f.seek(seek_pos) # Reset position for next read
                
                data = block + data
                lines_found = data.count(b'\n')
                
                if seek_pos == 0:
                    break
            
            lines = data.decode('utf-8', errors='replace').splitlines()
            return lines[-num_lines:]
    except FileNotFoundError:
        print(f"Error: File not found at {file_path}")
        return []
    except Exception as e:
        print(f"Error reading file: {e}")
        return []

def main():
    # Default path based on user context
    log_path = r"d:\ModelContextProtocol\unreal-engine-mcp\FlopperamUnrealMCP\Saved\Logs\FlopperamUnrealMCP.log"
    
    if len(sys.argv) > 1:
        log_path = sys.argv[1]
        
    print(f"Reading last 200 lines from: {log_path}")
    print("-" * 80)
    
    lines = read_last_lines(log_path, 200)
    
    # Filter for relevant lines (Errors, Warnings, or our Plugin logs)
    relevant_keywords = ["Error:", "Warning:", "Exception", "UmgMcp", "LogUmg", "Crash", "Assert"]
    
    for line in lines:
        # Print line if it contains a keyword or if it's very recent (last 20 lines)
        # to give context around the crash
        is_relevant = any(keyword in line for keyword in relevant_keywords)
        if is_relevant or line in lines[-20:]:
            print(line)

if __name__ == "__main__":
    main()
