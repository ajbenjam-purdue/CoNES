import sys

# Overall utils, not really colors

# Colors
color_UI = "#0e639c"
color_UI_hover = "#105380"

color_status_OK = "#007acc"
color_status_Success = "#16825d"
color_status_Diverged = "#d68f00"
color_status_Bad = "#a1260d"

# Typefaces
MONOSPACED_TYPEFACE = "Consolas" if sys.platform.startswith('win32') else "Andale Mono"
MONOSPACED_FONT = (MONOSPACED_TYPEFACE, 10)
MONOSPACED_FONT_BOLD = (MONOSPACED_TYPEFACE, 10, "bold")
MONOSPACED_FONT_SMALL = (MONOSPACED_TYPEFACE, 8)

UI_TYPEFACE = "Segoe UI" if sys.platform.startswith('win32') else "SF Pro Light"
UI_FONT = (UI_TYPEFACE, 11)
UI_FONT_SMALL = (UI_TYPEFACE, 9)

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