import subprocess
import json
import os
import re
import sys
import tempfile

DEFAULT_EXECUTABLE = "./cnes.exe" if sys.platform.startswith('win32') else "./cnes"

def is_cnes(path:str) -> bool:
    return os.path.exists(path) and path.endswith(".cnes")

def parse_time(solver_ms):
    if solver_ms >= 60000:
        return f"{solver_ms/60000:.0f} min {(solver_ms/1000%60):.0f} sec"
    if solver_ms >= 1000:
        return f"{solver_ms/1000:.3f} sec"
    return f"{solver_ms:.2f} ms"

class CoNESBackend:
    def __init__(self, executable_path=DEFAULT_EXECUTABLE):
        self.exe = os.path.abspath(executable_path)
        
    def version(self):# -> None|str:
        result = subprocess.run([self.exe, "--version"], capture_output=True, text=True, check=False)
        if result.returncode == 0: return result.stdout.strip()
        return None

    def solve(self, file_path, cwd=None):
        """Runs the solver on a file and returns the JSON result."""
        try:
            result = subprocess.run([self.exe, file_path, "--json"], 
                                     capture_output=True, 
                                     text=True, 
                                     check=False,
                                     cwd=cwd)
            if not result.stdout.strip():
                return {"success": False, "error": result.stderr or "No output from solver."}
            return json.loads(result.stdout)
        except Exception as e:
            return {"success": False, "error": str(e)}

    def get_metadata(self, cwd=None):
        """Fetches substances, functions, and constants for autocomplete and rich tooltips."""
        try:
            result = subprocess.run([self.exe, "--out-vscode-metadata"], 
                                     capture_output=True, 
                                     text=True, 
                                     check=False,
                                     cwd=cwd)
            if not result.stdout: return {"substances": {}, "functions": {}, "constants": {}}
            
            parts = result.stdout.split("|||")
            if len(parts) < 3: return {"substances": {}, "functions": {}, "constants": {}}
            
            # Constants: Name:Value:Unit:Desc
            constants = {}
            for p in parts[0].split("|"):
                if not p: continue
                fields = p.split(":")
                name = fields[0].strip()
                val = fields[1].strip() if len(fields) > 1 else ""
                unit = fields[2].strip() if len(fields) > 2 else ""
                desc = fields[3].strip() if len(fields) > 3 else ""
                constants[name] = {"value": val, "unit": unit, "desc": desc}
            
            # Functions: Name(args):Description
            functions = {}
            for p in parts[1].split("|"):
                if not p: continue
                # We expect the C++ side to provide "Signature:Description" or just "Signature"
                if ":" in p:
                    sig, desc = p.split(":", 1)
                else:
                    sig, desc = p, ""
                
                name = sig.split("(")[0].strip()
                functions[name] = {"sig": sig.strip(), "desc": desc.strip()}
                
            # Substances: Name:Summary
            substances = {}
            for p in parts[2].split("|"):
                if not p: continue
                if ":" in p:
                    name, summary = p.split(":", 1)
                else:
                    name, summary = p, ""
                substances[name.strip()] = {"summary": summary.strip()}
            
            return {
                "constants": constants,
                "functions": functions,
                "substances": substances
            }
        except Exception:
            return {"substances": {}, "functions": {}, "constants": {}}

    def lint(self, script_content, cwd=None):
        """Writes content to a temp file and runs the linter."""
        temp_dir = tempfile.gettempdir()
        temp_file = os.path.join(temp_dir, "cones_lint_target.cnes")
        
        try:
            with open(temp_file, "w") as f:
                f.write(script_content)
            
            result = subprocess.run([self.exe, temp_file, "--lint", "--json"], 
                                     capture_output=True, 
                                     text=True, 
                                     check=False,
                                     cwd=cwd)
            
            if not result.stdout.strip():
                # If solver crashed, return stderr as the error
                err = result.stderr.strip() or "Backend crash: No output."
                return {"success": False, "error": err}
            
            data = json.loads(result.stdout)
            
            if not data.get("success") and "error" in data:
                # Clean up the error message for the UI
                msg = data["error"]
                msg = re.sub(r"FATAL ERROR: ", "", msg)
                msg = re.sub(r"Parser: ", "", msg)
                data["error"] = msg
                
                match = re.search(r"Line (\d+)", msg)
                if match:
                    data["error_line"] = int(match.group(1))
            
            return data
        except Exception as e:
            return {"success": False, "error": f"Linter service error: {str(e)}"}
        finally:
            if os.path.exists(temp_file):
                try: os.remove(temp_file)
                except: pass
