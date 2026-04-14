#ifndef CONES_CORE_EXPRESSION_HPP
#define CONES_CORE_EXPRESSION_HPP

#include "dual_number.hpp"
#include <memory>
#include <vector>
#include <string>

namespace cones
{

    /**
     * @brief Abstract Base Class for Expression AST Nodes.
     */
    class Node
    {
    public:
        virtual ~Node() = default;
        virtual DualNumber evaluate(const std::vector<DualNumber> &values) const = 0;
        virtual std::string to_string() const = 0;
    };

    using NodePtr = std::shared_ptr<Node>;

    // --- Leaf Nodes ---

    class ConstantNode : public Node
    {
        double value_;

    public:
        explicit ConstantNode(double v) : value_(v) {}
        DualNumber evaluate(const std::vector<DualNumber> &) const override { return {value_, 0.0}; }
        std::string to_string() const override { return std::to_string(value_); }
    };

    class VariableNode : public Node
    {
        int index_;
        std::string name_;

    public:
        VariableNode(int idx, std::string name) : index_(idx), name_(std::move(name)) {}
        DualNumber evaluate(const std::vector<DualNumber> &values) const override { return values[index_]; }
        std::string to_string() const override { return name_; }
    };

    // --- Base Classes for Modularity ---

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
    };

    class SubNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) - right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " - " + right_->to_string() + ")"; }
    };

    class MulNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) * right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " * " + right_->to_string() + ")"; }
    };

    class DivNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return left_->evaluate(v) / right_->evaluate(v); }
        std::string to_string() const override { return "(" + left_->to_string() + " / " + right_->to_string() + ")"; }
    };

    class PowNode : public Node
    {
        NodePtr base_;
        double exponent_; // Simple version: power by constant
    public:
        PowNode(NodePtr b, double e) : base_(std::move(b)), exponent_(e) {}
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return pow(base_->evaluate(v), exponent_); }
        std::string to_string() const override { return "pow(" + base_->to_string() + ", " + std::to_string(exponent_) + ")"; }
    };

    // --- Unary Operations & Trig ---

    class NegNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return -child_->evaluate(v); }
        std::string to_string() const override { return "(-" + child_->to_string() + ")"; }
    };

    class SinNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return sin(child_->evaluate(v)); }
        std::string to_string() const override { return "sin(" + child_->to_string() + ")"; }
    };

    class CosNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return cos(child_->evaluate(v)); }
        std::string to_string() const override { return "cos(" + child_->to_string() + ")"; }
    };

    class TanNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return tan(child_->evaluate(v)); }
        std::string to_string() const override { return "tan(" + child_->to_string() + ")"; }
    };

    class LogNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return log(child_->evaluate(v)); }
        std::string to_string() const override { return "log(" + child_->to_string() + ")"; }
    };

    class ExpNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v) const override { return exp(child_->evaluate(v)); }
        std::string to_string() const override { return "exp(" + child_->to_string() + ")"; }
    };

} // namespace cones

#endif // CONES_CORE_EXPRESSION_HPP
