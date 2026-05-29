# What substances to tabulate
subs = ["Water", "R12", "R13", "R14", "R21", "R22", "R32", "R1234yf", "R125", "R134a", "R143a", "R410A", "Ethylene", "Isobutane", "Methanol"]
# How many points in each table
count = 40

# courtesy of https://stackoverflow.com/questions/46419607/how-to-automatically-install-required-packages-from-a-python-script-as-necessary
import sys, os
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(script_dir, '..', 'cones_studio'))
from bootstrap import ensure_dependencies
ensure_dependencies({'CoolProp', 'numpy'})

import CoolProp.CoolProp as CP
import numpy as np
import struct
import os, sys
import json

# Output Directory
OUT = "materials"
if not os.path.exists(OUT):
    os.makedirs(OUT)

def clamp(x, val_low, val_high):
    return min(max(x, val_low), val_high)

def write_cnesbin(path, p_grid, t_grid, data):
    """
    Standard binary format:
    [CNES (4b)][np (4b)][nt (4b)][p_grid (np*8b)][t_grid (nt*8b)][data (np*nt*8b)]
    """
    with open(path, "wb") as f:
        f.write(b"CNES")
        f.write(struct.pack("ii", len(p_grid), len(t_grid)))
        f.write(struct.pack(f"{len(p_grid)}d", *p_grid))
        f.write(struct.pack(f"{len(t_grid)}d", *t_grid))
        f.write(struct.pack(f"{len(data)}d", *data))

def PropsSISafe(out_prop, in1_prop, in1_val, in2_prop, in2_val, fluid):
    """
    Modular wrapper for CoolProp to handle bounds errors and critical point singularities.
    """
    try:
        # Get fluid bounds for clamping
        t_crit = CP.PropsSI('TCRIT', '', 0, '', 0, fluid)
        t_min = CP.PropsSI('TMIN', '', 0, '', 0, fluid)
        p_crit = CP.PropsSI('PCRIT', '', 0, '', 0, fluid)

        # Clamp Temperature inputs
        if in1_prop == 'T': in1_val = clamp(in1_val, t_min, 5000.0)
        if in2_prop == 'T': in2_val = clamp(in2_val, t_min, 5000.0)

        # Clamping for Saturation (Q=0 or Q=1)
        # If we use Q, the other parameter (P or T) must be below critical
        is_sat = (in1_prop == 'Q' or in2_prop == 'Q')
        if is_sat:
            if in1_prop == 'T': in1_val = min(in1_val, t_crit - 0.001)
            if in2_prop == 'T': in2_val = min(in2_val, t_crit - 0.001)
            if in1_prop == 'P': in1_val = min(in1_val, p_crit - 1.0)
            if in2_prop == 'P': in2_val = min(in2_val, p_crit - 1.0)

        return CP.PropsSI(out_prop, in1_prop, in1_val, in2_prop, in2_val, fluid)
    except:
        # If lookup fails, try to return a sensible physical boundary or 0.0
        return 0.0

