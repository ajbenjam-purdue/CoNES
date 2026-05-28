import customtkinter as ctk
import tkinter as tk
from tkinter import ttk
import threading
import os
import tempfile
from colors import *

# Utils
def has_brackets(x:str) -> bool: return "[" in x and "]" in x

def parse_user_header(x:str) -> tuple[str, str]:
    split = x.split()
    if len(split) == 1: # one term
        return (split[0].replace(".", ""), "")
    elif len(split) == 2: # 2 terms
        return (split[0].replace(".", ""), split[1].replace(".", "")) if has_brackets(split[1]) else ("_".join(split).replace(".", ""), "")
    elif len(split) > 2:
        name = []
        for term in split:
            if has_brackets(term):
                return ("_".join(name), term)
            name.append(term.replace(".", ""))
    return ("", "")

# Floating tooltip
class ToolTip:
    """Floating tooltip for error messages and detailed residuals"""
    def __init__(self, widget):
        self.widget = widget
        self.tip_window = None

    def show_tip(self, text, x, y):
        if self.tip_window or not text: return
        self.tip_window = tw = tk.Toplevel(self.widget)
        tw.wm_overrideredirect(True)
        
        tw.wm_geometry(f"+{x+15}+{y+10}") # Position slightly offset from cursor
        
        # Styled to match the dark theme with red text for errors
        label = tk.Label(tw, text=text, justify=tk.LEFT,
                         background="#2d2d2d", foreground="#ff5555", relief=tk.SOLID, borderwidth=1,
                         font=MONOSPACED_FONT_SMALL, padx=10, pady=8)
        label.pack()

    def hide_tip(self):
        tw = self.tip_window
        self.tip_window = None
        if tw: tw.destroy()

# Treeview low-tier wrapper
class EditableTreeview(ttk.Treeview):
    """Treeview that allows double-clicking to edit cells."""
    def __init__(self, master, on_edit_callback=None, **kwargs):
        super().__init__(master, **kwargs)
        self.bind("<Double-1>", self.on_double_click)
        self.entry = None
        self.on_edit_callback = on_edit_callback

    def on_double_click(self, event):
        region = self.identify_region(event.x, event.y)
        if region != "cell": return
        
        column = self.identify_column(event.x)
        item = self.identify_row(event.y)
        
        # Column #1 is "Run" (index-based naming like #1, #2...)
        if column == '#1': return 
        
        x, y, width, height = self.bbox(item, column)
        col_idx = int(column[1:]) - 1
        value = self.item(item, 'values')[col_idx]
        
        # Create an entry overlay for editing
        self.entry = tk.Entry(self, bg="#252526", fg="#ffffff", insertbackground="#cccccc", 
                              font=MONOSPACED_FONT, borderwidth=1, relief="solid")
        self.entry.place(x=x, y=y, width=width, height=height)
        self.entry.insert(0, value)
        self.entry.focus_set()
        self.entry.select_range(0, tk.END)
        
        def save_edit(event=None):
            if not self.entry: return
            new_val = self.entry.get()
            values = list(self.item(item, 'values'))
            values[col_idx] = new_val
            self.item(item, values=values)
            
            col_name = self["columns"][col_idx]
            if self.on_edit_callback:
                self.on_edit_callback(item, col_name, new_val)
                
            self.entry.destroy()
            self.entry = None
            
        def cancel_edit(event=None):
            if not self.entry: return
            self.entry.destroy()
            self.entry = None
            
        self.entry.bind("<Return>", save_edit)
        self.entry.bind("<FocusOut>", save_edit)
        self.entry.bind("<Escape>", cancel_edit)

