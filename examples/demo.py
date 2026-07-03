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

    # Register constants, substances, and functions
    # This prepares the environment with standard thermodynamic data and routines
    # We aren't finding the actual substances here, just loading in ideal gases
    system.constant_registry().load_standard_constants()
    system.substance_manager().register_ideal_gasses()
    
    # Expose custom math & property functions (e.g. SpecificVolume, Entropy, etc.)
    cones.register_builtin_functions(system.function_registry(), system.substance_manager())

    # Define a thermodynamic system of equations (Brayton Cycle State 1 & Compression)
    cnes_script = """
    // State 1: Air at ambient conditions
    T_1 := 300 [K]
    P_1 := 101325 [Pa]
    r_comp := 8
    
    // Compute State 1 properties
    v_1 = SpecificVolume(Air, T=T_1, P=P_1)
    s_1 = Entropy(Air, T=T_1, P=P_1)
    u_1 = InternalEnergy(Air, T=T_1, P=P_1)
    
    // State 2: Isentropic Compression
    v_2 = v_1 / r_comp
    s_2 = s_1
    
    // Find pressure and temperature at State 2
    v_2 = SpecificVolume(Air, T=T_2, P=P_2)
    s_2 = Entropy(Air, T=T_2, P=P_2)
    u_2 = InternalEnergy(Air, T=T_2, P=P_2)
    
    // Guess values to guide the solver
    T_2.guess := 680
    P_2.guess := 1.8e6
    """

    print("\nScanning and parsing CNES script...")
    lexer = cones.Lexer(cnes_script)
    tokens = lexer.scan_tokens()
    
    # The parser populates the equations and variables inside our System instance
    parser = cones.Parser(tokens, system, ".")
    parser.parse()

    print(f"Successfully loaded {system.get_equation_count()} equations.")

    # Instantiate the Newton-Raphson Solver
    #   Arguments: tolerance (default: 1e-9), max_iterations (default: 1000), verbose (default: false)
    solver = cones.NewtonSolver(1e-9, 500, False)
    
    # Enable or disable the newly implemented Block Decomposition solver
    # When enabled, the solver groups dependent equations and variables into sequentials blocks
    # For this example this will do next to nothing
    solver.set_blocking(True)
    
    print("\nSolving thermodynamic system...")
    report = solver.solve(system)

    if report.success:
        print(f"Convergence achieved in {report.iterations} iterations!")
        
        # 5. Retrieve and print results from the variable registry
        registry = system.registry()
        print("\n==========================================")
        print(f" {'Variable':<12} | {'Value':<14} | {'Unit':<10}")
        print("------------------------------------------")
        for i in range(registry.size()):
            var = registry.get_variable(i)
            # Skip substance identifiers which are dummy variables
            if var.name == "Air":
                continue
            
            # Format value
            if abs(var.value) > 1e4 or (0 < abs(var.value) < 1e-3):
                val_str = f"{var.value:.4e}"
            else:
                val_str = f"{var.value:.4f}"
                
            print(f" {var.name:<12} | {val_str:>14} | {var.unit_name:<10}")
        print("==========================================")
    else:
        print(f"Solver failed to converge: {report.error_msg}", file=sys.stderr)

if __name__ == "__main__":
    run_demo()
