#ifndef CONES_CORE_EXPRESSION_HPP
#define CONES_CORE_EXPRESSION_HPP

#include "dual_number.hpp"
#include "function_registry.hpp"
#include "unit.hpp"
#include "variable_registry.hpp"
#include "../lang/definition_registry.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
        virtual DualNumber evaluate(const std::vector<DualNumber> &values, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *local_scope = nullptr) const = 0;
        virtual DualRow evaluate_row(const std::vector<DualRow> &values, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *local_scope = nullptr) const = 0;
        virtual std::string to_string() const = 0;
        virtual Unit get_unit(const VariableRegistry &reg) const = 0;
        virtual void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const = 0;
    };

    using NodePtr = std::shared_ptr<Node>;

    class UnitCastNode : public Node
    {
        NodePtr child_;
        Unit from_unit_;
        Unit to_unit_;
        std::string unit_name_;

    public:
        UnitCastNode(NodePtr c, Unit from, Unit to, std::string name = "")
            : child_(std::move(c)), from_unit_(from), to_unit_(to), unit_name_(std::move(name)) {}

        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override
        {
            DualNumber val = child_->evaluate(v, reg, ls);
            if (from_unit_.is_dimensionless())
            {
                double si_val = (val.val + to_unit_.offset) * to_unit_.scale;
                double si_der = val.der * to_unit_.scale;
                return {si_val, si_der};
            }
            return val;
        }

        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override
        {
            DualRow val = child_->evaluate_row(v, reg, ls);
            if (from_unit_.is_dimensionless())
            {
                double si_val = (val.val + to_unit_.offset) * to_unit_.scale;
                Eigen::VectorXd si_der = val.der * to_unit_.scale;
                return {si_val, si_der};
            }
            return val;
        }

        std::string to_string() const override { return child_->to_string() + " [" + (unit_name_.empty() ? to_unit_.to_string() : unit_name_) + "]"; }
        Unit get_unit(const VariableRegistry &) const override { return to_unit_; }
        std::string get_unit_name() const { return unit_name_; }
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override { child_->collect_active_variables(vars, reg); }
    };

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
        CustomFunctionNode(std::shared_ptr<IFunction> f, std::vector<NodeArg> a) : func_(f), args_(a) {}
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override;
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override;
        std::string to_string() const override {
            std::string s = func_->name() + "(";
            for (size_t i = 0; i < args_.size(); ++i) {
                if (i > 0) s += ",";
                if (!args_[i].name.empty()) s += args_[i].name + "=";
                s += args_[i].node->to_string();
            }
            s += ")";
            return s;
        }
        Unit get_unit(const VariableRegistry &reg) const override
        {
            std::vector<Unit> units;
            for (const auto &arg : args_)
                units.push_back(arg.node->get_unit(reg));
            return func_->get_unit(units);
        }
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override
        {
            for (const auto &arg : args_)
                arg.node->collect_active_variables(vars, reg);
        }
    };

    class UserFunctionNode : public Node
    {
        const FunctionDef *def_;
        std::vector<NodePtr> arg_nodes_;

    public:
        UserFunctionNode(const FunctionDef *d, std::vector<NodePtr> a) : def_(d), arg_nodes_(std::move(a)) {}
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override;
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override;
        std::string to_string() const override {
            std::string s = def_->name + "(";
            for (size_t i = 0; i < arg_nodes_.size(); ++i) {
                if (i > 0) s += ",";
                s += arg_nodes_[i]->to_string();
            }
            s += ")";
            return s;
        }
        Unit get_unit(const VariableRegistry &) const override { return def_->return_unit; }
        std::string get_unit_name() const { return def_->return_unit_name; }
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override
        {
            for (const auto &arg : arg_nodes_)
                arg->collect_active_variables(vars, reg);
            for (const auto &assign : def_->body_assignments)
                assign.rhs_node->collect_active_variables(vars, reg);
            def_->return_node->collect_active_variables(vars, reg);
        }
    };

    class ConstantNode : public Node
    {
        double v_;

    public:
        explicit ConstantNode(double v) : v_(v) {}
        DualNumber evaluate(const std::vector<DualNumber> &, const VariableRegistry &, const std::unordered_map<std::string, DualNumber> *) const override { return {v_, 0.0}; }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &, const std::unordered_map<std::string, DualRow> *) const override
        {
            return {v_, (int)(v.empty() ? 0 : v[0].der.size())};
        }
        std::string to_string() const override { return std::to_string(v_); }
        Unit get_unit(const VariableRegistry &) const override { return Unit::Dimensionless(); }
        void collect_active_variables(std::unordered_set<int> &, const VariableRegistry &) const override {}
    };

    class LocalVariableNode : public Node
    {
        std::string name_;

    public:
        explicit LocalVariableNode(std::string n) : name_(std::move(n)) {}
        DualNumber evaluate(const std::vector<DualNumber> &, const VariableRegistry &, const std::unordered_map<std::string, DualNumber> *ls) const override
        {
            if (ls)
            {
                auto it = ls->find(name_);
                if (it != ls->end())
                    return it->second;
            }
            return {0.0, 0.0};
        }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &, const std::unordered_map<std::string, DualRow> *ls) const override
        {
            if (ls)
            {
                auto it = ls->find(name_);
                if (it != ls->end())
                    return it->second;
            }
            return {0.0, (int)(v.empty() ? 0 : v[0].der.size())};
        }
        std::string to_string() const override { return name_; }
        Unit get_unit(const VariableRegistry &) const override { return Unit::Dimensionless(); }
        void collect_active_variables(std::unordered_set<int> &, const VariableRegistry &) const override {}
    };

    class VariableNode : public Node
    {
        int idx_;
        std::string n_;

    public:
        VariableNode(int i, std::string n) : idx_(i), n_(n) {}
        int get_index() const { return idx_; }
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override;
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override;
        std::string to_string() const override { return n_; }
        Unit get_unit(const VariableRegistry &reg) const override;
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override
        {
            if (reg.is_active(idx_))
                vars.insert(idx_);
        }
    };

    class UnaryNode : public Node
    {
    protected:
        NodePtr c_;

    public:
        explicit UnaryNode(NodePtr c) : c_(c) {}
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override
        {
            c_->collect_active_variables(vars, reg);
        }
    };
    class BinaryNode : public Node
    {
    protected:
        NodePtr l_, r_;

    public:
        BinaryNode(NodePtr l, NodePtr r) : l_(l), r_(r) {}
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override
        {
            l_->collect_active_variables(vars, reg);
            r_->collect_active_variables(vars, reg);
        }
    };

    class AddNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override { return l_->evaluate(v, r, ls) + r_->evaluate(v, r, ls); }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override { return l_->evaluate_row(v, r, ls) + r_->evaluate_row(v, r, ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "+" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry &reg) const override;
    };
    class SubNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override { return l_->evaluate(v, r, ls) - r_->evaluate(v, r, ls); }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override { return l_->evaluate_row(v, r, ls) - r_->evaluate_row(v, r, ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "-" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry &reg) const override;
    };
    class MulNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override { return l_->evaluate(v, r, ls) * r_->evaluate(v, r, ls); }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override { return l_->evaluate_row(v, r, ls) * r_->evaluate_row(v, r, ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "*" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry &reg) const override;
    };
    class DivNode : public BinaryNode
    {
    public:
        using BinaryNode::BinaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override { return l_->evaluate(v, r, ls) / r_->evaluate(v, r, ls); }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override { return l_->evaluate_row(v, r, ls) / r_->evaluate_row(v, r, ls); }
        std::string to_string() const override { return "(" + l_->to_string() + "/" + r_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry &reg) const override;
    };
    class PowNode : public Node
    {
        NodePtr b_;
        double e_;

    public:
        PowNode(NodePtr b, double e) : b_(b), e_(e) {}
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override { return pow(b_->evaluate(v, r, ls), e_); }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override { return pow(b_->evaluate_row(v, r, ls), e_); }
        std::string to_string() const override { return "pow(" + b_->to_string() + "," + std::to_string(e_) + ")"; }
        Unit get_unit(const VariableRegistry &reg) const override;
        void collect_active_variables(std::unordered_set<int> &vars, const VariableRegistry &reg) const override
        {
            b_->collect_active_variables(vars, reg);
        }
    };
    class NegNode : public UnaryNode
    {
    public:
        using UnaryNode::UnaryNode;
        DualNumber evaluate(const std::vector<DualNumber> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualNumber> *ls = nullptr) const override { return -c_->evaluate(v, r, ls); }
        DualRow evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &r, const std::unordered_map<std::string, DualRow> *ls = nullptr) const override { return -c_->evaluate_row(v, r, ls); }
        std::string to_string() const override { return "(-" + c_->to_string() + ")"; }
        Unit get_unit(const VariableRegistry &reg) const override;
    };

    // Evaluate a variable node and yield a DN
    inline DualNumber VariableNode::evaluate(const std::vector<DualNumber> &v, const VariableRegistry &, const std::unordered_map<std::string, DualNumber> *ls) const
    {

        // First try to find the node in the registry, and return its result
        if (ls)
        {
            auto it = ls->find(n_);
            if (it != ls->end())
                return it->second;
        }

        // Otherwise return the actual DN's value
        return v[idx_];
    }

    // Clone of DualNumber method
    inline DualRow VariableNode::evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &, const std::unordered_map<std::string, DualRow> *ls) const
    {

        // First try to find the node in the registry, and return its result
        if (ls)
        {
            auto it = ls->find(n_);
            if (it != ls->end())
                return it->second;
        }

        // Otherwise return the actual DN's value
        return v[idx_];
    }
    inline Unit VariableNode::get_unit(const VariableRegistry &reg) const { return reg.get_variable(idx_).unit.to_si(); }
    inline Unit AddNode::get_unit(const VariableRegistry &reg) const { return l_->get_unit(reg); }
    inline Unit SubNode::get_unit(const VariableRegistry &reg) const { return l_->get_unit(reg); }
    inline Unit MulNode::get_unit(const VariableRegistry &reg) const { return l_->get_unit(reg) * r_->get_unit(reg); }
    inline Unit DivNode::get_unit(const VariableRegistry &reg) const { return l_->get_unit(reg) / r_->get_unit(reg); }
    inline Unit PowNode::get_unit(const VariableRegistry &reg) const { return b_->get_unit(reg).pow(e_); }
    inline Unit NegNode::get_unit(const VariableRegistry &reg) const { return c_->get_unit(reg); }

    // Evaluate a custom function and provide a dualnumber as output
    inline DualNumber CustomFunctionNode::evaluate(const std::vector<DualNumber> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *ls) const
    {
        std::vector<FuncArg> eval_args;

        // Bind and repack function's args
        for (const auto &arg : args_)
        {
            std::string effective_name = arg.name;
            if (effective_name.empty())
            {
                if (auto vnode = std::dynamic_pointer_cast<VariableNode>(arg.node))
                {
                    effective_name = vnode->to_string();
                }
            }
            eval_args.push_back({effective_name, arg.node->evaluate(v, reg, ls), arg.node->get_unit(reg)});
        }

        // Evaluate the new args
        return func_->evaluate(eval_args);
    }

    // Clone of DualNumber method for DualRow
    inline DualRow CustomFunctionNode::evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *ls) const
    {
        std::vector<FuncArgRow> eval_args;

        // Bind and repack function's args
        for (const auto &arg : args_)
        {
            std::string effective_name = arg.name;
            if (effective_name.empty())
            {
                if (auto vnode = std::dynamic_pointer_cast<VariableNode>(arg.node))
                {
                    effective_name = vnode->to_string();
                }
            }
            eval_args.push_back({effective_name, arg.node->evaluate_row(v, reg, ls), arg.node->get_unit(reg)});
        }

        // Evaluate the new args
        return func_->evaluate_row(eval_args);
    }

    // Evaluate a user function and provide a dualnumber as output
    inline DualNumber UserFunctionNode::evaluate(const std::vector<DualNumber> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualNumber> *) const
    {
        std::unordered_map<std::string, DualNumber> local_scope;

        // Bind parameters
        for (size_t i = 0; i < def_->params.size(); ++i)
        {
            if (i < arg_nodes_.size())
            {
                local_scope[def_->params[i]] = arg_nodes_[i]->evaluate(v, reg);
            }
        }

        // Evaluate body assignments
        for (const auto &assign : def_->body_assignments)
        {
            local_scope[assign.lhs_name] = assign.rhs_node->evaluate(v, reg, &local_scope);
        }

        // Return final result
        return def_->return_node->evaluate(v, reg, &local_scope);
    }

    // Clone of DualNumber method for DualRow
    inline DualRow UserFunctionNode::evaluate_row(const std::vector<DualRow> &v, const VariableRegistry &reg, const std::unordered_map<std::string, DualRow> *) const
    {
        std::unordered_map<std::string, DualRow> local_scope;

        // Bind parameters
        for (size_t i = 0; i < def_->params.size(); ++i)
        {
            if (i < arg_nodes_.size())
            {
                local_scope[def_->params[i]] = arg_nodes_[i]->evaluate_row(v, reg);
            }
        }

        // Evaluate body assignments
        for (const auto &assign : def_->body_assignments)
        {
            local_scope[assign.lhs_name] = assign.rhs_node->evaluate_row(v, reg, &local_scope);
        }

        // Return result
        return def_->return_node->evaluate_row(v, reg, &local_scope);
    }
} // Namespace cones

#endif
