#ifndef CONES_SOLVER_BLOCK_DECOMPOSER_HPP
#define CONES_SOLVER_BLOCK_DECOMPOSER_HPP

#include "../core/system.hpp"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include <iostream>

namespace cones
{

    struct SolverBlock
    {
        std::vector<int> variable_indices; // Global registry indices of active variables in this block
        std::vector<int> equation_indices; // Indices of equations in this block
    };

    class BlockDecomposer
    {
    private:
        // DFS for bipartite matching
        static bool find_augmenting_path(int var_idx,
                                         const std::vector<std::vector<int>> &adj,
                                         std::vector<int> &match_eq,
                                         std::vector<bool> &visited)
        {
            for (int eq_idx : adj[var_idx])
            {
                if (visited[eq_idx])
                    continue;
                visited[eq_idx] = true;

                // If eq_idx is not matched, or we can find an augmenting path for its match
                if (match_eq[eq_idx] < 0 || find_augmenting_path(match_eq[eq_idx], adj, match_eq, visited))
                {
                    match_eq[eq_idx] = var_idx;
                    return true;
                }
            }
            return false;
        }

        // Tarjan's SCC DFS helper
        // Inspired by https://www.geeksforgeeks.org/cpp/tarjans-algorithm-in-cpp/
        static void tarjan_dfs(int u,
                               const std::vector<std::vector<int>> &adj_vars,
                               std::vector<int> &index,
                               std::vector<int> &lowlink,
                               std::vector<bool> &on_stack,
                               std::stack<int> &s,
                               int &timer,
                               std::vector<std::vector<int>> &sccs)
        {
            index[u] = lowlink[u] = timer++;
            s.push(u);
            on_stack[u] = true;

            for (int v : adj_vars[u])
            {
                if (index[v] < 0)
                {
                    tarjan_dfs(v, adj_vars, index, lowlink, on_stack, s, timer, sccs);
                    lowlink[u] = std::min(lowlink[u], lowlink[v]);
                }
                else if (on_stack[v])
                {
                    lowlink[u] = std::min(lowlink[u], index[v]);
                }
            }

            if (lowlink[u] == index[u])
            {
                std::vector<int> scc;
                while (true)
                {
                    int v = s.top();
                    s.pop();
                    on_stack[v] = false;
                    scc.push_back(v);
                    if (v == u)
                        break;
                }
                sccs.push_back(scc);
            }
        }

