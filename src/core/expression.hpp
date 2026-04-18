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
    class VariableRegistry;

    class Node
    {
    public:
        virtual ~Node() = default;
        virtual DualNumber evaluate(const std::vector<DualNumber> &values, const VariableRegistry& reg) const = 0;
        virtual std::string to_string() const = 0;
        virtual Unit get_unit(const VariableRegistry& reg) const = 0;
    };

    using NodePtr = std::shared_ptr<Node>;

    class UnitCastNode : public Node {
        NodePtr child_;
        Unit from_unit_;
        Unit to_unit_;
    public:
        UnitCastNode(NodePtr c, Unit from, Unit to) 
            : child_(std::move(c)), from_unit_(from), to_unit_(to) {}

        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& reg) const override {
            DualNumber val = child_->evaluate(v, reg);
            double base_val = (val.val + from_unit_.offset) * from_unit_.scale;
            double target_val = (base_val / to_unit_.scale) - to_unit_.offset;
            double target_der = (val.der * from_unit_.scale) / to_unit_.scale;
            return {target_val, target_der};
        }

        std::string to_string() const override { return child_->to_string() + " [" + to_unit_.to_string() + "]"; }
        Unit get_unit(const VariableRegistry&) const override { return to_unit_; }
    };

    struct NodeArg { std::string name; NodePtr node; };

    class CustomFunctionNode : public Node
    {
        std::shared_ptr<IFunction> func_;
        std::vector<NodeArg> args_;
    public:
        CustomFunctionNode(std::shared_ptr<IFunction> f, std::vector<NodeArg> a) : func_(f), args_(a) {}
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry& reg) const override {
            std::vector<FuncArg> eval_args;
            for (const auto &arg : args_) {
                eval_args.push_back({arg.name, arg.node->evaluate(v, reg), arg.node->get_unit(reg)});
            }
            return func_->evaluate(eval_args);
        }
        std::string to_string() const override { return func_->name() + "(...)"; }
        Unit get_unit(const VariableRegistry& reg) const override {
            std::vector<Unit> units;
            for (const auto& arg : args_) units.push_back(arg.node->get_unit(reg));
            return func_->get_unit(units);
        }
    };

    class ConstantNode : public Node {
        double v_;
    public:
        explicit ConstantNode(double v) : v_(v) {}
        DualNumber evaluate(const std::vector<DualNumber>&, const VariableRegistry&) const override { return {v_, 0.0}; }
        std::string to_string() const override { return std::to_string(v_); }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class VariableNode : public Node {
        int idx_; std::string n_;
    public:
        VariableNode(int i, std::string n) : idx_(i), n_(n) {}
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry&) const override { return v[idx_]; }
        std::string to_string() const override { return n_; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class UnaryNode : public Node { protected: NodePtr c_; public: explicit UnaryNode(NodePtr c) : c_(c) {} };
    class BinaryNode : public Node { protected: NodePtr l_, r_; public: BinaryNode(NodePtr l, NodePtr r) : l_(l), r_(r) {} };

    class AddNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r) const override { return l_->evaluate(v,r) + r_->evaluate(v,r); }
        std::string to_string() const override { return "(" + l_->to_string() + "+" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class SubNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r) const override { return l_->evaluate(v,r) - r_->evaluate(v,r); }
        std::string to_string() const override { return "(" + l_->to_string() + "-" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class MulNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r) const override { return l_->evaluate(v,r) * r_->evaluate(v,r); }
        std::string to_string() const override { return "(" + l_->to_string() + "*" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class DivNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r) const override { return l_->evaluate(v,r) / r_->evaluate(v,r); }
        std::string to_string() const override { return "(" + l_->to_string() + "/" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class PowNode : public Node { NodePtr b_; double e_; public: PowNode(NodePtr b, double e) : b_(b), e_(e) {}
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r) const override { return pow(b_->evaluate(v,r), e_); }
        std::string to_string() const override { return "pow(" + b_->to_string() + "," + std::to_string(e_) + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class NegNode : public UnaryNode { public: using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r) const override { return -c_->evaluate(v,r); }
        std::string to_string() const override { return "(-" + c_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

} // namespace cones

#include "variable_registry.hpp"

namespace cones {
    inline Unit VariableNode::get_unit(const VariableRegistry& reg) const { return reg.get_variable(idx_).unit; }
    inline Unit AddNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg); }
    inline Unit SubNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg); }
    inline Unit MulNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg) * r_->get_unit(reg); }
    inline Unit DivNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg) / r_->get_unit(reg); }
    inline Unit PowNode::get_unit(const VariableRegistry& reg) const { return b_->get_unit(reg).pow(e_); }
    inline Unit NegNode::get_unit(const VariableRegistry& reg) const { return c_->get_unit(reg); }
}

#endif
