import customtkinter as ctk
from tkinter import ttk, filedialog, messagebox
import os, re
import threading
import tempfile
from editor import CodeEditor, UI_FONT, UI_FONT_SMALL, MONOSPACED_FONT
from backend import CoNESBackend, parse_time

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

color_UI = "#0e639c"
color_UI_hover = "#105380"

class CoNESStudio(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        self.title("CoNES Studio")
        self.geometry("1100x750")
        self.configure(fg_color="#1e1e1e")
        
        # Backend & File State
        self.backend = CoNESBackend()
        self.temp_dir = os.path.join(tempfile.gettempdir(), "cones_studio")
        os.makedirs(self.temp_dir, exist_ok=True)
        self.shadow_path = os.path.join(self.temp_dir, "shadow_solve.cnes")
        
        self.current_file = None
        self.metadata = self.backend.get_metadata()
        
        # Grid Configuration
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)
        
        # Toolbar
        self.toolbar = ctk.CTkFrame(self, height=35, corner_radius=0, fg_color="#2d2d2d", border_width=0)
        self.toolbar.grid(row=0, column=0, sticky="ew")
        
        # Buttons with 0 corner radius and flat styling
        btn_opts = {
            "width": 50, 
            "height": 25, 
            "corner_radius": 0, 
            "font": UI_FONT,
            "fg_color": "transparent",
            "hover_color": "#3e3e3e",
            "text_color": "#cccccc"
        }

        self.btn_open = ctk.CTkButton(self.toolbar, text="Open", command=self.open_file, **btn_opts)
        self.btn_open.pack(side="left")

        self.btn_save = ctk.CTkButton(self.toolbar, text="Save", command=self.save_file, **btn_opts)
        self.btn_save.pack(side="left")

        # Solve button with accent color but still blocky
        self.btn_solve = ctk.CTkButton(self.toolbar, text="Solve", 
                                       fg_color=color_UI, 
                                       hover_color=color_UI_hover, 
                                       text_color="white",
                                       corner_radius=0,
                                       width=50,
                                       height=25,
                                       font=UI_FONT,
                                       command=self.run_solve)
        self.btn_solve.pack(side="left", padx=(10, 0))
        
        # Main Splitter (Flush layout)
        self.main_container = ctk.CTkFrame(self, corner_radius=0, fg_color="#1e1e1e", border_width=0)
        self.main_container.grid(row=1, column=0, sticky="nsew")
        self.main_container.grid_rowconfigure(0, weight=1)
        self.main_container.grid_columnconfigure(0, weight=3) 
        self.main_container.grid_columnconfigure(1, weight=1) 
        
        # Editor (Flush against left and toolbar)
        self.editor = CodeEditor(self.main_container, metadata=self.metadata)
        self.editor.grid(row=0, column=0, sticky="nsew")
        self.editor.on_content_changed_callback = self.run_lint
        
        # Results Pane (Flush against editor)
        self.results_tabs = ctk.CTkTabview(self.main_container, 
                                           corner_radius=0, 
                                           fg_color="#252526", 
                                           segmented_button_selected_color="#1e1e1e",
                                           segmented_button_fg_color="#2d2d2d",
                                           segmented_button_unselected_color="#2d2d2d",
                                           text_color="#888888")
        self.results_tabs.grid(row=0, column=1, sticky="nsew", padx=(1, 0))
        self.tab_solution = self.results_tabs.add("Solution")
        self.tab_residuals = self.results_tabs.add("Residuals")
        
        self._setup_solution_table()
        self.tree.bind("<<TreeviewSelect>>", self._on_table_select)
        
        # Status Bar
        self.status_bar = ctk.CTkLabel(self, text="  Ready", anchor="w", 
                                       font=UI_FONT_SMALL, height=25, 
                                       fg_color="#007acc", text_color="white",
                                       corner_radius=0)
        self.status_bar.grid(row=2, column=0, sticky="ew")

    def _on_table_select(self, event):
        """Highlights the selected variable in the code editor."""
        selected = self.tree.selection()
        if not selected: return
        
        item = self.tree.item(selected[0])
        var_name = item['values'][0] # Variable name is first col
        self.editor.highlight_symbol(var_name)

    def _setup_solution_table(self):
        style = ttk.Style()
        # Use 'clam' to get better control over the headers
        style.theme_use("clam")
        
        # Configure the Treeview colors to match editor
        style.configure("Treeview", 
                        background="#1e1e1e", 
                        foreground="#cccccc", 
                        fieldbackground="#1e1e1e",
                        font=MONOSPACED_FONT,
                        borderwidth=0,
                        relief="solid",
                        rowheight=25)
        
        style.layout("Treeview", [
            ("Treeview.border", {
                "sticky": "nswe",
                "children": [
                    ("Treeview.padding", {
                        "sticky": "nswe",
                        "children": [
                            ("Treeview.treearea", {"sticky": "nswe"})
                        ]
                    })
                ]
            })
        ])
        
        # Strip the legacy header styling
        style.configure("Treeview.Heading", 
                        background="#2d2d2d", 
                        foreground="#888888", 
                        relief="flat",
                        font=UI_FONT_SMALL,
                        borderwidth=1)
        style.map("Treeview.Heading", 
                  background=[('active', '#3e3e3e')],
                  foreground=[('active', '#ffffff')])
        
        style.map("Treeview", background=[('selected', '#37373d')])
        
        # Create Treeview inside the tab
        self.tree = ttk.Treeview(self.tab_solution, columns=("Name", "Value", "Unit", "State"), show="headings", style="Treeview")
        self.tree.heading("Name", text="NAME")
        self.tree.heading("Value", text="VALUE")
        self.tree.heading("Unit", text="UNIT")
        self.tree.heading("State", text="STATE")
        
        self.tree.column("Name", width=90, anchor="w")
        self.tree.column("Value", width=100, anchor="e")
        self.tree.column("Unit", width=60, anchor="center")
        self.tree.column("State", width=50, anchor="center")
        
        self.tree.pack(fill="both", expand=True)

    def run_lint(self):
        threading.Thread(target=self._lint_thread, daemon=True).start()

    def _lint_thread(self):
        content = self.editor.get_text()
        result = self.backend.lint(content)
        self.after(0, lambda: self._handle_lint_result(result))

    def _handle_lint_result(self, result):
        if not result.get("success"):
            line = result.get("error_line", 0)
            self.editor.highlight_error(line)
            self.status_bar.configure(text=f"  Linter: {result.get('error')}", fg_color="#a1260d")
        else:
            self.editor.highlight_error(0)
            self.status_bar.configure(text="  Ready", fg_color="#16825d")

    def open_file(self):
        path = filedialog.askopenfilename(filetypes=[("CoNES Scripts", "*.cnes"), ("All Files", "*.*")])
        if path:
            with open(path, "r") as f:
                self.editor.set_text(f.read())
            self.current_file = path
            self.status_bar.configure(text=f"  Opened {os.path.basename(path)}", fg_color="#007acc")

    def save_file(self):
        if not self.current_file:
            self.current_file = filedialog.asksaveasfilename(defaultextension=".cnes")
        
        if self.current_file:
            with open(self.current_file, "w") as f:
                f.write(self.editor.get_text())
            self.status_bar.configure(text=f"  Saved {os.path.basename(self.current_file)}", fg_color="#16825d")

    def run_solve(self):
        content = self.editor.get_text()
        with open(self.shadow_path, "w") as f:
            f.write(content)
        
        self.status_bar.configure(text="  Solving...", fg_color="#007acc")
        threading.Thread(target=self._solve_thread, daemon=True).start()

    def _solve_thread(self):
        result = self.backend.solve(self.shadow_path)
        self.after(0, lambda: self._handle_solve_result(result))

    def _handle_solve_result(self, result):
        if not result.get("success"):
            self.status_bar.configure(text=f"  Error: {result.get('error')[:100]}", fg_color="#a1260d")
            return

        for item in self.tree.get_children(): self.tree.delete(item)
        for var in result.get("variables", []):
            state = "FIX" if var["is_fixed"] else "SOL"
            self.tree.insert("", "end", values=(var["name"], f"{var['value']:.6}", var["unit"], state))
            
        perf = result.get("performance", {})
        self.status_bar.configure(text=f"  Solved ({parse_time(perf.get('solver_ms', 0))})", fg_color="#16825d")

if __name__ == "__main__":
    app = CoNESStudio()
    app.mainloop()
