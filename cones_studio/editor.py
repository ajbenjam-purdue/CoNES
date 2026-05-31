import tkinter as tk
from tkinter import ttk
import customtkinter as ctk
import re
from typing import Callable, Optional, Dict, Any
from colors import *

class CodeEditor(ctk.CTkFrame):
    def __init__(self, master, metadata=None, **kwargs):
        super().__init__(master, **kwargs)
        self.metadata = metadata or {"constants": {}, "functions": {}, "substances": {}}
        
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(1, weight=1)
        
        # Line Numbers (Stable gutter)
        self.line_numbers = tk.Canvas(self, width=35, bg="#181818", highlightthickness=0)
        self.line_numbers.grid(row=0, column=0, sticky="ns")
        
        # Text Area (With Word Wrap)
        self.text_area = tk.Text(self, 
                                 undo=True,
                                 maxundo=100, 
                                 wrap="word", 
                                 bg="#1e1e1e", 
                                 fg="#d4d4d4", 
                                 insertbackground="#007acc",
                                 font=MONOSPACED_FONT,
                                 highlightthickness=0,
                                 padx=5, pady=2,
                                 borderwidth=0,
                                 spacing1=1, # Extra line spacing for readability
                                 autoseparators=True)
        self.text_area.grid(row=0, column=1, sticky="nsew")
        
        # Scrollbars
        self.v_scroll = ctk.CTkScrollbar(self, command=self._on_vscroll, width=12)
        self.v_scroll.grid(row=0, column=2, sticky="ns")
        self.text_area.configure(yscrollcommand=self.v_scroll.set)
        
        # State
        self.tooltip: Optional[tk.Toplevel] = None
        self._lint_timer: Optional[str] = None
        self.on_content_changed_callback: Optional[Callable[[], None]] = None
        self.current_ghost = ""
        
        # Robust Sync Bindings
        self.text_area.bind("<KeyRelease>", self._on_key_release)
        self.text_area.bind("<KeyPress>", self._on_key_press)
        self.text_area.bind("<Button-1>", self._on_click)
        self.text_area.bind("<Motion>", self._on_mouse_move)
        self.text_area.bind("<Configure>", lambda e: self.after(1, self._update_line_numbers))
        self.text_area.bind("<MouseWheel>", lambda e: self.after(1, self._update_line_numbers))
        
        self._setup_shortcuts()
        self._setup_tags()

    def _setup_tags(self):
        # Professional Engineering Theme
        self.text_area.tag_configure("keyword", foreground="#569cd6") 
        self.text_area.tag_configure("function", foreground="#dcdcaa") 
        self.text_area.tag_configure("substance", foreground="#4ec9b0", font=MONOSPACED_FONT_BOLD) 
        self.text_area.tag_configure("string", foreground="#ce9178") 
        self.text_area.tag_configure("number", foreground="#b5cea8") 
        self.text_area.tag_configure("unit", foreground="#b5cea8") 
        self.text_area.tag_configure("error", underline=True, underlinefg="#f44747")
        self.text_area.tag_configure("ghost", foreground="#505050")
        self.text_area.tag_configure("symbol_highlight", background="#485748")
        self.text_area.tag_configure("comment", foreground="#6a9955") 

    def _setup_shortcuts(self):
        self.text_area.bind("<Control-Left>", lambda e: self._jump_left(shift=False))
        self.text_area.bind("<Control-Right>", lambda e: self._jump_right(shift=False))
        self.text_area.bind("<Control-Shift-Left>", lambda e: self._jump_left(shift=True))
        self.text_area.bind("<Control-Shift-Right>", lambda e: self._jump_right(shift=True))
        self.text_area.bind("<Control-BackSpace>", self._delete_left)
        self.text_area.bind("<Control-Delete>", self._delete_right)
        self.text_area.bind("<Tab>", self._accept_ghost)
        self.text_area.bind("<Control-a>", lambda e: (self.text_area.tag_add("sel", "1.0", "end"), "break"))

    def highlight_error(self, line):
        self.text_area.tag_remove("error", "1.0", "end")
        if line > 0:
            start = f"{line}.0"
            end = f"{line}.end"
            self.text_area.tag_add("error", start, end)
            self.text_area.see(start)

    def highlight_line(self, line):
        self.text_area.tag_remove("symbol_highlight", "1.0", "end")
        if line > 0:
            start = f"{line}.0"
            end = f"{line}.end"
            self.text_area.tag_add("symbol_highlight", start, end)
            self.text_area.see(start)

    def highlight_symbol(self, name):
        self.text_area.tag_remove("symbol_highlight", "1.0", "end")
        if not name: return
        content = self.get_text()
        pattern = r"\b" + re.escape(name) + r"\b"
        for m in re.finditer(pattern, content):
            start = f"1.0 + {m.start()}c"
            end = f"1.0 + {m.end()}c"
            self.text_area.tag_add("symbol_highlight", start, end)

    def _on_mouse_move(self, event):
        index = self.text_area.index(f"@{event.x},{event.y}")
        word = self._get_word_at_index(index)
        
        if word in self.metadata["functions"]:
            meta = self.metadata["functions"][word]
            rich_text = f"{meta['sig']}\n{'-'*len(meta['sig'])}\n{meta['desc'].replace(';', '\n')}"
            self._show_tooltip(rich_text, event.x_root, event.y_root)
        elif word in self.metadata["constants"]:
            meta = self.metadata["constants"][word]
            val_str = f" = {meta['value']}" if meta['value'] else ""
            unit_str = f" [{meta['unit']}]" if meta['unit'] else ""
            rich_text = f"Constant: {word}{val_str}{unit_str}\n{'-'*20}\n{meta['desc']}"
            self._show_tooltip(rich_text, event.x_root, event.y_root)
        elif word in self.metadata["substances"]:
            meta = self.metadata["substances"][word]
            rich_text = f"Substance: {word}\n{'-'*20}\n{meta['summary']}"
            self._show_tooltip(rich_text, event.x_root, event.y_root)
        else:
            self._hide_tooltip()

    def _get_word_at_index(self, index):
        line_start = self.text_area.index(f"{index} linestart")
        line_end = self.text_area.index(f"{index} lineend")
        content = self.text_area.get(line_start, line_end)
        col = int(index.split(".")[1])
        start = col
        while start > 0 and re.match(r"\w", content[start-1]): start -= 1
        end = col
        while end < len(content) and re.match(r"\w", content[end]): end += 1
        return content[start:end]

    def _show_tooltip(self, text, x, y):
        if self.tooltip: return
        self.tooltip = tk.Toplevel(self)
        self.tooltip.wm_overrideredirect(True)
        self.tooltip.wm_geometry(f"+{x+15}+{y+10}")
        label = tk.Label(self.tooltip, text=text, justify='left',
                         background="#252526", foreground="#cccccc",
                         relief='solid', borderwidth=1, font=UI_FONT_SMALL,
                         padx=8, pady=5)
        label.pack()

    def _hide_tooltip(self):
        if self.tooltip:
            self.tooltip.destroy()
            self.tooltip = None

    def _on_key_press(self, event):
        if event.keysym == "Tab" and self.current_ghost:
            self._accept_ghost()
            return "break"
        if event.keysym not in ("Shift_L", "Shift_R", "Control_L", "Control_R"):
            self._clear_ghost()
            self._hide_tooltip()

    def _on_key_release(self, event=None):
        nav_keys = ("Left", "Right", "Up", "Down", "Prior", "Next", "Home", "End", "Tab", "Shift_L", "Shift_R", "Control_L", "Control_R")
        if event and event.keysym not in nav_keys:
            self._update_ghost_suggestion()
        
        self._update_line_numbers()
        self._highlight_syntax()
        
        if self.on_content_changed_callback:
            if self._lint_timer: self.after_cancel(self._lint_timer)
            self._lint_timer = self.after(800, self.on_content_changed_callback)

    def _update_ghost_suggestion(self):
        self._clear_ghost()
        cursor_pos = self.text_area.index("insert")
        line_prefix = self.text_area.get("insert linestart", cursor_pos)
        next_char = self.text_area.get(cursor_pos, f"{cursor_pos}+1c")
        full_prefix = self.text_area.get("1.0", cursor_pos)

        # Block if inside a block comment
        last_open = full_prefix.rfind("/*")
        last_close = full_prefix.rfind("*/")

        # If /* exists and appears after the last */ the cursor must be inside a block comment
        if last_open != -1 and last_open > last_close:
            return
        
        if "//" in line_prefix or next_char.isalnum() or next_char == "_": return

        match = re.search(r"(\w+)$", line_prefix)
        
        if match:
            prefix = match.group(1)
            local_vars = self._get_local_variables()
            candidates = set(list(self.metadata["constants"].keys()) + list(self.metadata["functions"].keys()) + list(self.metadata["substances"].keys()) + list(local_vars))
            
            for c in sorted(list(candidates)):
                if c.startswith(prefix) and c != prefix:
                    self.current_ghost = c[len(prefix):]
                    self.text_area.configure(undo=False)
                    self.text_area.insert(cursor_pos, self.current_ghost, "ghost")
                    self.text_area.configure(undo=True)
                    self.text_area.mark_set("insert", cursor_pos) 
                    
                    if c in self.metadata["functions"]:
                        bbox = self.text_area.bbox(cursor_pos)
                        if bbox:
                            root_x = self.text_area.winfo_rootx() + bbox[0]
                            root_y = self.text_area.winfo_rooty() + bbox[1]
                            meta = self.metadata["functions"][c]
                            rich_text = f"{meta['sig']}\n{'-'*len(meta['sig'])}\n{meta['desc']}"
                            self._show_tooltip(rich_text, root_x, root_y - 40)
                    elif c in self.metadata["constants"]:
                        bbox = self.text_area.bbox(cursor_pos)
                        if bbox:
                            root_x = self.text_area.winfo_rootx() + bbox[0]
                            root_y = self.text_area.winfo_rooty() + bbox[1]
                            meta = self.metadata["constants"][c]
                            val_str = f" = {meta['value']}" if meta['value'] else ""
                            unit_str = f" [{meta['unit']}]" if meta['unit'] else ""
                            rich_text = f"Constant: {c}{val_str}{unit_str}\n{'-'*20}\n{meta['desc']}"
                            self._show_tooltip(rich_text, root_x, root_y - 40)
                    elif c in self.metadata["substances"]:
                        bbox = self.text_area.bbox(cursor_pos)
                        if bbox:
                            root_x = self.text_area.winfo_rootx() + bbox[0]
                            root_y = self.text_area.winfo_rooty() + bbox[1]
                            meta = self.metadata["substances"][c]
                            rich_text = f"Substance: {c}\n{'-'*20}\n{meta['summary']}"
                            self._show_tooltip(rich_text, root_x, root_y - 40)
                    break

    def _get_local_variables(self):
        content = self.get_text()
        words = re.findall(r"\b([a-zA-Z_]\w*)\b", content)
        keywords = {"include", "routine", "function", "return", "end"}
        return {w for w in words if w not in keywords}

    def _accept_ghost(self, event=None):
        if self.current_ghost:
            cursor_pos = self.text_area.index("insert")
            ghost_len = len(self.current_ghost)
            self.text_area.tag_remove("ghost", cursor_pos, f"{cursor_pos} + {ghost_len}c")
            self.text_area.mark_set("insert", f"{cursor_pos} + {ghost_len}c")
            self.current_ghost = ""
            self._hide_tooltip()
            self._highlight_syntax()
            return "break"
        return None

    def _clear_ghost(self):
        ranges = self.text_area.tag_ranges("ghost")
        if ranges:
            self.text_area.configure(undo=False)
            for i in range(len(ranges)-2, -1, -2):
                self.text_area.delete(ranges[i], ranges[i+1])
            self.text_area.configure(undo=True)
        self.current_ghost = ""

    def get_text(self):
        full_content = self.text_area.get("1.0", "end-1c")
        ranges = self.text_area.tag_ranges("ghost")
        if not ranges: return full_content
        parts = []
        last_pos = "1.0"
        for i in range(0, len(ranges), 2):
            parts.append(self.text_area.get(last_pos, ranges[i]))
            last_pos = ranges[i+1]
        parts.append(self.text_area.get(last_pos, "end"))
        return "".join(parts).strip()

    def _on_vscroll(self, *args):
        self.text_area.yview(*args)
        self._update_line_numbers()

    def _update_line_numbers(self):
        """Stable rendering loop that supports word-wrapping."""
        self.line_numbers.delete("all")
        i = self.text_area.index("@0,0")
        while True:
            dline = self.text_area.dlineinfo(i)
            if dline is None: break
            
            y = dline[1]
            linenum = str(i).split(".")[0]
            
            # Only draw the number if this is the START of a logical line
            # If it's a wrapped visual line, the 'linestart' index will differ from current 'i'
            if self.text_area.index(f"{i} linestart") == i:
                self.line_numbers.create_text(30, y + 2, anchor="ne", text=linenum, fill="#505050", font=MONOSPACED_FONT_SMALL)
            
            i = self.text_area.index("%s + 1 line" % i)
            if self.text_area.compare(i, "==", "end"): break

    def _highlight_syntax(self):
        content = self.get_text()
        for tag in ["keyword", "function", "substance", "string", "comment", "unit", "number"]:
            self.text_area.tag_remove(tag, "1.0", "end")
        
        # Dynamic rules based on C++ Registry
        substance_pattern = r"\b(" + "|".join(re.escape(s) for s in self.metadata["substances"].keys()) + r")\b" if self.metadata["substances"] else r"(?!x)x"
        function_pattern = r"\b(" + "|".join(re.escape(f) for f in self.metadata["functions"].keys()) + r")\b" if self.metadata["functions"] else r"(?!x)x"
        
        rules = [
            ("keyword", r"\b(include|routine|function|return|end)\b"),
            ("comment", r"//.*|/\*[\s\S]*?\*/"),
            ("unit", r"\[.*?\]"),
            ("string", r'".*?"'),
            ("number", r"\b\d+\.?\d*([eE][-+]?\d+)?\b"),
            ("substance", substance_pattern),
            ("function", function_pattern)
        ]
        
        for tag, pattern in rules:
            for m in re.finditer(pattern, content):
                self.text_area.tag_add(tag, f"1.0 + {m.start()}c", f"1.0 + {m.end()}c")

    def _on_click(self, event):
        self._clear_ghost()
        self._hide_tooltip()
        self.after(1, self._update_line_numbers)

    def _jump_left(self, shift=False):
        self._clear_ghost()
        pos = self.text_area.index("insert")
        content = self.text_area.get("1.0", pos)[::-1]
        match = re.search(r"[ \t\n\.,\(\)\[\]\{\}\+\-\*/\^=:]", content)
        offset = (match.start() or 1) if match else len(content)
        new_pos = self.text_area.index(f"insert - {offset}c")
        if not shift: self.text_area.tag_remove("sel", "1.0", "end")
        self.text_area.mark_set("insert", new_pos)
        self.text_area.see(new_pos)
        return "break"

    def _jump_right(self, shift=False):
        self._clear_ghost()
        pos = self.text_area.index("insert")
        content = self.text_area.get(pos, "end-1c")
        match = re.search(r"[ \t\n\.,\(\)\[\]\{\}\+\-\*/\^=:]", content)
        offset = (match.start() or 1) if match else len(content)
        new_pos = self.text_area.index(f"insert + {offset}c")
        if not shift: self.text_area.tag_remove("sel", "1.0", "end")
        self.text_area.mark_set("insert", new_pos)
        self.text_area.see(new_pos)
        return "break"

    def _delete_left(self, event=None):
        start = self.text_area.index("insert")
        self._jump_left()
        self.text_area.delete("insert", start)
        return "break"

    def _delete_right(self, event=None):
        start = self.text_area.index("insert")
        self._jump_right()
        self.text_area.delete(start, "insert")
        return "break"

    def set_text(self, text):
        self._clear_ghost()
        self.text_area.delete("1.0", "end")
        self.text_area.insert("1.0", text)
        self._update_line_numbers()
        self._highlight_syntax()
