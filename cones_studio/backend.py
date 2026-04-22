import subprocess
import json
import os
import re

class CoNESBackend:
    def __init__(self, executable_path="./cnes.exe"):
        self.exe = executable_path

    def solve(self, file_path):
        """Runs the solver on a file and returns the JSON result."""
        try:
            result = subprocess.run([self.exe, file_path, "--json"], 
                                     capture_output=True, 
                                     text=True, 
                                     check=False)
            if not result.stdout.strip():
                return {"success": False, "error": result.stderr or "No output from solver."}
            return json.loads(result.stdout)
        except Exception as e:
            return {"success": False, "error": str(e)}

    def get_metadata(self):
        """Fetches substances, functions, and constants for autocomplete and rich tooltips."""
        try:
            result = subprocess.run([self.exe, "--out-vscode-metadata"], 
                                     capture_output=True, 
                                     text=True, 
                                     check=False)
            if not result.stdout: return {"substances": [], "functions": {}, "constants": []}
            
            parts = result.stdout.split("|||")
            if len(parts) < 3: return {"substances": [], "functions": {}, "constants": []}
            
            # Constants: Name:Value:Desc
            constants = [p.split(":")[0].strip() for p in parts[0].split("|") if p]
            
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
                
            # Substances: Name
            substances = [p.strip() for p in parts[2].split("|") if p]
            
            return {
                "constants": sorted(list(set(constants))),
                "functions": functions,
                "substances": sorted(list(set(substances)))
            }
        except Exception:
            return {"substances": [], "functions": {}, "constants": []}

    def lint(self, script_content):
        """Writes content to a temp file and runs the linter."""
        # Use a platform-appropriate temp path
        temp_dir = os.environ.get("TEMP", os.environ.get("TMP", "/tmp"))
        temp_file = os.path.join(temp_dir, "cones_lint_target.cnes")
        
        try:
            with open(temp_file, "w") as f:
                f.write(script_content)
            
            result = subprocess.run([self.exe, temp_file, "--lint", "--json"], 
                                     capture_output=True, 
                                     text=True, 
                                     check=False)
            
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
