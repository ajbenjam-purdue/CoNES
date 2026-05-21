#ifndef CONES_CORE_SUBSTANCE_MANAGER_HPP
#define CONES_CORE_SUBSTANCE_MANAGER_HPP

#include "substance.hpp"
#include "ideal_gas.hpp"
#include <map>
#include <memory>
#include <vector>
#include <string>

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
