import CoolProp.CoolProp as CP
import numpy as np
import struct
import os

# Output Directory
OUT = "materials"
if not os.path.exists(OUT):
    os.makedirs(OUT)

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

# Updated to 1D/2D Smart system using Gemini
def export_substance(name, p_range=(1e4, 2e7), t_range=(250, 800)):
    print(f"Exporting {name}...")
    
    # Standard (P, T) Grids for properties
    p_grid = np.geomspace(p_range[0], p_range[1], 50)
    t_grid = np.linspace(t_range[0], t_range[1], 50)
    
    props = {
        'h': 'H', 's': 'S', 'u': 'U', 'rho': 'D', 'mu': 'V', 'k': 'L', 'Pr': 'PRANDTL'
    }

    for code, cp_name in props.items():
        data = []
        for p in p_grid:
            for t in t_grid:
                try: val = CP.PropsSI(cp_name, 'P', p, 'T', t, name)
                except: val = 0.0
                data.append(val)
        write_cnesbin(os.path.join(OUT, f"{name}_{code}.cnesbin"), p_grid, t_grid, data)
    
    # Specific volume v (P, T)
    data_v = []
    for p in p_grid:
        for t in t_grid:
            try: 
                rho = CP.PropsSI('D', 'P', p, 'T', t, name)
                data_v.append(1.0/rho if rho > 0 else 0.0)
            except: data_v.append(0.0)
    write_cnesbin(os.path.join(OUT, f"{name}_v.cnesbin"), p_grid, t_grid, data_v)

    # Inverted Grids: T(P, h) and T(P, s)
    # These allow lookups like Temperature(Water, P=P1, h=h1)
    # We use a grid of P and H/S to find T
    h_min = CP.PropsSI('H', 'P', p_range[0], 'T', t_range[0], name)
    h_max = CP.PropsSI('H', 'P', p_range[1], 'T', t_range[1], name)
    h_grid = np.linspace(h_min, h_max, 50)

    data_t_ph = []
    for p in p_grid:
        for h in h_grid:
            try: data_t_ph.append(CP.PropsSI('T', 'P', p, 'H', h, name))
            except: data_t_ph.append(0.0)
    write_cnesbin(os.path.join(OUT, f"{name}_T_ph.cnesbin"), p_grid, h_grid, data_t_ph)

    s_min = CP.PropsSI('S', 'P', p_range[0], 'T', t_range[0], name)
    s_max = CP.PropsSI('S', 'P', p_range[1], 'T', t_range[1], name)
    s_grid = np.linspace(s_min, s_max, 50)

    data_t_ps = []
    for p in p_grid:
        for s in s_grid:
            try: data_t_ps.append(CP.PropsSI('T', 'P', p, 'S', s, name))
            except: data_t_ps.append(0.0)
    write_cnesbin(os.path.join(OUT, f"{name}_T_ps.cnesbin"), p_grid, s_grid, data_t_ps)

    # Saturation Lookups
    # Psat(T), Tsat(P)
    t_sat_grid = np.linspace(t_range[0], CP.PropsSI('TCRIT', '', 0, '', 0, name)-0.1, 100)
    psat_data = [CP.PropsSI('P', 'T', t, 'Q', 0, name) for t in t_sat_grid]
    write_cnesbin(os.path.join(OUT, f"{name}_Psat.cnesbin"), [0.0], t_sat_grid, psat_data)

    p_sat_grid = np.geomspace(p_range[0], CP.PropsSI('PCRIT', '', 0, '', 0, name)-100, 100)
    tsat_data = [CP.PropsSI('T', 'P', p, 'Q', 0, name) for p in p_sat_grid]
    write_cnesbin(os.path.join(OUT, f"{name}_Tsat.cnesbin"), p_sat_grid, [0.0], tsat_data)

    # Two-Phase Saturated Properties: hf(P), hg(P), sf(P), sg(P)
    # Allows Enthalpy(Water, P=P1, x=0.5)
    for pcode, cp_name, q in [('hf', 'H', 0), ('hg', 'H', 1), ('sf', 'S', 0), ('sg', 'S', 1)]:
        data = []
        for p in p_sat_grid:
            try: data.append(CP.PropsSI(cp_name, 'P', p, 'Q', q, name))
            except: data.append(0.0)
        write_cnesbin(os.path.join(OUT, f"{name}_{pcode}.cnesbin"), p_sat_grid, [0.0], data)

if __name__ == "__main__":
    # Pressure is expressed in Pa, Temperature in K
    export_substance("Water", p_range=(1e4, 22e6), t_range=(274, 1000))
    export_substance("R134a", p_range=(1e4, 4e6), t_range=(230, 450))
    export_substance("R12", p_range=(1e4, 4e6), t_range=(230, 450))
    print("Export Complete.")
