import importlib.metadata
import subprocess
import sys
import socket
import re
from pathlib import Path

pattern = re.compile(
    r'^\s*include\s+"([^"]+)"\s*$',
    re.MULTILINE
)

def get_all_includes(text: str) -> list[str]:
    return [instance for instance in pattern.findall(text)]

def resolve_name(name: str, paths:list[Path]|None = None) -> Path | None:
    """
    Searches for a .cnes file by name across a list of directory paths.
    Returns the first resolved Path, or None if not found
    """
    if paths is None:
        paths = []

    # Ensure the target filename ends with the .cnes extension
    target_name = name if name.endswith('.cnes') else f"{name}.cnes"

    for directory in paths:
        # Convert to Path object in case a list of strings was passed
        dir_path = Path(directory)
        potential_file = dir_path / target_name
        
        # Return the first matching file found in the prioritized list
        if potential_file.is_file():
            return potential_file

    # Return None if the file was not found in any of the provided paths
    return None

def is_connected():
    try:
        # Attempt to connect to Google's public DNS
        socket.create_connection(("8.8.8.8", 53), timeout=2)
        return True
    except OSError:
        return False

def check_tkinter():
    """
    Checks if tkinter is installed and configured correctly
    Returns (success, error_message)
    """
    try:
        import tkinter
        import _tkinter
        return True, ""
    except ImportError as e:
        msg = "Tkinter (Python's GUI library) is missing or not configured correctly.\n"
        if sys.platform.startswith('darwin'):
            msg += "Suggestion: If using Homebrew, run 'brew install python-tk'"
        elif sys.platform.startswith('linux'):
            msg += "Suggestion: Run 'sudo apt install python3-tk' or your distro's equivalent."
        else:
            msg += f"Error details: {e}"
        return False, msg

def ensure_dependencies(required_packages):
    """
    Checks if packages are installed and attempts to install missing ones
    """
    
    # First check for Tkinter since it's a system dependency
    if 'customtkinter' in required_packages:
        tk_ok, tk_msg = check_tkinter()
        if not tk_ok:
            print("\n" + "!"*60)
            print(tk_msg)
            print("!"*60 + "\n")
            sys.exit(1)

    # pip packages
    try:
        installed = {pkg.metadata['Name'].lower() for pkg in importlib.metadata.distributions()}
    except Exception:
        installed = set()

    missing = [pkg for pkg in required_packages if pkg.lower() not in installed]

    if missing:
        print(f">>> CoNES Bootstrapper: Missing dependencies: {', '.join(missing)}")
        
        # Check if we are in a venv
        in_venv = (sys.prefix != sys.base_prefix)
        
        cmd = [sys.executable, "-m", "pip", "install"]
        if not in_venv:
            # If not in venv, need --user
            cmd.append("--user")
        
        cmd.extend(missing)

        try:
            print(f">>> CoNES Bootstrapper: Attempting to install via {'venv' if in_venv else 'pip --user'}...")
            subprocess.check_call(cmd)
            print(">>> CoNES Bootstrapper: Dependencies installed successfully.")
        except subprocess.CalledProcessError as e:
            if not is_connected():
                print(">>> CoNES Error: No internet connection detected. Cannot install missing packages.")
            else:
                print(f">>> CoNES Error: Failed to install dependencies. Command failed: {' '.join(cmd)}")
            sys.exit(1)
        except Exception as e:
            print(f">>> CoNES Error: An unexpected error occurred during installation: {e}")
            sys.exit(1)

if __name__ == "__main__":
    ensure_dependencies(['customtkinter', 'CoolProp', 'numpy'])
