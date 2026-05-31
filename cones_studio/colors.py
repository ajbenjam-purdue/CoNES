"""
Theme and UI constants for CoNES Studio.
Contains color definitions, fonts, and common component configurations.
"""
import sys

# UI Accent Colors
COLOR_UI = "#0e639c"
COLOR_UI_HOVER = "#105380"

# Status Colors
COLOR_STATUS_OK = "#007acc"
COLOR_STATUS_SUCCESS = "#16825d"
COLOR_STATUS_DIVERGED = "#d68f00"
COLOR_STATUS_BAD = "#a1260d"

# Compatibility aliases (to avoid breaking existing code)
color_UI = COLOR_UI
color_UI_hover = COLOR_UI_HOVER
color_status_OK = COLOR_STATUS_OK
color_status_Success = COLOR_STATUS_SUCCESS
color_status_Diverged = COLOR_STATUS_DIVERGED
color_status_Bad = COLOR_STATUS_BAD

# Typefaces
MONOSPACED_TYPEFACE = "Consolas" if sys.platform.startswith('win32') else "Andale Mono"
MONOSPACED_FONT = (MONOSPACED_TYPEFACE, 10)
MONOSPACED_FONT_BOLD = (MONOSPACED_TYPEFACE, 10, "bold")
MONOSPACED_FONT_SMALL = (MONOSPACED_TYPEFACE, 8)

UI_TYPEFACE = "Segoe UI" if sys.platform.startswith('win32') else "SF Pro Light"
UI_FONT = (UI_TYPEFACE, 11)
UI_FONT_SMALL = (UI_TYPEFACE, 9)

# Common Component Options
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