# Table mid-tier wrapper
class ParametricTable(ctk.CTkFrame):
    def __init__(self, master, app_ref, **kwargs):
        super().__init__(master, corner_radius=0, fg_color="transparent", **kwargs)
        self.app = app_ref
        
        # Maps item_id -> set of column names that are user-set, Maps item_id -> error result dictionary
        self.inputs_map = {}
        self.error_map = {}
        self.shadow_path = os.path.join(tempfile.gettempdir(), f"cones_parametric_{id(self)}.cnes")
        
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)
        
        # Toolbar
        self.toolbar = ctk.CTkFrame(self, height=35, corner_radius=0, fg_color="transparent")
        self.toolbar.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 5))
        
        # Square/Flush button styling to match main UI
        self.btn_add_col = ctk.CTkButton(self.toolbar, text="Add Column", command=self.add_column, **btn_opts)
        self.btn_add_col.pack(side="left")
        
        self.btn_add_row = ctk.CTkButton(self.toolbar, text="Add Row", command=self.add_row, **btn_opts)
        self.btn_add_row.pack(side="left")
        
        self.btn_clear = ctk.CTkButton(self.toolbar, text="Clear", command=self.clear_outputs, **btn_opts)
        self.btn_clear.pack(side="left")

        # Solver Run button with accent color
        run_opts = btn_opts.copy()
        run_opts.update({"fg_color": color_UI, "hover_color": color_UI_hover, "text_color": "white", "width": 80})
        self.btn_run = ctk.CTkButton(self.toolbar, text="Run Table", command=self.run_table, **run_opts)
        self.btn_run.pack(side="right")
        
        # Treeview with Scrollbars. Use a container to manage scrollbar placement.
        self.tree_container = ctk.CTkFrame(self, corner_radius=0, fg_color="transparent")
        self.tree_container.grid(row=1, column=0, sticky="nsew")
        
        # Use place manager for an inner frame to decouple the treeview's requested size from the parent grid, completely preventing the sidebar from expanding.
        self.inner_frame = ctk.CTkFrame(self.tree_container, corner_radius=0, fg_color="transparent")
        self.inner_frame.place(relx=0, rely=0, relwidth=1, relheight=1)
        self.inner_frame.grid_rowconfigure(0, weight=1)
        self.inner_frame.grid_columnconfigure(0, weight=1)

        self.tree = EditableTreeview(self.inner_frame, on_edit_callback=self.on_cell_edited, 
                                     columns=("Run",), show="headings", style="Treeview")
        self.tree.heading("Run", text="Run")
        self.tree.column("Run", width=50, minwidth=50, anchor="center", stretch=False)
        self.tree.grid(row=0, column=0, sticky="nsew")
        
        # Fixed width scrollbars attached to the inner frame
        self.v_scroll = ctk.CTkScrollbar(self.inner_frame, orientation="vertical", command=self.tree.yview)
        self.v_scroll.grid(row=0, column=1, sticky="ns")
        
        self.h_scroll = ctk.CTkScrollbar(self.inner_frame, orientation="horizontal", command=self.tree.xview)
        self.h_scroll.grid(row=1, column=0, sticky="ew")
        
        self.tree.configure(yscrollcommand=self.v_scroll.set, xscrollcommand=self.h_scroll.set)
        
        # Context Menus for Rows/Columns
        self.row_menu = tk.Menu(self, tearoff=0, bg="#2d2d2d", fg="#cccccc", activebackground="#3e3e3e")
        self.row_menu.add_command(label="Insert Row Above", command=self.insert_row_above)
        self.row_menu.add_command(label="Duplicate Row", command=self.duplicate_row)
        self.row_menu.add_separator()
        self.row_menu.add_command(label="Delete Row", command=self.delete_row)

        self.col_menu = tk.Menu(self, tearoff=0, bg="#2d2d2d", fg="#cccccc", activebackground="#3e3e3e")
        self.col_menu.add_command(label="Rename Column", command=self.rename_column)
        self.col_menu.add_command(label="Delete Column", command=self.delete_column)

        # Right click bindings
        self.tree.bind("<Button-3>", self.on_right_click)
        self.tree.bind("<Button-2>", self.on_right_click) # macOS
        
        # Tooltip for detailed error reporting
        self.tooltip = ToolTip(self.tree)
        self.tree.bind("<Motion>", self.on_mouse_move)
        
        # Styling tags for treeview rows
        self.tree.tag_configure("calculated", foreground="#888888")
        self.tree.tag_configure("error", foreground="#ff5555")

        # Start with 5 rows
        self.headers = ["Run"]
        self.run_counter = 0
        for _ in range(5): self.add_row()

    def copy_from(self, other):
        """Deep copy state from another ParametricTable instance"""
        # Clear default rows created in __init__
        for item in self.tree.get_children():
            self.tree.delete(item)
            
        self.headers = other.headers.copy()
        self.run_counter = other.run_counter
        
        # Configure columns
        self.tree.configure(columns=self.headers)
        for h in self.headers:
            self.tree.heading(h, text=h)
            if h == "Run":
                self.tree.column(h, width=50, minwidth=50, anchor="center", stretch=False)
            else:
                self.tree.column(h, width=100, minwidth=70, anchor="center", stretch=True)
                
        # Copy rows and re-map IDs for inputs/errors
        item_map = {}
        for old_item in other.tree.get_children():
            vals = other.tree.item(old_item, "values")
            tags = other.tree.item(old_item, "tags")
            new_item = self.tree.insert("", "end", values=vals, tags=tags)
            item_map[old_item] = new_item
            
        # Re-map inputs and errors
        self.inputs_map = {item_map[old_id]: other.inputs_map[old_id].copy() 
                           for old_id in other.inputs_map if old_id in item_map}
        self.error_map = {item_map[old_id]: other.error_map[old_id].copy() 
                          for old_id in other.error_map if old_id in item_map}

    def on_cell_edited(self, item_id, col_name, new_val):
        """Track which cells are user-set to avoid clearing them"""
        if new_val.strip() == "":
            if item_id in self.inputs_map and col_name in self.inputs_map[item_id]:
                self.inputs_map[item_id].remove(col_name)
        else:
            if item_id not in self.inputs_map:
                self.inputs_map[item_id] = set()
            self.inputs_map[item_id].add(col_name)
        
        # Reset error state on edit
        tags = list(self.tree.item(item_id, "tags"))
        if "error" in tags:
            tags.remove("error")
            self.tree.item(item_id, tags=tags)

    def on_right_click(self, event):
        """Show context menus based on clicked region"""
        region = self.tree.identify_region(event.x, event.y)
        if region == "heading":
            self.clicked_col = self.tree.identify_column(event.x)
            self.col_menu.post(event.x_root, event.y_root)
        elif region == "cell":
            item = self.tree.identify_row(event.y)
            self.tree.selection_set(item)
            self.clicked_row = item
            self.row_menu.post(event.x_root, event.y_root)

    def on_mouse_move(self, event):
        """Display detailed tooltips iff hovering over an ERROR cell"""
        item = self.tree.identify_row(event.y)
        column = self.tree.identify_column(event.x)
        
        if item in self.error_map:
            col_idx = int(column[1:]) - 1
            vals = self.tree.item(item, "values")
            # If the specific cell hovered is the error indicator
            if col_idx < len(vals) and vals[col_idx] == "ERROR":
                err_data = self.error_map[item]
                msg = f"ERROR: {err_data.get('error', 'Newton solver failed to converge.')}\n"
                msg += "—" * 35 + "\n"
                msg += "Worst Performing Residuals:\n"
                
                # Fetch residuals and sort by absolute error
                residuals = err_data.get("residuals", [])
                sorted_res = sorted(residuals, key=lambda x: abs(x.get("value", 0)), reverse=True)
                
                for res in sorted_res[:6]:
                    expr = res.get("expression", "unknown")
                    # Truncate long expressions
                    if len(expr) > 28: expr = expr[:25] + "..."
                    val = res.get("value", 0)
                    msg += f" {res.get('id', '?')}: {expr:<28} | {val:.4e}\n"
                
                self.tooltip.show_tip(msg.strip(), event.x_root, event.y_root)
                return
        
        self.tooltip.hide_tip()

    def add_column(self):
        """Prompt user for a new variable to track."""
        dialog = ctk.CTkInputDialog(text="Enter variable name (e.g. T [K]):", title="Add Column")
        var_name = dialog.get_input()
        if not var_name: return
        var_name = var_name.strip()
        
        if var_name in self.headers: return
        self._add_column(var_name)

    def _add_column(self, name):
        self.headers.append(name)
        self.tree.configure(columns=self.headers)
        for h in self.headers:
            self.tree.heading(h, text=h)
            if h == "Run":
                self.tree.column(h, width=50, minwidth=50, anchor="center", stretch=False)
            else:
                self.tree.column(h, width=100, minwidth=70, anchor="center", stretch=True)
        
        # Populate new empty cells for existing rows
        for item in self.tree.get_children():
            vals = list(self.tree.item(item, "values"))
            vals.append("")
            self.tree.item(item, values=vals)

    def rename_column(self):
        """Rename right clicked column"""
        col_idx = int(self.clicked_col[1:]) - 1
        if col_idx == 0: return # Locked
        
        old_name = self.headers[col_idx]
        dialog = ctk.CTkInputDialog(text=f"Rename '{old_name}' to:", title="Rename Column")
        new_name = dialog.get_input()
        if not new_name or new_name.strip() == "": return
        new_name = new_name.strip()
        
        self.headers[col_idx] = new_name
        self.tree.heading(self.clicked_col, text=new_name)
        
        # Migrate input maps
        for item_id in self.inputs_map:
            if old_name in self.inputs_map[item_id]:
                self.inputs_map[item_id].remove(old_name)
                self.inputs_map[item_id].add(new_name)

    def delete_column(self):
        """Delete right clicked column"""
        col_idx = int(self.clicked_col[1:]) - 1
        if col_idx == 0: return # Locked
        
        name = self.headers.pop(col_idx)
        self.tree.configure(columns=self.headers)
        for h in self.headers:
            self.tree.heading(h, text=h)
            
        for item in self.tree.get_children():
            vals = list(self.tree.item(item, "values"))
            vals.pop(col_idx)
            self.tree.item(item, values=vals)
            if item in self.inputs_map and name in self.inputs_map[item]:
                self.inputs_map[item].remove(name)

    def add_row(self):
        self.run_counter += 1
        vals = [self.run_counter] + [""] * (len(self.headers) - 1)
        self.tree.insert("", "end", values=vals)

    def insert_row_above(self):
        index = self.tree.index(self.clicked_row)
        self.run_counter += 1
        vals = [self.run_counter] + [""] * (len(self.headers) - 1)
        self.tree.insert("", index, values=vals)

    def duplicate_row(self):
        index = self.tree.index(self.clicked_row)
        old_vals = list(self.tree.item(self.clicked_row, "values"))
        self.run_counter += 1
        new_vals = [self.run_counter] + old_vals[1:]
        new_item = self.tree.insert("", index + 1, values=new_vals)
        
        if self.clicked_row in self.inputs_map:
            self.inputs_map[new_item] = self.inputs_map[self.clicked_row].copy()

    def delete_row(self):
        item = self.clicked_row
        self.tree.delete(item)
        if item in self.inputs_map: del self.inputs_map[item]
        if item in self.error_map: del self.error_map[item]

    def clear_outputs(self):
        """Clears calculated values while keeping user-defined inputs."""
        for item in self.tree.get_children():
            vals = list(self.tree.item(item, "values"))
            new_vals = [vals[0]]
            user_inputs = self.inputs_map.get(item, set())
            for i, col in enumerate(self.headers[1:]):
                if col in user_inputs:
                    new_vals.append(vals[i+1])
                else:
                    new_vals.append("")
            self.tree.item(item, values=new_vals, tags=())
            if item in self.error_map: del self.error_map[item]

    def run_table(self):
        if len(self.headers) <= 1: return
        self.btn_run.configure(state="disabled", text="Running...")
        threading.Thread(target=self._run_loop, daemon=True).start()
        
    def _run_loop(self):
        """Background thread for executing parametric studies"""
        base_script = self.app.editor.get_text()
        cwd = os.path.dirname(self.app.current_file) if self.app.current_file else os.getcwd()
        
        for item in self.tree.get_children():
            vals = list(self.tree.item(item, "values"))
            overrides = ""
            user_inputs = self.inputs_map.get(item, set())
            
            # Construct overrides for this run
            for i, col in enumerate(self.headers[1:]):
                if col in user_inputs:
                    val_str = parse_user_header(col)
                    val = str(vals[i+1]).strip()
                    if val:
                        overrides += f"{val_str[0]} := {val} {val_str[1]}\n"
            
            full_script = "// --- FROM PARAMETRIC TABLE ---\n" + overrides + "\n// --- BASE SCRIPT ---\n" + base_script
            
            with open(self.shadow_path, "w") as f:
                f.write(full_script)
                
            result = self.app.backend.solve(self.shadow_path, cwd=cwd)
            
            if result.get("success"):
                solved_vars = {v["name"]: v["value"] for v in result["variables"]}
                new_vals = [vals[0]]
                for i, col in enumerate(self.headers[1:]):
                    if col in user_inputs:
                        new_vals.append(vals[i+1])
                    else:
                        if col in solved_vars:
                            new_vals.append(f"{solved_vars[col]:.5g}")
                        else:
                            new_vals.append("-")
                # Apply 'calculated' tag
                self.app.after(0, lambda it=item, nv=new_vals: self.tree.item(it, values=nv, tags=("calculated",)))
                if item in self.error_map: del self.error_map[item]
            else:
                new_vals = [vals[0]]
                for i, col in enumerate(self.headers[1:]):
                    if col in user_inputs:
                        new_vals.append(vals[i+1])
                    else:
                        new_vals.append("ERROR")
                
                # Store full result for rich tooltip data
                self.error_map[item] = result
                self.app.after(0, lambda it=item, nv=new_vals: self.tree.item(it, values=nv, tags=("error",)))
                
        self.app.after(0, lambda: self.btn_run.configure(state="normal", text="Run Table"))