# Def. ranges: 1kPa - 200 MPa; 100K - 1000K
def export_substance(name, p_range=(1e3, 2e8), t_range=(100, 1000), count=50):
    # Fetch fluid-specific boundaries
    try:
        t_min = CP.PropsSI('TMIN', '', 0, '', 0, name)
        t_crit = CP.PropsSI('TCRIT', '', 0, '', 0, name)
        p_crit = CP.PropsSI('PCRIT', '', 0, '', 0, name)
    except:
        print(f"  - Error fetching bounds for {name}")
        return

    # Adapt ranges to fluid bounds
    t_start = max(t_range[0], t_min + 1.0)
    t_end = t_range[1]
    p_start = p_range[0]
    p_end = min(p_range[1], 100e6) # Cap at 100 MPa

    # Standard (P, T) Grids
    p_grid = np.geomspace(p_start, p_end, count) # Log dist
    t_grid = np.linspace(t_start, t_end, count)  # Lin dist
    
    # Map to CoolProp's prop labels
    props = {'h': 'H', 's': 'S', 'u': 'U', 'rho': 'D', 'mu': 'V', 'k': 'L', 'Pr': 'PRANDTL'}

    for code, cp_name in props.items():
        # Get a list of property values in p-major array form
        data = [PropsSISafe(cp_name, 'P', p, 'T', t, name) for p in p_grid for t in t_grid]
        
        # Write the data to a binary table for small size
        write_cnesbin(os.path.join(OUT, f"{name}_{code}.cnesbin"), p_grid, t_grid, data)
    
    # Specific volume v (P, T)
    data_v = []
    for p in p_grid:
        for t in t_grid:
            rho = PropsSISafe('D', 'P', p, 'T', t, name)
            data_v.append(1.0/rho if rho > 0 else 0.0)
    write_cnesbin(os.path.join(OUT, f"{name}_v.cnesbin"), p_grid, t_grid, data_v)

    # Inverted Grids: T(P, h) and T(P, s)
    h_min = PropsSISafe('H', 'P', p_start, 'T', t_start, name)
    h_max = PropsSISafe('H', 'P', p_end, 'T', t_end, name)
    if h_min != 0 and h_max != 0:
        h_grid = np.linspace(h_min, h_max, count)
        data_t_ph = [PropsSISafe('T', 'P', p, 'H', h, name) for p in p_grid for h in h_grid]
        write_cnesbin(os.path.join(OUT, f"{name}_T_ph.cnesbin"), p_grid, h_grid, data_t_ph)

    s_min = PropsSISafe('S', 'P', p_start, 'T', t_start, name)
    s_max = PropsSISafe('S', 'P', p_end, 'T', t_end, name)
    if s_min != 0 and s_max != 0:
        s_grid = np.linspace(s_min, s_max, count)
        data_t_ps = [PropsSISafe('T', 'P', p, 'S', s, name) for p in p_grid for s in s_grid]
        write_cnesbin(os.path.join(OUT, f"{name}_T_ps.cnesbin"), p_grid, s_grid, data_t_ps)

    # Saturation Lookups
    t_sat_grid = np.linspace(t_min + 0.1, t_crit - 0.1, 100)
    psat_data = [PropsSISafe('P', 'T', t, 'Q', 0, name) for t in t_sat_grid]
    write_cnesbin(os.path.join(OUT, f"{name}_Psat.cnesbin"), [0.0], t_sat_grid, psat_data)

    p_sat_grid = np.geomspace(p_start, p_crit - 100, 100)
    tsat_data = [PropsSISafe('T', 'P', p, 'Q', 0, name) for p in p_sat_grid]
    write_cnesbin(os.path.join(OUT, f"{name}_Tsat.cnesbin"), p_sat_grid, [0.0], tsat_data)

    # Two-Phase Boundary tables vs Pressure
    for pcode, cp_name, q in [('hf', 'H', 0), ('hg', 'H', 1), ('sf', 'S', 0), ('sg', 'S', 1)]:
        data = [PropsSISafe(cp_name, 'P', p, 'Q', q, name) for p in p_sat_grid]
        write_cnesbin(os.path.join(OUT, f"{name}_{pcode}.cnesbin"), p_sat_grid, [0.0], data)

def update_vscode_syntax(substances):
    print("Updating VS Code syntax highlighting...")
    syntax_path = "src/lang/cnes/syntaxes/cnes.tmLanguage.json"
    if not os.path.exists(syntax_path): return
    with open(syntax_path, 'r') as f: syntax = json.load(f)
    if 'repository' in syntax and 'substances' in syntax['repository']:
        pattern = "\\b(" + "|".join(substances) + ")\\b"
        syntax['repository']['substances']['patterns'][0]['match'] = pattern
        with open(syntax_path, 'w') as f: json.dump(syntax, f, indent=4)

if __name__ == "__main__":
    if len(sys.argv) == 2: # Clamp
        try: count = min(max(int(sys.argv[1]), 12), 100)
        except: pass
    
    # Build the tables
    print(f"Creating {count}-item binary substance tables...")
    for i, s in enumerate(subs):
        print(f"[{i+1}/{len(subs)}] {s}: ", end='', flush=True)
        export_substance(s, count=count)
        print('Done')
    
    # TODO: Make listing more resilient... Do I even need to keep the vsc plugin up-to-date?
    update_vscode_syntax(subs + ["Air", "Argon", "CO2", "Nitrogen", "O2"])
    print("Export Complete.")
