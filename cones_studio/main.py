# Modified from src: https://stackoverflow.com/questions/46419607/how-to-automatically-install-required-packages-from-a-python-script-as-necessary
from bootstrap import ensure_dependencies, get_all_includes, resolve_name
ensure_dependencies({'customtkinter', 'CoolProp'})

import customtkinter as ctk
from tkinter import ttk, filedialog
import pathlib
import os
import json
import threading
import tempfile
from typing import Optional
from editor import CodeEditor, UI_FONT, UI_FONT_SMALL, MONOSPACED_FONT
from backend import CoNESBackend, parse_time, is_cnes
from documentation import Documentation
from parametric_ui import ParametricPane
from colors import * # Used for less terrible formatting

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("dark-blue")

class CustomInputDialog(ctk.CTkToplevel):
    """A custom input dialog that supports a pre-filled initial value."""
    def __init__(self, text: str, title: str, initial_value: str = ""):
        super().__init__()
        self.title(title)
        self.geometry("300x150")
        self.resizable(False, False)
        self.transient(self.master) #type: ignore
        self.grab_set()
        
        self.result: Optional[str] = None
        
        self.label = ctk.CTkLabel(self, text=text, font=UI_FONT)
        self.label.pack(pady=(15, 5))
        
        self.entry = ctk.CTkEntry(self, width=200)
        self.entry.pack(pady=5)
        self.entry.insert(0, initial_value)
        self.entry.focus_set()
        
        self.btn_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.btn_frame.pack(pady=10)
        
        self.ok_btn = ctk.CTkButton(self.btn_frame, text="OK", width=80, command=self._on_ok)
        self.ok_btn.pack(side="left", padx=5)
        
        self.cancel_btn = ctk.CTkButton(self.btn_frame, text="Cancel", width=80, fg_color="transparent", 
                                        border_width=1, command=self._on_cancel)
        self.cancel_btn.pack(side="left", padx=5)
        
        self.bind("<Return>", lambda e: self._on_ok())
        self.bind("<Escape>", lambda e: self._on_cancel())
        
        self.wait_window()

    def _on_ok(self):
        self.result = self.entry.get()
        self.destroy()

    def _on_cancel(self):
        self.result = None
        self.destroy()

    def get_input(self) -> Optional[str]:
        return self.result

