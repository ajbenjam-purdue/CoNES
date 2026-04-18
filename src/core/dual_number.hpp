#ifndef CONES_CORE_DUAL_NUMBER_HPP
#define CONES_CORE_DUAL_NUMBER_HPP

#include <cmath>

namespace cones
{

    /**
     * @brief Simple Dual Number for Forward-Mode Automatic Differentiation.
     * Represented as: v + de, where e^2 = 0.
     */
    struct DualNumber
    {
        double val; // Value of the function f(x)
        double der; // Derivative f'(x)

        DualNumber(double v = 0.0, double d = 0.0) : val(v), der(d) {}

        // Operator overloading for basic arithmetic
        DualNumber operator+(const DualNumber &other) const
        {
            return {val + other.val, der + other.der};
        }

        DualNumber operator-(const DualNumber &other) const
        {
            return {val - other.val, der - other.der};
        }

        DualNumber operator*(const DualNumber &other) const
        {
            // Product Rule (uv)' = u'v + uv'
            return {val * other.val, der * other.val + val * other.der};
        }

        DualNumber operator/(const DualNumber &other) const
        {
            // Quotient Rule (u/v)' = (u'v - uv') / v²
            return {val / other.val, (der * other.val - val * other.der) / (other.val * other.val)};
        }

        // Scalar operations
        DualNumber operator+(double s) const { return {val + s, der}; }
        DualNumber operator-(double s) const { return {val - s, der}; }
        DualNumber operator*(double s) const { return {val * s, der * s}; }
        DualNumber operator/(double s) const { return {val / s, der / s}; }

        // Unary minus (negation)
        DualNumber operator-() const { return {-val, -der}; }

        DualNumber pow(double p) const
        {
            return {std::pow(val, p), p * std::pow(val, p - 1) * der};
        }

        friend DualNumber operator+(double s, const DualNumber &d) { return d + s; }
        friend DualNumber operator-(double s, const DualNumber &d) { return {s - d.val, -d.der}; }
        friend DualNumber operator*(double s, const DualNumber &d) { return d * s; }
        friend DualNumber operator/(double s, const DualNumber &d)
        {
            return {s / d.val, -s * d.der / (d.val * d.val)};
        }
    };

    // Common math functions for Dual Numbers
    inline DualNumber pow(const DualNumber &d, double p)
    {
        // (u^p)' = p * u^(p-1) * u'
        return {std::pow(d.val, p), p * std::pow(d.val, p - 1) * d.der};
    }

    inline DualNumber sqrt(const DualNumber &d)
    {
        double s = std::sqrt(d.val);
        return {s, 0.5 * d.der / s};
    }

    inline DualNumber sin(const DualNumber &d)
    {
        return {std::sin(d.val), std::cos(d.val) * d.der};
    }

    inline DualNumber cos(const DualNumber &d)
    {
        return {std::cos(d.val), -std::sin(d.val) * d.der};
    }

    inline DualNumber tan(const DualNumber &d)
    {
        double t = std::tan(d.val);
        double c = std::cos(d.val);
        return {t, d.der / (c * c)};
    }

    inline DualNumber exp(const DualNumber &d)
    {
        double ev = std::exp(d.val);
        return {ev, ev * d.der};
    }

    inline DualNumber log(const DualNumber &d)
    {
        return {std::log(d.val), d.der / d.val};
    }

} // namespace cones

#endif // CONES_CORE_DUAL_NUMBER_HPP
