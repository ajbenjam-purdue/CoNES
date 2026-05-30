#ifndef CONES_CORE_DUAL_NUMBER_HPP
#define CONES_CORE_DUAL_NUMBER_HPP

#include <cmath>
#include <vector>
#include <Eigen/Dense>

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

        // Operator overloading for basic arithmetic (DN to DN)
        DualNumber operator+(const DualNumber &other) const { return {val + other.val, der + other.der}; }
        DualNumber operator-(const DualNumber &other) const { return {val - other.val, der - other.der}; }
        DualNumber operator*(const DualNumber &other) const { return {val * other.val, der * other.val + val * other.der}; }
        DualNumber operator/(const DualNumber &other) const { return {val / other.val, (der * other.val - val * other.der) / (other.val * other.val)}; }

        // Operator overloading for basic arithmetic (DN to const)
        DualNumber operator+(double s) const { return {val + s, der}; }
        DualNumber operator-(double s) const { return {val - s, der}; }
        DualNumber operator*(double s) const { return {val * s, der * s}; }
        DualNumber operator/(double s) const { return {val / s, der / s}; }
        friend DualNumber operator+(double s, const DualNumber &d) { return d + s; }
        friend DualNumber operator-(double s, const DualNumber &d) { return {s - d.val, -d.der}; }
        friend DualNumber operator*(double s, const DualNumber &d) { return d * s; }
        friend DualNumber operator/(double s, const DualNumber &d) { return {s / d.val, -s * d.der / (d.val * d.val)}; }

        // Invert
        DualNumber operator-() const { return {-val, -der}; }

        // Power
        DualNumber pow(double p) const { return {std::pow(val, p), p * std::pow(val, p - 1) * der}; }
    };

    /**
     * @brief Dual Number carrying a vector of derivatives for efficient Jacobian computation.
     */
    struct DualRow {
        double val;
        Eigen::VectorXd der;

        DualRow(double v = 0.0) : val(v) {}
        DualRow(double v, int size) : val(v), der(Eigen::VectorXd::Zero(size)) {}
        DualRow(double v, Eigen::VectorXd d) : val(v), der(std::move(d)) {}

        DualRow operator+(const DualRow &other) const { return {val + other.val, der + other.der}; }
        DualRow operator-(const DualRow &other) const { return {val - other.val, der - other.der}; }
        DualRow operator*(const DualRow &other) const { return {val * other.val, der * other.val + val * other.der}; }
        DualRow operator/(const DualRow &other) const { return {val / other.val, (der * other.val - val * other.der) / (other.val * other.val)}; }

        DualRow operator+(double s) const { return {val + s, der}; }
        DualRow operator-(double s) const { return {val - s, der}; }
        DualRow operator*(double s) const { return {val * s, der * s}; }
        DualRow operator/(double s) const { return {val / s, der / s}; }

        DualRow operator-() const { return {-val, -der}; }

        friend DualRow operator+(double s, const DualRow &d) { return d + s; }
        friend DualRow operator-(double s, const DualRow &d) { return {s - d.val, -d.der}; }
        friend DualRow operator*(double s, const DualRow &d) { return d * s; }
        friend DualRow operator/(double s, const DualRow &d) { return {s / d.val, -s * d.der / (d.val * d.val)}; }
    };

    // Math functions for DualNumber
    inline DualNumber sin(const DualNumber &d) { return {std::sin(d.val), std::cos(d.val) * d.der}; }
    inline DualNumber cos(const DualNumber &d) { return {std::cos(d.val), -std::sin(d.val) * d.der}; }
    inline DualNumber tan(const DualNumber &d) { double t = std::tan(d.val); double c = std::cos(d.val); return {t, d.der / (c * c)}; }
    inline DualNumber asin(const DualNumber &d) { return {std::asin(d.val), d.der / std::sqrt(1.0 - d.val * d.val)}; }
    inline DualNumber acos(const DualNumber &d) { return {std::acos(d.val), -d.der / std::sqrt(1.0 - d.val * d.val)}; }
    inline DualNumber atan(const DualNumber &d) { return {std::atan(d.val), d.der / (1.0 + d.val * d.val)}; }
    inline DualNumber sinh(const DualNumber &d) { return {std::sinh(d.val), std::cosh(d.val) * d.der}; }
    inline DualNumber cosh(const DualNumber &d) { return {std::cosh(d.val), std::sinh(d.val) * d.der}; }
    inline DualNumber tanh(const DualNumber &d) { double t = std::tanh(d.val); double ch = std::cosh(d.val); return {t, d.der / (ch * ch)}; }
    inline DualNumber exp(const DualNumber &d) { double ev = std::exp(d.val); return {ev, ev * d.der}; }
    inline DualNumber log(const DualNumber &d) { return {std::log(d.val), d.der / d.val}; }
    inline DualNumber log10(const DualNumber &d) { return {std::log10(d.val), d.der / (d.val * std::log(10.0))}; }
    inline DualNumber sqrt(const DualNumber &d) { double v = std::sqrt(d.val); return {v, 0.5 * d.der / (v + 1e-15)}; }
    inline DualNumber abs(const DualNumber &d) { return {std::abs(d.val), (d.val >= 0 ? 1.0 : -1.0) * d.der}; }
    inline DualNumber pow(const DualNumber &d, double p) { return {std::pow(d.val, p), p * std::pow(d.val, p - 1.0) * d.der}; }

    // Math functions for DualRow
    inline DualRow sin(const DualRow &d) { return {std::sin(d.val), std::cos(d.val) * d.der}; }
    inline DualRow cos(const DualRow &d) { return {std::cos(d.val), -std::sin(d.val) * d.der}; }
    inline DualRow tan(const DualRow &d) { double t = std::tan(d.val); double c = std::cos(d.val); return {t, d.der / (c * c)}; }
    inline DualRow asin(const DualRow &d) { return {std::asin(d.val), d.der / std::sqrt(1.0 - d.val * d.val)}; }
    inline DualRow acos(const DualRow &d) { return {std::acos(d.val), -d.der / std::sqrt(1.0 - d.val * d.val)}; }
    inline DualRow atan(const DualRow &d) { return {std::atan(d.val), d.der / (1.0 + d.val * d.val)}; }
    inline DualRow sinh(const DualRow &d) { return {std::sinh(d.val), std::cosh(d.val) * d.der}; }
    inline DualRow cosh(const DualRow &d) { return {std::cosh(d.val), std::sinh(d.val) * d.der}; }
    inline DualRow tanh(const DualRow &d) { double t = std::tanh(d.val); double ch = std::cosh(d.val); return {t, d.der / (ch * ch)}; }
    inline DualRow exp(const DualRow &d) { double ev = std::exp(d.val); return {ev, ev * d.der}; }
    inline DualRow log(const DualRow &d) { return {std::log(d.val), d.der / d.val}; }
    inline DualRow log10(const DualRow &d) { return {std::log10(d.val), d.der / (d.val * std::log(10.0))}; }
    inline DualRow sqrt(const DualRow &d) { double v = std::sqrt(d.val); return {v, 0.5 * d.der / (v + 1e-15)}; }
    inline DualRow abs(const DualRow &d) { return {std::abs(d.val), (d.val >= 0 ? 1.0 : -1.0) * d.der}; }
    inline DualRow pow(const DualRow &d, double p) { return {std::pow(d.val, p), p * std::pow(d.val, p - 1.0) * d.der}; }

} // namespace cones

#endif // CONES_CORE_DUAL_NUMBER_HPP
