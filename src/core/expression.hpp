#ifndef CONES_CORE_EXPRESSION_HPP
#define CONES_CORE_EXPRESSION_HPP

#include "dual_number.hpp"
#include "function_registry.hpp"
#include "unit.hpp"
#include <memory>
#include <vector>
#include <string>

namespace cones
{
    class VariableRegistry; // Forward declaration

    /**
     * @brief Abstract Base Class for Expression AST Nodes.
     */
    class Node
    {
    public:
        virtual ~Node() = default;
        virtual DualNumber evaluate(const std::vector<DualNumber> &values) const = 0;
        virtual std::string to_string() const = 0;
        virtual Unit get_unit(const VariableRegistry& reg) const = 0;
    };

    using NodePtr = std::shared_ptr<Node>;

    /**
     * @brief Casts an expression into a different unit.
     */
    class UnitCastNode : public Node {
        NodePtr child_;
        Unit from_unit_;
        Unit to_unit_;
    public:
        UnitCastNode(NodePtr c, Unit from, Unit to) 
            : child_(std::move(c)), from_unit_(from), to_unit_(to) {}

        DualNumber evaluate(const std::vector<DualNumber>& v) const override {
            DualNumber val = child_->evaluate(v);
            double base_val = (val.val + from_unit_.offset) * from_unit_.scale;
            double target_val = (base_val / to_unit_.scale) - to_unit_.offset;
            double target_der = (val.der * from_unit_.scale) / to_unit_.scale;
            return {target_val, target_der};
        }

        std::string to_string() const override { return child_->to_string() + " [" + std::to_string(to_unit_.scale) + "]"; }
        Unit get_unit(const VariableRegistry&) const override { return to_unit_; }
    };

    // --- Custom/Intelligent Functions ---

    struct NodeArg
    {
        std::string name;
        NodePtr node;
    };

    class CustomFunctionNode : public Node
    {
        std::shared_ptr<IFunction> func_;
        std::vector<NodeArg> args_;

    public:
        CustomFunctionNode(std::shared_ptr<IFunction> f, std::vector<NodeArg> a)
            : func_(std::move(f)), args_(std::move(a)) {}

        DualNumber evaluate(const std::vector<DualNumber> &v) const override
        {
            std::vector<FuncArg> eval_args;
            for (const auto &arg : args_)
            {
                eval_args.push_back({arg.name, arg.node->evaluate(v)});
            }
            return func_->evaluate(eval_args);
        }

        std::string to_string() const override
        {
            std::string s = func_->name() + "(";
            for (size_t i = 0; i < args_.size(); ++i)
            {
                if (!args_[i].name.empty())
                    s += args_[i].name + "=";
                s += args_[i].node->to_string();
                if (i < args_.size() - 1)
                    s += ", ";
            }
            return s + ")";
        }

        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    // --- Leaf Nodes ---

    class ConstantNode : public Node
    {
        double value_;
    public:
        explicit ConstantNode(double v) : value_(v) {}
        DualNumber evaluate(const std::vector<DualNumber> &) const override { return {value_, 0.0}; }
        std::string to_string() const override { return std::to_string(value_); }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class VariableNode : public Node
    {
        int index_;
        std::string name_;
    public:
        VariableNode(int idx, std::string name) : index_(idx), name_(std::move(name)) {}
        DualNumber evaluate(const std::vector<DualNumber> &values) const override { return values[index_]; }
        std::string to_string() const override { return name_; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    // --- Base Classes ---

    class UnaryNode : public Node
    {
    protected:
        NodePtr child_;
    public:
        explicit UnaryNode(NodePtr c) : child_(std::move(c)) {}
    };

    class BinaryNode : public Node
    {
    protected:
        NodePtr left_;
        NodePtr right_;
    public:
        BinaryNode(NodePtr l, NodePtr r) : left_(std::move(l)), right_(std::move(r)) {}
    };

    // --- Binary Operations ---

    class AddNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) + right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " + " + right_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class SubNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) - right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " - " + right_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class MulNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) * right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " * " + right_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class DivNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) / right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " / " + right_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class PowNode : public Node
    {
        NodePtr base_;
        double exponent_;
    public:
        PowNode(NodePtr b, double e) : base_(std::move(b)), exponent_(e) {}
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return pow(base_->evaluate(v), exponent_); }
        std::string to_string() const override { return "pow(" + base_->to_string() + ", " + std::to_string(exponent_) + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    // --- Unary ---

    class NegNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return -child_->evaluate(v); }
        std::string to_string() const override { return "(-" + child_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class SinNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return sin(child_->evaluate(v)); }
        std::string to_string() const override { return "sin(" + child_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class CosNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return cos(child_->evaluate(v)); }
        std::string to_string() const override { return "cos(" + child_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class TanNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return tan(child_->evaluate(v)); }
        std::string to_string() const override { return "tan(" + child_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class LogNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return log(child_->evaluate(v)); }
        std::string to_string() const override { return "log(" + child_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class ExpNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return exp(child_->evaluate(v)); }
        std::string to_string() const override { return "exp(" + child_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

} // namespace cones

#include "variable_registry.hpp"

namespace cones {
    inline Unit VariableNode::get_unit(const VariableRegistry& reg) const { return reg.get_variable(index_).unit; }
    inline Unit AddNode::get_unit(const VariableRegistry& reg) const { return left_->get_unit(reg); }
    inline Unit SubNode::get_unit(const VariableRegistry& reg) const { return left_->get_unit(reg); }
    inline Unit MulNode::get_unit(const VariableRegistry& reg) const { return left_->get_unit(reg) * right_->get_unit(reg); }
    inline Unit DivNode::get_unit(const VariableRegistry& reg) const { return left_->get_unit(reg) / right_->get_unit(reg); }
    inline Unit PowNode::get_unit(const VariableRegistry& reg) const { return base_->get_unit(reg).pow(exponent_); }
    inline Unit NegNode::get_unit(const VariableRegistry& reg) const { return child_->get_unit(reg); }
}

#endif