class CoNESStudio(ctk.CTk):
    """
    Main application window for CoNES Studio.
    Manages the editor, backend communication, and results display.
    """
    def _set_title(self, x: Optional[str] = None):
        """Sets the window title, optionally appending a file name."""
        base_title = f"CoNES Studio ({self.versionNumber})"
        self.title(f"{base_title}: {x}" if x else base_title)
        
    def __init__(self):
        super().__init__()
        
        self.backend = CoNESBackend()
        self.versionNumber = self.backend.version() or "Unknown Version"
        self._set_title(None)
        self.geometry("1100x750")
        self.configure(fg_color="#1e1e1e")
        
        # Backend & File State
        self.temp_dir = os.path.join(tempfile.gettempdir(), "cones_studio")
        os.makedirs(self.temp_dir, exist_ok=True)
        self.shadow_path = os.path.join(self.temp_dir, "shadow_solve.cnes")
        
        self.current_file: Optional[str] = None
        self.last_lib_name: str = ""
        self.metadata = self.backend.get_metadata()
        self._linting_in_progress = False
        self._lint_pending: Optional[str] = None
        
        # Grid Configuration
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)
        
        # Toolbar
        self.toolbar = ctk.CTkFrame(self, height=35, corner_radius=0, fg_color="#2d2d2d", border_width=0)
        self.toolbar.grid(row=0, column=0, sticky="ew")

        self.btn_new = ctk.CTkButton(self.toolbar, text="New", command=self.new_file, **btn_opts)
        self.btn_new.pack(side="left")

        self.btn_open = ctk.CTkButton(self.toolbar, text="Open", command=self.open_file, **btn_opts)
        self.btn_open.pack(side="left")

        self.btn_save = ctk.CTkButton(self.toolbar, text="Save", command=self.save_file, **btn_opts)
        self.btn_save.pack(side="left")

        self.btn_save_as = ctk.CTkButton(self.toolbar, text="Save As", command=self.save_file_as, **btn_opts)
        self.btn_save_as.pack(side="left")

        self.btn_save_lib = ctk.CTkButton(self.toolbar, text="Export Lib", command=self.save_lib, **btn_opts)
        self.btn_save_lib.pack(side="left")

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
        self.main_container.grid_columnconfigure(0, minsize=240, weight=4) # Editor
        self.main_container.grid_columnconfigure(1, minsize=160, weight=2) # Right column
        
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
        self.tab_parametric = self.results_tabs.add("Parametric Studies")

        # Parametric runs pane
        self.parametric_pane = ParametricPane(self.tab_parametric, self)
        self.parametric_pane.pack(fill="both", expand=True)

        self._setup_solution_table()
        self.tree.bind("<<TreeviewSelect>>", self._on_table_select)
        self.tree_res.bind("<<TreeviewSelect>>", self._on_residual_select)
        
        # Table Shortcuts
        self.tree.bind("<Control-c>", self.copy_solution_to_clipboard)
        self.tree.bind("<Command-c>", self.copy_solution_to_clipboard)
        self.tree.bind("<Control-C>", self.copy_solution_to_clipboard)
        self.tree.bind("<Command-C>", self.copy_solution_to_clipboard)
        
        self.tree_res.bind("<Control-c>", self.copy_residuals_to_clipboard)
        self.tree_res.bind("<Command-c>", self.copy_residuals_to_clipboard)
        self.tree_res.bind("<Control-C>", self.copy_residuals_to_clipboard)
        self.tree_res.bind("<Command-C>", self.copy_residuals_to_clipboard)
        
        # Status Bar
        self.status_bar = ctk.CTkLabel(self, text="  Ready", anchor="w", 
                                       font=UI_FONT_SMALL, height=25, 
                                       fg_color=color_status_OK, text_color="white",
                                       corner_radius=0)
        self.status_bar.grid(row=2, column=0, sticky="ew")
        
        # Shortcuts
        self.bind_all("<Control-n>", self.new_file)
        self.bind_all("<Command-n>", self.new_file)
        self.bind_all("<Control-o>", self.open_file)
        self.bind_all("<Command-o>", self.open_file)
        self.bind_all("<Control-s>", self.save_file)
        self.bind_all("<Command-s>", self.save_file)
        self.bind_all("<Control-Shift-s>", self.save_file_as)
        self.bind_all("<Command-Shift-s>", self.save_file_as)
        self.bind_all("<Control-Shift-S>", self.save_file_as)
        self.bind_all("<Command-Shift-S>", self.save_file_as)
        self.bind_all("<Control-r>", lambda e: self.run_solve())
        self.bind_all("<Command-r>", lambda e: self.run_solve())

    def copy_solution_to_clipboard(self, event=None):
        """Copies selected solution rows to clipboard."""
        selection = self.tree.selection()
        if not selection: return
        
        lines = []
        for item_id in selection:
            vals = self.tree.item(item_id, "values")
            # Format: Name = Value [Unit]
            lines.append(f"{vals[0]} = {vals[1]} [{vals[2]}]")
        
        if lines:
            self.clipboard_clear()
            self.clipboard_append("\n".join(lines))
            self.status_bar.configure(text=f"  Copied {len(lines)} variables to clipboard", fg_color=color_status_Success)

    def copy_residuals_to_clipboard(self, event=None):
        """Copies selected residual rows to clipboard."""
        selection = self.tree_res.selection()
        if not selection: return
        
        lines = []
        for item_id in selection:
            vals = self.tree_res.item(item_id, "values")
            # Format: ID: EQN = Value
            lines.append(f"{vals[0]}: {vals[1]} = {vals[2]}")
        
        if lines:
            self.clipboard_clear()
            self.clipboard_append("\n".join(lines))
            self.status_bar.configure(text=f"  Copied {len(lines)} residuals to clipboard", fg_color=color_status_Success)

    def _on_table_select(self, event):
        """Highlights the selected variable in the code editor."""
        selected = self.tree.selection()
        if not selected: return
        
        item = self.tree.item(selected[0])
        var_name = item['values'][0] # Variable name is first col
        self.editor.highlight_symbol(var_name)

    def _on_residual_select(self, event):
        """Highlights the corresponding line in the code editor."""
        selected = self.tree_res.selection()
        if not selected: return

        line = self.tree_res.item(selected[0])["values"][3]
        self.editor.highlight_line(int(line))

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
        
        # Create Treeview inside the tab: RESULTS
        self.tree = ttk.Treeview(self.tab_solution, columns=("Name", "Value", "Unit", "State", "LINE"), show="headings", style="Treeview")
        self.tree["displaycolumns"] = ("Name", "Value", "Unit", "State")
        self.tree.heading("Name", text="NAME")
        self.tree.heading("Value", text="VALUE")
        self.tree.heading("Unit", text="UNIT")
        self.tree.heading("State", text="STATE")
        
        self.tree.column("Name", width=90, anchor="w")
        self.tree.column("Value", width=100, anchor="e")
        self.tree.column("Unit", width=60, anchor="center")
        self.tree.column("State", width=50, anchor="center")
        
        self.tree.pack(fill="both", expand=True)
        
        # Create Treeview inside the tab: RESIDUALS
        self.tree_res = ttk.Treeview(self.tab_residuals, columns=("ID", "EQN", "Value", "LINE"), show="headings", style="Treeview")
        self.tree_res["displaycolumns"] = ("ID", "EQN", "Value")
        self.tree_res.heading("ID", text="ID")
        self.tree_res.heading("EQN", text="Equation")
        self.tree_res.heading("Value", text="Value")
        
        self.tree_res.column("ID", width=30, anchor="w")
        self.tree_res.column("EQN", width=100)
        self.tree_res.column("Value", width=50, anchor="e")
        
        self.tree_res.pack(fill="both", expand=True)

    def run_lint(self):
        """Debounced linting trigger."""
        if self._lint_pending:
            self.after_cancel(self._lint_pending)
        self._lint_pending = self.after(300, self._start_lint_thread)

    def _start_lint_thread(self):
        """Starts the linting thread if not already running."""
        if self._linting_in_progress:
            # Re-schedule if already running
            self._lint_pending = self.after(300, self._start_lint_thread)
            return
            
        self._linting_in_progress = True
        threading.Thread(target=self._lint_thread, daemon=True).start()

    def _lint_thread(self):
        try:
            content = self.editor.get_text()
            cwd = os.path.dirname(self.current_file) if self.current_file else os.getcwd()
            result = self.backend.lint(content, cwd=cwd)
            self.after(0, lambda: self._handle_lint_result(result))
        except Exception:
            self._linting_in_progress = False

    def _handle_lint_result(self, result):
        self._linting_in_progress = False
        if not result.get("success"):
            line = result.get("error_line", 0)
            self.editor.highlight_error(line)
            self.status_bar.configure(text=f"  Linter: {result.get('error')}", fg_color=color_status_Bad)
        else:
            self.editor.highlight_error(0)
            self.status_bar.configure(text="  Ready", fg_color=color_status_Success)

    def new_file(self, event=None):        
        self.editor.set_text("")
        self.current_file = None
        self.status_bar.configure(text="   Good luck!", fg_color=color_status_OK)
        self._set_title(None)
        
    def _scrape(self, path:pathlib.Path, visited:set[pathlib.Path]) -> set[Documentation]:
        if path in visited: return set() # Already visited, disregard
        visited.add(path) # Account
        
        # read actual path
        with open(path, 'r') as f:
            editor_text = f.read()
        result = set(Documentation.from_text_block(editor_text))
            
        # gobble up
        name_checks = get_all_includes(editor_text)
        path_checks = [pathlib.Path(self.backend.exe).parent / 'libs']
        if self.current_file:
            path_checks.append(pathlib.Path(self.current_file).parent)
            
        for j, name_check in enumerate(name_checks):
            resolved = resolve_name(name_check, path_checks)
            # print(f"{j}: Scan {name_check} ({resolved}) for functions")
            
            if resolved: 
                # Use .update() for efficient in-place set merging
                result.update(self._scrape(resolved, visited))
            
        return result
        
    def load_metadata(self):
        # Check native text
        editor_text = self.editor.get_text()
        for i, metadata in enumerate(Documentation.from_text_block(editor_text)):
            # print(f"{i}: Add {metadata.to_dict()} to functions")
            self.metadata['functions'].update(metadata.to_dict())
            
        # Recursive descent
        name_checks = get_all_includes(editor_text)
        
        # Safely construct path_checks without adding None
        path_checks = [pathlib.Path(self.backend.exe).parent / 'libs']
        if self.current_file:
            path_checks.append(pathlib.Path(self.current_file).parent)
            
        visited = set()
        for j, name_check in enumerate(name_checks):
            resolved = resolve_name(name_check, path_checks)
            # print(f"{j}: Scan {name_check} ({resolved}) for functions")
            
            if resolved:
                # Capture the returned data and merge it into metadata
                scraped_docs = self._scrape(resolved, visited)
                for doc in scraped_docs:
                    self.metadata['functions'].update(doc.to_dict())

    def open_file(self, event=None):
        """Dynamic open supporting both .cnes and .cnesp project bundles."""
        path = filedialog.askopenfilename(filetypes=[
            ("All CoNES Formats", "*.cnes *.cnesp"),
            ("CoNES Project", "*.cnesp"),
            ("CoNES Script", "*.cnes"),
            ("All Files", "*.*")
        ])
        if path:
            self.load_file(path)

    def load_file(self, path: str):
        """Loads data from disk, handling both raw scripts and project bundles."""
        try:
            if path.endswith(".cnesp"):
                with open(path, "r") as f:
                    project_data = json.load(f)
                
                self.editor.set_text(project_data.get("code", ""))
                if "parametric_studies" in project_data:
                    self.parametric_pane.from_dict(project_data["parametric_studies"])
                
                msg = f"  Loaded project: {os.path.basename(path)}"
            else:
                with open(path, "r") as f:
                    self.editor.set_text(f.read())
                msg = f"  Opened script: {os.path.basename(path)}"

            self.current_file = path
            self.status_bar.configure(text=msg, fg_color=COLOR_STATUS_OK)
            self._set_title(pathlib.Path(path).name)
            self.load_metadata()
            self._clear_trees()
        except Exception as e:
            self.status_bar.configure(text=f"  Failed to load: {str(e)}", fg_color=COLOR_STATUS_BAD)

    def save_file(self, event=None):
        """Performs a standard save. Prompts for path if none exists."""
        if not self.current_file:
            self.save_file_as()
        else:
            self._execute_save(self.current_file)

    def save_file_as(self, event=None):
        """Prompts the user for a new file path and format."""
        path = filedialog.asksaveasfilename(
            defaultextension=".cnesp",
            filetypes=[
                ("CoNES Project", "*.cnesp"),
                ("CoNES Script", "*.cnes")
            ]
        )
        if path:
            self.current_file = path
            self._execute_save(path)

    def _execute_save(self, path: str):
        """Internal save logic that handles formatting based on extension."""
        try:
            if path.endswith(".cnesp"):
                project_data = {
                    "version": "1.0",
                    "code": self.editor.get_text(),
                    "parametric_studies": self.parametric_pane.to_dict()
                }
                with open(path, "w") as f:
                    json.dump(project_data, f, indent=2)
                msg = f"  Project saved: {os.path.basename(path)}"
            else:
                with open(path, "w") as f:
                    f.write(self.editor.get_text())
                msg = f"  Script saved: {os.path.basename(path)}"

            self.status_bar.configure(text=msg, fg_color=COLOR_STATUS_SUCCESS)
            self._set_title(pathlib.Path(path).name)
            self.load_metadata()
        except Exception as e:
            self.status_bar.configure(text=f"  Save failed: {str(e)}", fg_color=COLOR_STATUS_BAD)
    
    def save_lib(self):
        # Determine the libs directory relative to project root
        base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        libs_dir = os.path.join(base_dir, "libs")
        if not os.path.exists(libs_dir):
            os.makedirs(libs_dir, exist_ok=True)

        dialog = CustomInputDialog(text="Enter library name (e.g. 'my_lib'):", 
                                   title="Export Library", 
                                   initial_value=self.last_lib_name.rstrip('.cnes'))
        lib_name = dialog.get_input()
        
        if lib_name:
            if not lib_name.endswith(".cnes"):
                lib_name += ".cnes"
            
            save_path = os.path.join(libs_dir, lib_name)
            try:
                with open(save_path, "w") as f:
                    f.write(self.editor.get_text())
                
                self.last_lib_name = lib_name
                self.status_bar.configure(text=f"  Library saved to: libs/{lib_name}", fg_color=COLOR_STATUS_SUCCESS)
                self._set_title(lib_name+" (Library)")
            except Exception as e:
                self.status_bar.configure(text=f"  Failed to save library: {str(e)}", fg_color=COLOR_STATUS_BAD)

    def run_solve(self):
        content = self.editor.get_text()
        with open(self.shadow_path, "w") as f:
            f.write(content)
        
        self.status_bar.configure(text="  Solving...", fg_color=color_status_OK)
        
        # Pass the directory of the current file to resolve relative imports
        cwd = os.path.dirname(self.current_file) if self.current_file else os.getcwd()
        threading.Thread(target=self._solve_thread, args=(cwd,), daemon=True).start()

    def _solve_thread(self, cwd=None):
        result = self.backend.solve(self.shadow_path, cwd=cwd)
        self.after(0, lambda: self._handle_solve_result(result))

    def _clear_trees(self):
        for item in self.tree.get_children(): self.tree.delete(item)
        for item in self.tree_res.get_children(): self.tree_res.delete(item)
        
    def _handle_solve_result(self, result):
        solved = result.get("success")
        
        self._clear_trees()
        for var in result.get("variables", []):
            state = "FIX" if var["is_fixed"] else "SOL"
            self.tree.insert("", "end", values=(var["name"], f"{var['value']:.6}", var["unit"], state, var["line"]))
            
        for var in result.get("residuals", []):
            self.tree_res.insert("", "end", values=(var["id"], var["expression"], f"{var['value']:.5g}", var["line"]))
        
        perf = result.get("performance", {})
        if solved:
            self.status_bar.configure(text=f"  {'Success' if result.get('success') else 'Diverged'} ({parse_time(perf.get('solver_ms', 0))}, {perf.get('iterations', 0)} total iterations, {perf.get('total_residuals', 1):.4g} total residuals)")
            if result.get('success'): self.status_bar.configure(fg_color=color_status_Success)
            else: self.status_bar.configure(fg_color=color_status_Diverged) # This lowkey will never happen BUT its future-proofed!
        else:
            self.status_bar.configure(text=f"  Error: {result.get('error')[:100]} ({parse_time(perf.get('solver_ms', 0))}, {perf.get('iterations', 0)} total iterations, {perf.get('total_residuals', 1):.4g} total residuals)", fg_color=color_status_Bad)

if __name__ == "__main__":
    
    # Instantiate
    app = CoNESStudio()
    
    # Load the path if it exists AND it ends in .cnes
    app.mainloop()
    if len(sys.argv) > 1 and is_cnes(sys.argv[1]): app.load_file(sys.argv[1])
