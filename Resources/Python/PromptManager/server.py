import http.server
import socketserver
import json
import os
import urllib.parse
import webbrowser

# Configuration
PORT = 8085
# Path derived relative to this script: ../../prompts.json
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROMPTS_FILE = os.path.abspath(os.path.join(CURRENT_DIR, "..", "..", "prompts.json"))

class PromptRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        # Serve API GET request
        if self.path == '/api/prompts':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            try:
                if os.path.exists(PROMPTS_FILE):
                    with open(PROMPTS_FILE, 'r', encoding='utf-8') as f:
                        self.wfile.write(f.read().encode('utf-8'))
                else:
                    self.wfile.write(json.dumps({"system_instruction": "", "tools": []}).encode('utf-8'))
            except Exception as e:
                self.wfile.write(json.dumps({"error": str(e)}).encode('utf-8'))
            return
        
        # Serve Static Files
        # Map root '/' to '/static/index.html'
        if self.path == '/' or self.path == '/index.html':
            self.path = '/static/index.html'
        
        # Serve allowed static files
        if self.path.startswith('/static/'):
            # Security check: ensure path is within current directory range
            # SimpleHTTPRequestHandler serves relative to CWD.
            # We need to make sure we serve from the script's dir.
            return super().do_GET()
        
        self.send_error(404, "File not found")

    def do_POST(self):
        if self.path == '/api/save':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            try:
                data = json.loads(post_data.decode('utf-8'))
                
                # Validation: basic check
                if "tools" not in data or "system_instruction" not in data:
                     raise ValueError("Invalid JSON structure")

                # Save to disk
                with open(PROMPTS_FILE, 'w', encoding='utf-8') as f:
                    json.dump(data, f, indent=4)
                
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"status": "success", "message": "Saved successfully"}).encode('utf-8'))
                
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"status": "error", "message": str(e)}).encode('utf-8'))
            return

def run():
    # Change working directory to this script's directory so SimpleHTTPRequestHandler finds 'static'
    os.chdir(CURRENT_DIR)
    
    with socketserver.TCPServer(("", PORT), PromptRequestHandler) as httpd:
        print(f"Serving Prompt Manager at http://localhost:{PORT}")
        # Automatically open browser
        webbrowser.open(f"http://localhost:{PORT}")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server...")
            httpd.shutdown()

if __name__ == "__main__":
    run()
