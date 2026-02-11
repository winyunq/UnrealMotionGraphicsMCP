import re
import sys
import os
from collections import defaultdict

def analyze_log(log_path):
    if not os.path.exists(log_path):
        print(f"Error: Log file not found at {log_path}")
        return

    print(f"Reading: {log_path}")
    
    # Try encodings: System Default (mbcs) -> UTF-8 -> Latin-1
    encodings = ['mbcs', 'utf-8', 'latin-1']
    lines = []
    
    for enc in encodings:
        try:
            with open(log_path, 'r', encoding=enc) as f:
                lines = f.readlines()
            break
        except UnicodeDecodeError:
            continue
            
    if not lines:
        print("Failed to read log file with any encoding.")
        return

    # Pattern:  D:\...\File.cpp(123): error Cxxxx: msg
    # Regex groups: (1) FilePath, (2) Line, (3) Code, (4) Msg
    error_pattern = re.compile(r"^\s*((?:[a-zA-Z]:)?[^:(\r\n]+)\(([0-9,]+)\)\s*:\s*(?:fatal )?error\s+([A-Z]+[0-9]+)\s*:\s*(.*)")
    
    errors_by_file = defaultdict(list)
    found_count = 0

    for line in lines:
        match = error_pattern.match(line)
        if match:
            fpath, fline, code, msg = match.groups()
            
            # Make path readable (relative to plugin root if possible)
            display_path = fpath
            if "FabUmgMcp" in fpath:
                try:
                    display_path = fpath.split("FabUmgMcp")[-1].lstrip(os.sep)
                except:
                    pass
            else:
                display_path = os.path.basename(fpath)
                
            errors_by_file[display_path].append(f"Line {fline} | {code}: {msg.strip()}")
            found_count += 1
            
    if found_count == 0:
        # Fallback check for generic format
        if "error :" in "".join(lines[:100]).lower():
             print("No MSVC-style errors found, but 'error' keyword detected. Printing raw grep:")
             for line in lines:
                 if "error" in line.lower() and "warning" not in line.lower():
                     if "LogInit:" in line or "LogExit:" in line: continue
                     print(line.strip()[:150])
        else:
             print("No MSVC compilation errors found.")
        return

    print(f"Total Errors: {found_count}")
    print("=" * 60)
    
    for fpath, errs in errors_by_file.items():
        print(f"FILE: {fpath}")
        for e in errs:
            print(f"  {e}")
        print("-" * 40)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        log_file = sys.argv[1]
    else:
        # Default path requested by user
        default_log = r"D:\UE5Project\CLIUMGMCP\Saved\Logs\FabUMGMCP.log"
        if os.path.exists(default_log):
            log_file = default_log
        else:
            log_file = input("Log file path: ").strip().strip('"')
    
    analyze_log(log_file)
