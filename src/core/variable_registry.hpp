#ifndef CONES_CORE_VARIABLE_REGISTRY_HPP
#define CONES_CORE_VARIABLE_REGISTRY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace cones {

/**
 * Manages the mapping between variable names and their numerical indices.
 * 
 * Bridge between the human-readable script and the high-performance
 * numerical solver which uses index-based lookup.
 */
class VariableRegistry {
    std::unordered_map<std::string, int> name_to_index_;
    std::vector<std::string> index_to_name_;

public:
    /**
     * Registers a variable name and returns its index.
     * If already registered, returns the existing index.
     */
    int register_variable(const std::string& name) {
        auto it = name_to_index_.find(name);
        if (it != name_to_index_.end()) {
            return it->second;
        }
        
        int index = static_cast<int>(index_to_name_.size());
        name_to_index_[name] = index;
        index_to_name_.push_back(name);
        return index;
    }

    // Returns the index of the human-legible string input. If not found, returns -1 but does not throw an error.
    int get_index(const std::string& name) const {
        auto it = name_to_index_.find(name);
        if (it == name_to_index_.end()) {
            return -1;
        }
        return it->second;
    }

    // Returns the human-legible string at the index input.
    const std::string& get_name(int index) const {
        return index_to_name_.at(index);
    }

    const size_t size() { return index_to_name_.size(); }
};

} // namespace cones

#endif // CONES_CORE_VARIABLE_REGISTRY_HPP
