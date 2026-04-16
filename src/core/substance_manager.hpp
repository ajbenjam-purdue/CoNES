#ifndef CONES_CORE_SUBSTANCE_MANAGER_HPP
#define CONES_CORE_SUBSTANCE_MANAGER_HPP

#include "substance.hpp"
#include <map>
#include <memory>

namespace cones
{

    /**
     * @brief Manages all known substances in the CoNES environment.
     * To register a substance with the manager, use the `register_substance` method.
     * To get a shared pointer to a substance of the manager, use the `get` method. 
     */
    class SubstanceManager
    {
        std::map<std::string, std::shared_ptr<Substance>> substances_;

    public:
        void register_substance(std::shared_ptr<Substance> sub)
        {
            substances_[sub->name()] = sub;
        }

        std::shared_ptr<Substance> get(const std::string &name) const
        {
            auto it = substances_.find(name);
            return (it != substances_.end()) ? it->second : nullptr;
        }
    };

} // namespace cones

#endif