# Pane high-tier wrapper
class ParametricPane(ctk.CTkFrame):
    def __init__(self, master, app_ref, **kwargs):
        super().__init__(master, corner_radius=0, fg_color="transparent", **kwargs)
        self.app = app_ref
        
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)
        
        self.toolbar = ctk.CTkFrame(self, height=35, corner_radius=0, fg_color="transparent")
        self.toolbar.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        
        # Tool buttons
        self.btn_new_table = ctk.CTkButton(self.toolbar, text="New Table", command=self.new_table, **btn_opts)
        self.btn_new_table.pack(side="left")
        
        self.btn_rename_table = ctk.CTkButton(self.toolbar, text="Rename Table", command=self.rename_table, **btn_opts)
        self.btn_rename_table.pack(side="left")
        
        kill_opts = btn_opts.copy()
        kill_opts.update({"hover_color": color_status_Bad})
        self.btn_kill_table = ctk.CTkButton(self.toolbar, text="Delete Table", command=self.kill_table, **kill_opts)
        self.btn_kill_table.pack(side="left")
        
        self.tabs = ctk.CTkTabview(self, corner_radius=0, fg_color="#252526",
                                   segmented_button_selected_color="#1e1e1e",
                                   segmented_button_fg_color="#2d2d2d",
                                   segmented_button_unselected_color="#2d2d2d",
                                   text_color="#888888")
        self.tabs.grid(row=1, column=0, sticky="nsew")
        
        self.tables:dict[str, ParametricTable] = {}
        self.table_counter = 0

        self.new_table()

    def new_table(self):
        """Creates a new parametric table tab"""
        self.table_counter += 1
        name = f"Table {self.table_counter}"
        # TODO: Check for already existing
        self.tabs.add(name)
        tab_frame = self.tabs.tab(name)
        
        pt = ParametricTable(tab_frame, self.app)
        pt.pack(fill="both", expand=True)
        self.tables[name] = pt
        self.tabs.set(name)

    def rename_table(self):
        """Renames the current parametric table"""
        
        # Get new name
        old_name = self.tabs.get()
        if not old_name: return
        
        dialog = ctk.CTkInputDialog(text=f"Rename '{old_name}' to:", title="Rename Table")
        new_name = dialog.get_input()
        if not new_name or new_name.strip() == "": return
        new_name = new_name.strip()
        
        if new_name in self.tables: return
        
        # Create a new tab and frame
        self.tabs.add(new_name)
        tab_frame = self.tabs.tab(new_name)
        
        old_pt = self.tables[old_name]
        new_pt = ParametricTable(tab_frame, self.app)
        new_pt.pack(fill="both", expand=True)
        
        # Copy data
        new_pt.copy_from(old_pt)
        
        self.tables[new_name] = new_pt
        self.tabs.set(new_name)
        
        # Cleanup
        self.tabs.delete(old_name)
        del self.tables[old_name]
    
    def kill_table(self):
        """Deletes the currently selected table tab"""
        name = self.tabs.get()
        if not name: return
        
        if name in self.tables:
            self.tabs.delete(name)
            del self.tables[name]
            
            # Auto-reset if no tables left
            if not self.tables:
                self.table_counter = 0
                self.new_table()
            else:
                remaining = list(self.tables.keys())
                self.tabs.set(remaining[-1])
