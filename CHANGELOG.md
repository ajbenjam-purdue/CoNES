## CoNES Changelog
CoNES versioning follows the Major.Minor.Patch pattern. The current version is 0.1.4.

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