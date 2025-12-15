import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import json
import os
import sys

# Configuration
# Path relative to this script: ../prompts.json
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROMPTS_FILE = os.path.abspath(os.path.join(CURRENT_DIR, "..", "prompts.json"))

class GeminiPromptManager(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Gemini Prompt Manager - System & Tool Defs Editor")
        self.geometry("1000x700")
        
        self.prompts_data = {"system_instruction": "", "tools": []}
        self.current_tool_index = None

        self.create_widgets()
        self.load_prompts()

    def create_widgets(self):
        # Grid Config
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)

        # --- Sidebar: Tools List ---
        sidebar_frame = ttk.Frame(self, padding="5")
        sidebar_frame.grid(row=0, column=0, rowspan=2, sticky="ns", padx=(10, 5), pady=10)
        
        ttk.Label(sidebar_frame, text="Tools Definitions", font=("Segoe UI", 9, "bold")).pack(anchor="w")
        self.tool_listbox = tk.Listbox(sidebar_frame, width=35, height=30, font=("Consolas", 10))
        self.tool_listbox.pack(fill=tk.BOTH, expand=True, pady=(0, 5))
        self.tool_listbox.bind("<<ListboxSelect>>", self.on_tool_select)
        
        btn_frame = ttk.Frame(sidebar_frame)
        btn_frame.pack(fill=tk.X)
        ttk.Button(btn_frame, text="New Tool", width=10, command=self.add_tool).pack(side=tk.LEFT, padx=1)
        ttk.Button(btn_frame, text="Delete", width=10, command=self.delete_tool).pack(side=tk.LEFT, padx=1)
        
        # --- Main Content ---
        content_frame = ttk.Frame(self, padding="5")
        content_frame.grid(row=0, column=1, sticky="nsew", padx=10, pady=10)
        
        # 1. System Instruction Editor (Top)
        sys_frame = ttk.LabelFrame(content_frame, text="System Instruction (Global Prompt)", padding="5")
        sys_frame.pack(fill=tk.X, pady=(0, 10))
        self.sys_inst_text = tk.Text(sys_frame, height=5, font=("Consolas", 10))
        self.sys_inst_text.pack(fill=tk.X)
        ttk.Button(sys_frame, text="Update System Instruction", command=self.save_system_instruction).pack(anchor="e", pady=5)

        # 2. Tool Editor (Bottom)
        self.tool_frame = ttk.LabelFrame(content_frame, text="Edit Selected Tool", padding="10")
        self.tool_frame.pack(fill=tk.BOTH, expand=True)
        
        # Tool Name
        ttk.Label(self.tool_frame, text="Tool Name:").pack(anchor="w")
        self.tool_name_var = tk.StringVar()
        self.tool_name_entry = ttk.Entry(self.tool_frame, textvariable=self.tool_name_var, width=50)
        self.tool_name_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Tool Description
        ttk.Label(self.tool_frame, text="Description (Prompt):").pack(anchor="w")
        self.tool_desc_text = tk.Text(self.tool_frame, height=10, font=("Consolas", 10))
        self.tool_desc_text.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        # Save Tool Button
        self.save_tool_btn = ttk.Button(self.tool_frame, text="Save Tool Changes", command=self.save_current_tool)
        self.save_tool_btn.pack(side=tk.RIGHT)
        
        # Status Bar
        self.status_var = tk.StringVar(value=f"Target: {PROMPTS_FILE}")
        ttk.Label(self, textvariable=self.status_var, relief=tk.SUNKEN, anchor="w").grid(row=2, column=0, columnspan=2, sticky="ew")

    def load_prompts(self):
        if not os.path.exists(PROMPTS_FILE):
             self.status_var.set("File not found.")
             self.prompts_data = {"system_instruction": "", "tools": []}
        else:
            try:
                with open(PROMPTS_FILE, 'r', encoding='utf-8') as f:
                    self.prompts_data = json.load(f)
                self.status_var.set(f"Loaded {PROMPTS_FILE}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to load JSON: {e}")
                self.prompts_data = {"system_instruction": "", "tools": []}

        # Populate UI
        self.sys_inst_text.delete("1.0", tk.END)
        self.sys_inst_text.insert("1.0", self.prompts_data.get("system_instruction", ""))
        self.update_tool_list()

    def update_tool_list(self):
        self.tool_listbox.delete(0, tk.END)
        tools = self.prompts_data.get("tools", [])
        for t in tools:
            self.tool_listbox.insert(tk.END, t.get("name", "Unnamed"))

    def on_tool_select(self, event):
        selection = self.tool_listbox.curselection()
        if not selection: return
        
        index = selection[0]
        self.current_tool_index = index
        tools = self.prompts_data.get("tools", [])
        
        if 0 <= index < len(tools):
            tool = tools[index]
            self.tool_name_var.set(tool.get("name", ""))
            self.tool_desc_text.delete("1.0", tk.END)
            self.tool_desc_text.insert("1.0", tool.get("description", ""))

    def add_tool(self):
        name = simpledialog.askstring("New Tool", "Tool Name:")
        if name:
            new_tool = {"name": name, "description": "New description..."}
            self.prompts_data.setdefault("tools", []).append(new_tool)
            self.update_tool_list()
            self.save_to_file()

    def delete_tool(self):
        selection = self.tool_listbox.curselection()
        if not selection: return
        if messagebox.askyesno("Confirm", "Delete this tool definition?"):
            del self.prompts_data["tools"][selection[0]]
            self.update_tool_list()
            self.clear_tool_editor()
            self.save_to_file()

    def clear_tool_editor(self):
        self.tool_name_var.set("")
        self.tool_desc_text.delete("1.0", tk.END)
        self.current_tool_index = None

    def save_system_instruction(self):
        self.prompts_data["system_instruction"] = self.sys_inst_text.get("1.0", tk.END).strip()
        self.save_to_file()

    def save_current_tool(self):
        if self.current_tool_index is not None:
            tools = self.prompts_data.get("tools", [])
            if 0 <= self.current_tool_index < len(tools):
                tools[self.current_tool_index]["name"] = self.tool_name_var.get()
                tools[self.current_tool_index]["description"] = self.tool_desc_text.get("1.0", tk.END).strip()
                self.save_to_file()
                self.update_tool_list() # refresh name

    def save_to_file(self):
        try:
            with open(PROMPTS_FILE, 'w', encoding='utf-8') as f:
                json.dump(self.prompts_data, f, indent=4)
            self.status_var.set(f"Saved to {PROMPTS_FILE}")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save: {e}")

if __name__ == "__main__":
    app = GeminiPromptManager()
    app.mainloop()
