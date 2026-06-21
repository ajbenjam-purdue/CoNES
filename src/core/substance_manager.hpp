#ifndef CONES_CORE_SUBSTANCE_MANAGER_HPP
#define CONES_CORE_SUBSTANCE_MANAGER_HPP

#include "substance.hpp"
#include "ideal_gas.hpp"
#include "tabulated_substance.hpp"
#include "property_types.hpp"
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

namespace cones
{
    // For sure there's a better way to do this
    std::vector<std::shared_ptr<Substance>> ideal_gasses = {
        std::make_shared<IdealGasSubstance>("Air", 287.05, 1005.0),
        std::make_shared<IdealGasSubstance>("Argon", 208.1, 520.0),
        std::make_shared<IdealGasSubstance>("CO2", 188.9, 845.9),
        std::make_shared<IdealGasSubstance>("Nitrogen", 296.8, 1041.0),
        std::make_shared<IdealGasSubstance>("O2", 259.8, 918.9)
    };

    /**
     * @brief Manages all known substances in the CoNES environment.
     * This includes all substance derivatives like ideal gases and tabulated_substances.
     */
    class SubstanceManager
    {
        std::map<std::string, std::shared_ptr<Substance>> substances_;

    public:
        // Method to register a substance in the manager
        void register_substance(std::shared_ptr<Substance> sub)
        {
            substances_[sub->name()] = sub;
        }

        // Method to register all ideal gases internally
        void register_ideal_gasses()
        {
            for (auto &sub : ideal_gasses)
            {
                substances_[sub->name()] = sub;
            }
        }

        // Method to automatically load all Tabulated Substances from a directory
        void load_materials(const std::string& directory_path)
        {
            std::filesystem::path materials_path = directory_path;
            if (!std::filesystem::exists(materials_path)) return;

            for (const auto &entry : std::filesystem::directory_iterator(materials_path))
            {
                if (entry.path().extension() == ".cnesbin")
                {
                    std::string fname = entry.path().stem().string(); // e.g., "Water_h"

                    // Parse the _ char
                    size_t underscore_pos = fname.find('_');
                    if (underscore_pos == std::string::npos)
                        continue; // None, skip
                    std::string sub_name = fname.substr(0, underscore_pos);
                    std::string prop_code = fname.substr(underscore_pos + 1);

                    // Special handling for multi-word property codes (T_ph, etc.)
                    PropertyType prop = string_to_property(prop_code);
                    if (prop == PropertyType::UNKNOWN)
                    {
                        // Outdated catch for inverted T(p,h) and T(p,s) tables
                        if (prop_code == "T_ph")
                            prop = PropertyType::T_PH;
                        else if (prop_code == "T_ps")
                            prop = PropertyType::T_PS;
                    }

                    if (prop == PropertyType::UNKNOWN)
                        continue; // Ignore bad input

                    // Load and register to the appropriate substance
                    auto sub = std::dynamic_pointer_cast<TabulatedSubstance>(get(sub_name));
                    if (!sub)
                    { // This is very reliant on correct naming of cnes binary tables. Should consider improving the implementation.
                        sub = std::make_shared<TabulatedSubstance>(sub_name);
                        register_substance(sub);
                    }

                    // Populate the substance with the correct property from the provided path
                    sub->load_table(prop, entry.path().string());
                }
            }
        }

        std::shared_ptr<Substance> get(const std::string& name) const {
            auto it = substances_.find(name);
            return (it != substances_.end()) ? it->second : nullptr;
        }

        std::vector<std::string> get_substance_names() const {
            std::vector<std::string> names;
            for (const auto& [name, _] : substances_) names.push_back(name);
            return names;
        }
    };

} // namespace cones

#endif
