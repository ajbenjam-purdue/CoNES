#ifndef CONES_CORE_VERSION_HPP
#define CONES_CORE_VERSION_HPP

#include <string>

namespace cones {

/**
 * @brief Versioning constants for the CoNES engine.
 */
struct Version {
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 2;
    static constexpr int PATCH = 0;
    
    static std::string string() {
        return std::to_string(MAJOR) + "." + 
               std::to_string(MINOR) + "." + 
               std::to_string(PATCH);
    }
    
    static std::string full() {
        return "CoNES v" + string();
    }
};

} // namespace cones

#endif
