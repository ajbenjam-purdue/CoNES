#ifndef CONES_CORE_SUBSTANCE_HPP
#define CONES_CORE_SUBSTANCE_HPP

#include "dual_number.hpp"
#include "property_types.hpp"
#include <vector>
#include <string>

namespace cones
{
    // Structs for holding both a type (e.g. Enthalpy) AND a value (num/row)
    struct PropertyArg
    {
        PropertyType type;
        DualNumber value;
    };
    struct PropertyArgRow
    {
        PropertyType type;
        DualRow value;
    };

    /**
     * @brief Abstract base class for all substances (Tabulated, Ideal Gas, etc.)
     */
    class Substance
    {
    public:
        virtual ~Substance() = default;
        virtual std::string name() const = 0;
        virtual std::string summary() const = 0;

        /**
         * @brief The core State Resolver.
         * Given n inputs, returns the target property.
         */
        virtual DualNumber evaluate(PropertyType target, const std::vector<PropertyArg> &inputs) const = 0;
        virtual DualRow evaluate(PropertyType target, const std::vector<PropertyArgRow> &inputs) const = 0;
    };

} // namespace cones

#endif
