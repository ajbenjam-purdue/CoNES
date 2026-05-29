## CoNES Changelog
CoNES versioning follows the `Major.Minor.Patch` pattern. The current version is `0.1.7`.

### Version 0.1.7
  - Added a variety of 1D, SS methods from Bergman et. Al, Fundamentals of Heat and Mass Transfer (7th ed.); more of the tables will be brought into the libraries for ease of use over time
    - Heat rates for: `plane wall`, `cylindrical wall`, and `spherical wall`
    - Heat fluxes for: `plane wall` and `spherical wall`
    - Thermal resistances for: `plane wall`, `cylindrical wall`, and `spherical wall`
  - Unit system revamped
    - Implemented a recursive descent unit parser to interface with potential user strings (like `W/m*K`)
    - Implemented a new typed system for tracking units, obseleted the older string-based system
    - Implemented a unit registry and a unit checking system to provide better API access and .cnes interface
      - Added a specific error message for an unidentified unit being lexed/parsed in a .cnes file
    - Added direct API support for new base units: `Mols, Watts`
    - Added new customary/standard units: `ft` (foot), `in` (inch), `mile`, `lbm` (pound-mass), `slg` (slug), `lbf` (pound-force), `psia` (pounds/sq-in, absolute), `psig` (pounds/sq-in, relative to `1 atm`), `atm` (atmosphere), `BTU` (British thermal units), `cal` (calorie), `hp` (horsepower), `F` (Fahrenheit), `R` (Rankine)
    - Reworked the STP constants to just be in Pascals/Celcius to allow users to more easily cast into the desired units
  - Bug fixes
    - Corrected an unescaped quotation mark in the json output for certain error messages, which would occaisonally cause malformed json outputs
    - Fixed an issue where the parser would give up trying to assign a unit if the unit was not SI-base
  - Improved documentation

### Version 0.1.6
  - Added a slew of common thermodynamic cycle implementations in examples; in total, the following now are represented:
    - Vapor-compression
    - Brayton
    - Otto
    - Lenoir
    - Joule-Thompson
    - Stirling
  - Added a new `thermo_lib.cnes` library with various open- and closed-cycle processes
  - Improved CoNES Studio UI/UX
    - Added a library save feature for more robust library creation and modification
    - Changed failed solution behavior to still display the results for transparency
    - Added expected platform-agnostic keybinds: ctrl+O *(open)*, ctrl+S *(save)*, ctrl+N *(new file)*, ctrl+R *(solve)*
  - Improved performance and solution likelihood
    - Implemented normalization during the solution for both the jacobian matrix and the residuals vector to eliminate order-of-magnitude issues
    - Implemented duplicate equation identification and filtering to correct for potential singularities caused by over-defined systems
    - Implemented a hard lower floor of `1e-7` for all variables by default to reduce divide-by-zero instances which cause instability
    - Increased default solution attempts to 100 from 5 (will increase in the future)
  - Removed outdated reference scripts
  - Bug fixes
    - Corrected the IDE improperly pulling in library files due to an incorrect temp directory search instead of the executable directory search
    - Corrected improper guess scaling causing intermittent and conditional instability
      - This usually was caught by the scramble system, but would incur more iterations than necessary
    - Corrected parsing issues not allowing `x := y [z]` formatted automatic unit association for assertions
  - Documentation improvements

### Version 0.1.5
  - Updated JSON output to include residuals information for use in CoNES Studio
    - Included internal reworks to use a new `SolverReport` data struct for cleaner reading code
  - Updated CoNES studio to make use of the new residuals information and display the identified line information on selection
  - Added support for custom Python interpreter path usage
  - Updated python scripts (CoNES Studio and Substance Table Builder Tool) to automatically download dependencies

### Version 0.1.4
  - Changed parameters to improve performance
    - Adjusted default solution attempts to 500 (up from 200) to improve unstable system solution likelihood
    - Adjusted the default guess for Enthalpy up to 1500 (from the default) to encourage faster convergance
  - Updated Python scripts
    - Introduced native `--IDE` flag for an easier way to open the Python IDE
    - Reworked all python scripts to automatically import dependencies
  - Bug fixes
    - Corrected the solver wrongfully returning -1 when convergence was reached
    - Corrected the solver returning a success after one stable iteration instead of solving to tolerance, which was causing garbage results for more complex systems
    - Corrected conservative step limitations for instability by increasing the step adjustment gain
    - Fixed a set of backwards logic where `current_inf_norm` meeting tolerance was incorrectly registering a failure, not a success
  - Documentation improvements

### Version 0.1.3
  - Minor improvements to API for ideal gases
  - Added automatic and manual substances library creation in the `cnes` tool
  - Changed default parameters and behavior of the `cnesbin` python build tool to improve responsiveness
    - Included new CoolProp substances: Ethylene, Isobutane, Methanol
  - General documentation improvements

### Version 0.1.2
  - Lots of housekeeping in preparation for public release: 
    - Cleaned up the test suite to more consistently reflect correct API usage. Still more to be done here!
    - Improved consistency and clarity of comments throughout source code.
    - Added an overload for `system.registry().get_variable(std::string)` to avoid needless index-getters and translators
    - Added a series of `std::string to_string` overloads to better follow C++ standards
  - Added more inbuilt math and physics functions; check `--list-functions` to see all supported functions
  - Added a dynamic table printing method in an effort to clean up main.cpp and improve readability

### Version 0.1.1
 - First tracked change.