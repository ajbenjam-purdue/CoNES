#!/usr/bin/env python3
"""
CoNES Python API Demonstration Script
This script shows how to programmatically use the 'cones' package to parse, configure, and solve thermodynamic systems
"""

import os
import sys

# Add build directory to path for development imports if package is not installed globally
# project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# sys.path.insert(0, project_root)
# sys.path.insert(0, os.path.join(project_root, "build"))
# sys.path.insert(0, os.path.join(project_root, "build", "Release"))

import cones

def run_demo():
    print(f"--- CoNES Python API Demo (Version: {cones.Version.string()}) ---")

    # Initialize the system
    system = cones.System()

    # Register constants, ideal gases, tabulated materials, and builtin functions
    system.constant_registry().load_standard_constants()
    system.substance_manager().register_ideal_gasses()
    if os.path.exists("materials"):
        system.substance_manager().load_materials("materials")
    cones.register_builtin_functions(system.function_registry(), system.substance_manager())

    # Define a thermodynamic system of equations (R410A Two-Phase Evaporator with Temperature Glide)
    cnes_script = """
    P_evap := 500000 [Pa]
    x_in := 0.2
    x_out := 1.0

    // Saturation & Two-Phase Temperatures with Glide
    T_bubble = Tsat_f(R410A=1, P=P_evap)
    T_dew = Tsat_g(R410A=1, P=P_evap)
    T_in = Temperature(R410A=1, P=P_evap, x=x_in)
    T_out = Temperature(R410A=1, P=P_evap, x=x_out)

    // Enthalpy & Specific Heat Calculations
    h_in = Enthalpy(R410A=1, P=P_evap, x=x_in)
    h_out = Enthalpy(R410A=1, P=P_evap, x=x_out)
    q_evap = h_out - h_in
    """

    print("\nScanning and parsing CNES script...")
    lexer = cones.Lexer(cnes_script)
    tokens = lexer.scan_tokens()
    
    # The parser populates the equations and variables inside our System instance
    parser = cones.Parser(tokens, system, ".")
    parser.parse()

    print(f"Successfully loaded {system.get_equation_count()} equations.")

    # Instantiate the Newton-Raphson Solver
    solver = cones.NewtonSolver(1e-9, 500, False)
    solver.set_blocking(True)
    
    print("\nSolving thermodynamic system...")
    report = solver.solve(system)

    if report.success:
        print(f"Convergence achieved in {report.iterations} iterations!")
        
        # Retrieve and print results from the variable registry
        registry = system.registry()
        print("\n=====================================================")
        print(f" {'Variable':<14} | {'Value':<14} | {'Unit':<10}")
        print("-----------------------------------------------------")
        for i in range(registry.size()):
            var = registry.get_variable(i)
            if var.name == "R410A":
                continue
            
            if abs(var.value) > 1e4 or (0 < abs(var.value) < 1e-3):
                val_str = f"{var.value:.4e}"
            else:
                val_str = f"{var.value:.4f}"
                
            print(f" {var.name:<14} | {val_str:>14} | {var.unit_name:<10}")
        print("=====================================================")

        # Direct Python C++ Dual Property Evaluation Demo
        r410a = system.substance_manager().get("R410A")
        if r410a:
            p_val = cones.DualNumber(500000.0, 1.0)
            x_val = cones.DualNumber(0.5, 0.0)
            t_eval = r410a.evaluate(cones.PropertyType.TEMPERATURE, [
                cones.PropertyArg(cones.PropertyType.PRESSURE, p_val),
                cones.PropertyArg(cones.PropertyType.QUALITY, x_val)
            ])
            print(f"\nDirect C++ Python Binding Evaluation:")
            print(f" R410A T(P=500kPa, x=0.5) = {t_eval.val:.2f} K (dT/dP = {t_eval.der:.6e})")
    else:
        print(f"Solver failed to converge: {report.error_msg}", file=sys.stderr)

if __name__ == "__main__":
    run_demo()
