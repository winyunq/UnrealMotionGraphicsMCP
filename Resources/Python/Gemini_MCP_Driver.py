import tkinter as tk
from tkinter import ttk, messagebox
import json
import os
import sys

# Configuration
PROMPTS_FILE = "prompts.json"

class GeminiMCPDriver(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Gemini MCP Driver - Context & Prompt Manager")
        self.geometry("1000x700")
        
        # Data
        self.prompts_data = {}
        self.current_context = {
            "target_asset": None,
            "selected_widgets": [],
            "last_action": None
        }

        # UI Layout
        self.create_widgets()
        self.load_prompts()

    def create_widgets(self):
        # Configure Grid
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)

        # --- Header Section (Mode Selection) ---
        header_frame = ttk.Frame(self, padding="10")
        header_frame.grid(row=0, column=0, columnspan=2, sticky="ew")
        
        ttk.Label(header_frame, text="Current Domain (Semantics):", font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT, padx=(0, 10))
        
        self.mode_var = tk.StringVar()
        self.mode_combo = ttk.Combobox(header_frame, textvariable=self.mode_var, state="readonly", width=30)
        self.mode_combo.pack(side=tk.LEFT)
        self.mode_combo.bind("<<ComboboxSelected>>", self.on_mode_change)

        ttk.Button(header_frame, text="Reload Prompts", command=self.load_prompts).pack(side=tk.LEFT, padx=10)

        # --- Sidebar (Prompt List) ---
        sidebar_frame = ttk.LabelFrame(self, text="Available Prompts", padding="5")
        sidebar_frame.grid(row=1, column=0, sticky="ns", padx=10, pady=5)
        
        self.prompt_list = tk.Listbox(sidebar_frame, width=40, font=("Consolas", 10))
        self.prompt_list.pack(fill=tk.BOTH, expand=True)
        self.prompt_list.bind("<<ListboxSelect>>", self.on_prompt_select)

        # --- Main Content Area ---
        content_frame = ttk.Frame(self, padding="5")
        content_frame.grid(row=1, column=1, sticky="nsew", padx=10, pady=5)
        
        # 1. Prompt Details
        ttk.Label(content_frame, text="Selected Prompt (Meta-Instruction):", font=("Segoe UI", 9, "bold")).pack(anchor="w")
        self.prompt_text = tk.Text(content_frame, height=8, font=("Consolas", 10))
        self.prompt_text.pack(fill=tk.X, expand=False, pady=(0, 10))
        
        # 2. Context Cache Visualization
        ttk.Label(content_frame, text="Shared Context Cache (Injects into LLM):", font=("Segoe UI", 9, "bold")).pack(anchor="w")
        self.context_text = tk.Text(content_frame, height=10, bg="#f0f0f0", font=("Consolas", 10))
        self.context_text.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        self.update_context_display()

        # 3. Action Buttons
        action_frame = ttk.Frame(content_frame)
        action_frame.pack(fill=tk.X)
        
        ttk.Button(action_frame, text="Run with Context (Simulated)", command=self.run_simulation).pack(side=tk.RIGHT)
        ttk.Button(action_frame, text="Update Context", command=self.manual_update_context).pack(side=tk.RIGHT, padx=5)

    def load_prompts(self):
        try:
            if not os.path.exists(PROMPTS_FILE):
                messagebox.showerror("Error", f"Could not find {PROMPTS_FILE}")
                return
                
            with open(PROMPTS_FILE, 'r', encoding='utf-8') as f:
                self.prompts_data = json.load(f)
            
            # Populate Combo
            domains = list(self.prompts_data.keys())
            self.mode_combo['values'] = domains
            if domains:
                self.mode_combo.current(0)
                self.on_mode_change(None)
                
            print(f"Loaded {len(domains)} domains from {PROMPTS_FILE}")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load prompts: {e}")

    def on_mode_change(self, event):
        domain = self.mode_var.get()
        self.prompt_list.delete(0, tk.END)
        
        prompts = self.prompts_data.get(domain, [])
        for p in prompts:
            self.prompt_list.insert(tk.END, p.get("name", "Unknown"))
            
    def on_prompt_select(self, event):
        selection = self.prompt_list.curselection()
        if not selection:
            return
            
        index = selection[0]
        domain = self.mode_var.get()
        prompts = self.prompts_data.get(domain, [])
        
        if 0 <= index < len(prompts):
            prompt_content = prompts[index].get("prompt", "")
            self.prompt_text.delete("1.0", tk.END)
            self.prompt_text.insert("1.0", prompt_content)

    def update_context_display(self):
        self.context_text.delete("1.0", tk.END)
        self.context_text.insert("1.0", json.dumps(self.current_context, indent=2))

    def manual_update_context(self):
        try:
            new_context = json.loads(self.context_text.get("1.0", tk.END))
            self.current_context = new_context
            messagebox.showinfo("Success", "Context cache updated manually.")
        except json.JSONDecodeError as e:
            messagebox.showerror("Error", f"Invalid JSON in context window: {e}")

    def run_simulation(self):
        prompt = self.prompt_text.get("1.0", tk.END).strip()
        if not prompt:
            return
            
        # Simulation of what gets sent to the LLM
        final_payload = {
            "system_instruction": "You are a UMG Assistant.",
            "context": self.current_context,
            "user_prompt": prompt
        }
        
        print("\n--- SENDING TO LLM ---")
        print(json.dumps(final_payload, indent=2))
        print("----------------------\n")
        messagebox.showinfo("Simulated Run", "Payload printed to console. See log.")

if __name__ == "__main__":
    app = GeminiMCPDriver()
    app.mainloop()
