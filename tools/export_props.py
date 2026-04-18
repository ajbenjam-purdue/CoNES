import CoolProp.CoolProp as CP
import numpy as np
import struct
import os

def export_to_cnes(substance_name, output_path):
    print(f"Exporting {substance_name} to {output_path}...")
    
    # Define Grids
    # P: 1 bar to 200 bar (Log scale is often better for pressure)
    p_grid = np.geomspace(1e5, 20e6, 50) 
    # T: 300 K to 800 K
    t_grid = np.linspace(300, 800, 50)
    
    data = []
    for p in p_grid:
        for t in t_grid:
            try:
                # Get Enthalpy [J/kg]
                h = CP.PropsSI('H', 'P', p, 'T', t, substance_name)
            except:
                h = 0.0 # Error handling for two-phase region (simplified)
            data.append(h)
            
    # Write Binary
    with open(output_path, "wb") as f:
        # 1. Magic
        f.write(b"CNES")
        # 2. Dimensions
        f.write(struct.pack("ii", len(p_grid), len(t_grid)))
        # 3. Grids
        f.write(struct.pack(f"{len(p_grid)}d", *p_grid))
        f.write(struct.pack(f"{len(t_grid)}d", *t_grid))
        # 4. Data
        f.write(struct.pack(f"{len(data)}d", *data))
        
    print("Done.")

if __name__ == "__main__":
    if not os.path.exists("materials"):
        os.makedirs("materials")
    export_to_cnes("Water", "materials/Water_h.cnesbin")
