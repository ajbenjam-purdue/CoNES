#ifndef CONES_CORE_EXPRESSION_HPP
#define CONES_CORE_EXPRESSION_HPP

#include "dual_number.hpp"
#include "function_registry.hpp"
#include "unit.hpp"
#include "../lang/definition_registry.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace cones
{
    class VariableRegistry;

    class Node
    {
        int line_ = -1;
    public:
        virtual ~Node() = default;
        void set_line(int l) { line_ = l; }
        int get_line() const { return line_; }
        virtual DualNumber evaluate(const std::vector<DualNumber> &values, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>* local_scope = nullptr) const = 0;
        virtual std::string to_string() const = 0;
        virtual Unit get_unit(const VariableRegistry& reg) const = 0;
    };

    using NodePtr = std::shared_ptr<Node>;

    class UnitCastNode : public Node {
        NodePtr child_;
        Unit from_unit_;
        Unit to_unit_;
        std::string unit_name_;
    public:
        UnitCastNode(NodePtr c, Unit from, Unit to, std::string name = "") 
            : child_(std::move(c)), from_unit_(from), to_unit_(to), unit_name_(std::move(name)) {}

        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override {
            DualNumber val = child_->evaluate(v, reg, ls);
            if (from_unit_.is_dimensionless()) {
                double si_val = (val.val + to_unit_.offset) * to_unit_.scale;
                double si_der = val.der * to_unit_.scale;
                return {si_val, si_der};
            }
            return val;
        }

        std::string to_string() const override { return child_->to_string() + " [" + (unit_name_.empty() ? to_unit_.to_string() : unit_name_) + "]"; }
        Unit get_unit(const VariableRegistry&) const override { return to_unit_; }
        std::string get_unit_name() const { return unit_name_; }
    };

    struct NodeArg { std::string name; NodePtr node; };

    class CustomFunctionNode : public Node
    {
        std::shared_ptr<IFunction> func_;
        std::vector<NodeArg> args_;
    public:
        CustomFunctionNode(std::shared_ptr<IFunction> f, std::vector<NodeArg> a) : func_(f), args_(a) {}
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override;
        std::string to_string() const override { return func_->name() + "(...)"; }
        Unit get_unit(const VariableRegistry& reg) const override {
            std::vector<Unit> units;
            for (const auto& arg : args_) units.push_back(arg.node->get_unit(reg));
            return func_->get_unit(units);
        }
    };

    class UserFunctionNode : public Node {
        const FunctionDef* def_;
        std::vector<NodePtr> arg_nodes_;
    public:
        UserFunctionNode(const FunctionDef* d, std::vector<NodePtr> a) : def_(d), arg_nodes_(std::move(a)) {}
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override;
        std::string to_string() const override { return def_->name + "(...)"; }
        Unit get_unit(const VariableRegistry&) const override { return def_->return_unit; }
        std::string get_unit_name() const { return def_->return_unit_name; }
    };

    class ConstantNode : public Node {
        double v_;
    public:
        explicit ConstantNode(double v) : v_(v) {}
        DualNumber evaluate(const std::vector<DualNumber>&, const VariableRegistry&, const std::unordered_map<std::string, DualNumber>*) const override { return {v_, 0.0}; }
        std::string to_string() const override { return std::to_string(v_); }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class LocalVariableNode : public Node {
        std::string name_;
    public:
        explicit LocalVariableNode(std::string n) : name_(std::move(n)) {}
        DualNumber evaluate(const std::vector<DualNumber>&, const VariableRegistry&, const std::unordered_map<std::string, DualNumber>* ls) const override {
            if (ls) {
                auto it = ls->find(name_);
                if (it != ls->end()) return it->second;
            }
            return {0.0, 0.0};
        }
        std::string to_string() const override { return name_; }
        Unit get_unit(const VariableRegistry&) const override { return Unit::Dimensionless(); }
    };

    class VariableNode : public Node {
        int idx_; std::string n_;
    public:
        VariableNode(int i, std::string n) : idx_(i), n_(n) {}
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override;
        std::string to_string() const override { return n_; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

    class UnaryNode : public Node { protected: NodePtr c_; public: explicit UnaryNode(NodePtr c) : c_(c) {} };
    class BinaryNode : public Node { protected: NodePtr l_, r_; public: BinaryNode(NodePtr l, NodePtr r) : l_(l), r_(r) {} };

    class AddNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override { return l_->evaluate(v,r,ls) + r_->evaluate(v,r,ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "+" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class SubNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override { return l_->evaluate(v,r,ls) - r_->evaluate(v,r,ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "-" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class MulNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override { return l_->evaluate(v,r,ls) * r_->evaluate(v,r,ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "*" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class DivNode : public BinaryNode { public: using BinaryNode::BinaryNode; 
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override { return l_->evaluate(v,r,ls) / r_->evaluate(v,r,ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "/" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class PowNode : public Node { NodePtr b_; double e_; public: PowNode(NodePtr b, double e) : b_(b), e_(e) {}
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override { return pow(b_->evaluate(v,r,ls), e_); }
        std::string to_string() const override { return "pow(" + b_->to_string() + "," + std::to_string(e_) + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };
    class NegNode : public UnaryNode { public: using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber>& v, const VariableRegistry& r, const std::unordered_map<std::string, DualNumber>* ls = nullptr) const override { return -c_->evaluate(v,r,ls); }
        std::string to_string() const override { return "(-" + c_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry& reg) const override;
    };

} // namespace cones

#include "variable_registry.hpp"

namespace cones {
    inline DualNumber VariableNode::evaluate(const std::vector<DualNumber>& v, const VariableRegistry&, const std::unordered_map<std::string, DualNumber>* ls) const { 
        if (ls) {
            auto it = ls->find(n_);
            if (it != ls->end()) return it->second;
        }
        return v[idx_];
    }
    inline Unit VariableNode::get_unit(const VariableRegistry& reg) const { return reg.get_variable(idx_).unit.to_si(); }
    inline Unit AddNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg); }
    inline Unit SubNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg); }
    inline Unit MulNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg) * r_->get_unit(reg); }
    inline Unit DivNode::get_unit(const VariableRegistry& reg) const { return l_->get_unit(reg) / r_->get_unit(reg); }
    inline Unit PowNode::get_unit(const VariableRegistry& reg) const { return b_->get_unit(reg).pow(e_); }
    inline Unit NegNode::get_unit(const VariableRegistry& reg) const { return c_->get_unit(reg); }

    inline DualNumber CustomFunctionNode::evaluate(const std::vector<DualNumber> &v, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>* ls) const {
        std::vector<FuncArg> eval_args;
        for (const auto &arg : args_) {
            std::string effective_name = arg.name;
            if (effective_name.empty()) {
                if (auto vnode = std::dynamic_pointer_cast<VariableNode>(arg.node)) {
                    effective_name = vnode->to_string();
                }
            }
            eval_args.push_back({effective_name, arg.node->evaluate(v, reg, ls), arg.node->get_unit(reg)});
        }
        return func_->evaluate(eval_args);
    }

    inline DualNumber UserFunctionNode::evaluate(const std::vector<DualNumber> &v, const VariableRegistry& reg, const std::unordered_map<std::string, DualNumber>*) const {
        std::unordered_map<std::string, DualNumber> local_scope;

        // 1. Bind parameters
        for (size_t i = 0; i < def_->params.size(); ++i) {
            if (i < arg_nodes_.size()) {
                local_scope[def_->params[i]] = arg_nodes_[i]->evaluate(v, reg);
            }
        }

        // 2. Evaluate body assignments
        for (const auto& assign : def_->body_assignments) {
            local_scope[assign.lhs_name] = assign.rhs_node->evaluate(v, reg, &local_scope);
        }
        
        // 3. Return final result
        return def_->return_node->evaluate(v, reg, &local_scope);
    }
}

#endif
