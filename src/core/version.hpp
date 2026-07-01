#ifndef CONES_CORE_VERSION_HPP
#define CONES_CORE_VERSION_HPP

#include <string>

namespace cones {

/**
 * @brief Versioning constants for the CoNES engine.
 */
struct Version {

    // Binary
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 2;
    static constexpr int PATCH = 2;

    // Lang standards
    static constexpr int LANG_MAJOR = 1;
    static constexpr int LANG_MINOR = 2;
    
    static std::string string() {
        return std::to_string(MAJOR) + "." + 
               std::to_string(MINOR) + "." + 
               std::to_string(PATCH);
    }
    
    static std::string lang_string() {
        return std::to_string(LANG_MAJOR) + "." + 
               std::to_string(LANG_MINOR);
    }
    
    static std::string full() {
        return "CoNES v" + string();
    }
    
    static std::string lang_full() {
        return "CoNES Language Standard v" + lang_string();
    }
};

} // namespace cones

#endif
