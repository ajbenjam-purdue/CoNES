## CoNES Changelog
CoNES versioning follows the Major.Minor.Patch pattern. The current version is 0.1.2.

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