    public:
        static std::vector<SolverBlock> decompose(const System &system, bool verbose = false)
        {
            const auto &equations = system.get_equations();
            const auto &registry = system.registry();
            auto active_indices = registry.get_active_indices();

            int n = static_cast<int>(equations.size());
            int m = static_cast<int>(active_indices.size());

            if (n == 0 || m == 0)
            {
                return {};
            }

            if (n < m)
            {
                if (verbose)
                {
                    std::cout << "BlockDecomposer: Under-determined system (equations " << n
                              << " < variables " << m << "). Falling back to single block." << std::endl;
                }
                std::vector<int> eqs(n);
                for (int i = 0; i < n; ++i)
                    eqs[i] = i;
                return {{active_indices, eqs}};
            }

            // Map global registry index -> local active variable index (0 to m-1)
            std::unordered_map<int, int> global_to_local_var;
            for (int i = 0; i < m; ++i)
            {
                global_to_local_var[active_indices[i]] = i;
            }

            // adj[local_var_idx] = list of equations that depend on this variable
            std::vector<std::vector<int>> adj(m);
            for (int eq_idx = 0; eq_idx < n; ++eq_idx)
            {
                std::unordered_set<int> deps;
                equations[eq_idx]->collect_active_variables(deps, registry);
                for (int global_var_idx : deps)
                {
                    auto it = global_to_local_var.find(global_var_idx);
                    if (it != global_to_local_var.end())
                    {
                        adj[it->second].push_back(eq_idx);
                    }
                }
            }

            // Count active variables per equation and sort matching options to match simplest equations first
            std::vector<int> eq_num_vars(n, 0);
            for (int eq_idx = 0; eq_idx < n; ++eq_idx)
            {
                std::unordered_set<int> deps;
                equations[eq_idx]->collect_active_variables(deps, registry);
                int active_deps = 0;
                for (int d : deps)
                {
                    if (global_to_local_var.count(d))
                        active_deps++;
                }
                eq_num_vars[eq_idx] = active_deps;
            }

            for (int i = 0; i < m; ++i)
            {
                std::sort(adj[i].begin(), adj[i].end(), [&](int eq_a, int eq_b)
                          { return eq_num_vars[eq_a] < eq_num_vars[eq_b]; });
            }

            // Bipartite Matching (pairing local variables with equations)
            // match_eq[eq_idx] = local_var_idx matched to eq_idx
            std::vector<int> match_eq(n, -1);
            int matching_size = 0;
            for (int var_idx = 0; var_idx < m; ++var_idx)
            {
                std::vector<bool> visited(n, false);
                if (find_augmenting_path(var_idx, adj, match_eq, visited))
                {
                    matching_size++;
                }
            }

            if (verbose)
            {
                std::cout << "--- Bipartite Matching ---" << std::endl;
                for (int eq_idx = 0; eq_idx < n; ++eq_idx)
                {
                    int var_idx = match_eq[eq_idx];
                    if (var_idx >= 0)
                    {
                        std::cout << "  Eq " << eq_idx << " (" << equations[eq_idx]->to_string()
                                  << ") matched to Var " << registry.get_variable(active_indices[var_idx]).name << std::endl;
                    }
                }
            }

            if (matching_size < m) // Under-square
            {
                if (verbose)
                {
                    std::cout << "Warning: BlockDecomposer: System is structurally singular (matching size "
                              << matching_size << " < " << m << "). Falling back to single block." << std::endl;
                }
                std::vector<int> eqs(n);
                for (int i = 0; i < n; ++i)
                    eqs[i] = i;
                return {{active_indices, eqs}};
            }

            // eq_to_var[eq_idx] = local_var_idx matched to eq_idx
            // var_to_eq[local_var_idx] = eq_idx matched to local_var_idx
            std::vector<int> var_to_eq(m, -1);
            for (int eq_idx = 0; eq_idx < n; ++eq_idx)
            {
                int var_idx = match_eq[eq_idx];
                if (var_idx >= 0)
                {
                    var_to_eq[var_idx] = eq_idx;
                }
            }

            // Build directed dependency graph between local variables
            // adj_vars[u] = list of local variables that variable u depends on
            // Specifically, var u is matched to eq_u = var_to_eq[u]
            // If eq_u depends on variable v, then solving for u depends on v. So draw directed edge from u to v
            std::vector<std::vector<int>> adj_vars(m);
            for (int u = 0; u < m; ++u)
            {
                int eq_idx = var_to_eq[u];
                std::unordered_set<int> deps;
                equations[eq_idx]->collect_active_variables(deps, registry);
                for (int global_var_idx : deps)
                {
                    auto it = global_to_local_var.find(global_var_idx);
                    if (it != global_to_local_var.end() && it->second != u)
                    {
                        adj_vars[u].push_back(it->second);
                    }
                }
            }

            // Find SCCs of local variables using Tarjan's algorithm
            std::vector<int> index(m, -1);
            std::vector<int> lowlink(m, -1);
            std::vector<bool> on_stack(m, false);
            std::stack<int> s;
            int timer = 0;
            std::vector<std::vector<int>> sccs;

            for (int i = 0; i < m; ++i)
            {
                if (index[i] < 0)
                {
                    tarjan_dfs(i, adj_vars, index, lowlink, on_stack, s, timer, sccs);
                }
            }

            // Construct SolverBlocks
            std::vector<SolverBlock> blocks;
            blocks.reserve(sccs.size());
            for (const auto &scc : sccs)
            {
                SolverBlock block;
                for (int local_var_idx : scc)
                {
                    block.variable_indices.push_back(active_indices[local_var_idx]);
                    block.equation_indices.push_back(var_to_eq[local_var_idx]);
                }
                blocks.push_back(block);
            }

            return blocks;
        }
    };

} // namespace cones

#endif // CONES_SOLVER_BLOCK_DECOMPOSER_HPP
