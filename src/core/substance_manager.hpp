#ifndef CONES_CORE_SUBSTANCE_MANAGER_HPP
#define CONES_CORE_SUBSTANCE_MANAGER_HPP

#include "substance.hpp"
#include <map>
#include <memory>
#include <vector>
#include <string>

namespace cones
{

    /**
     * @brief Manages all known substances in the CoNES environment.
     * This includes all substance derivatives like ideal gases and tabulated_substances.
     */
    class SubstanceManager
    {
        std::map<std::string, std::shared_ptr<Substance>> substances_;

    public:
        void register_substance(std::shared_ptr<Substance> sub)
        {
            substances_[sub->name()] = sub;
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
