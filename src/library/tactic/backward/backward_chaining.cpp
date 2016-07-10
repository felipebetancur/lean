/*
Copyright (c) 2016 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include "util/sexpr/option_declarations.h"
#include "library/trace.h"
#include "library/vm/vm_nat.h"
#include "library/vm/vm_expr.h"
#include "library/vm/vm_list.h"
#include "library/tactic/tactic_state.h"
#include "library/tactic/apply_tactic.h"
#include "library/tactic/backward/backward_lemmas.h"

#ifndef LEAN_DEFAULT_BACKWARD_CHAINING_MAX_DEPTH
#define LEAN_DEFAULT_BACKWARD_CHAINING_MAX_DEPTH 8
#endif

namespace lean {
static name * g_backward_chaining_max_depth = nullptr;

unsigned get_backward_chaining_max_depth(options const & o) {
    return o.get_unsigned(*g_backward_chaining_max_depth, LEAN_DEFAULT_BACKWARD_CHAINING_MAX_DEPTH);
}

#define lean_back_trace(code) lean_trace(name({"tactic", "back_chaining"}), scope_trace_env _scope1(m_ctx.env(), m_ctx); code)

struct back_chaining_fn {
    tactic_state         m_initial_state;
    type_context         m_ctx;
    bool                 m_use_instances;
    unsigned             m_max_depth;
    vm_obj               m_leaf_tactic;
    backward_lemma_index m_lemmas;

    struct choice {
        tactic_state         m_state;
        list<backward_lemma> m_lemmas;
        choice(tactic_state const & s, list<backward_lemma> const & lemmas):
            m_state(s), m_lemmas(lemmas) {}
    };

    tactic_state        m_state;
    buffer<choice>      m_choices;

    back_chaining_fn(tactic_state const & s, transparency_mode md, bool use_instances,
                     unsigned max_depth, vm_obj const & leaf_tactic,
                     list<expr> const & extra_lemmas):
        m_initial_state(s),
        m_ctx(mk_type_context_for(s, md)),
        m_use_instances(use_instances),
        m_max_depth(max_depth),
        m_leaf_tactic(leaf_tactic),
        m_lemmas(backward_lemma_index(m_ctx)),
        m_state(m_initial_state) {
        lean_assert(s.goals());
        for (expr const & extra : extra_lemmas) {
            m_lemmas.insert(m_ctx, extra);
        }
    }

    bool invoke_leaf_tactic() {
        lean_assert(m_state.goals());
        tactic_state tmp = set_goals(m_state, to_list(head(m_state.goals())));
        vm_obj s = to_obj(tmp);
        vm_obj r = invoke(m_leaf_tactic, 1, &s);
        if (optional<tactic_state> new_s = is_tactic_success(r)) {
            m_state = set_goals(*new_s, tail(m_state.goals()));
            return true;
        } else {
            return false;
        }
    }

    bool try_lemmas(list<backward_lemma> const & lemmas) {
        m_ctx.set_mctx(m_state.mctx());
        list<backward_lemma> it = lemmas;
        while (it) {
            backward_lemma const & blemma = head(it);
            expr lemma = blemma.to_expr(m_ctx);
            lean_back_trace(tout() << "[" << m_choices.size() << "] trying lemma " << lemma << "\n";);
            if (optional<tactic_state> new_state = apply(m_ctx, false, m_use_instances, lemma, m_state)) {
                lean_back_trace(tout() << "succeed\n";);
                if (tail(it)) {
                    m_choices.emplace_back(m_state, tail(it));
                }
                m_state = *new_state;
                return true;
            }
            it = tail(it);
        }
        return false;
    }

    bool backtrack() {
        while (!m_choices.empty()) {
            lean_back_trace(tout() << "[" << m_choices.size() << "] backtracking\n";);
            list<backward_lemma> lemmas = m_choices.back().m_lemmas;
            m_state = m_choices.back().m_state;
            m_choices.pop_back();
            if (try_lemmas(lemmas)) {
                return true;
            }
        }
        return false;
    }

    bool run() {
        while (true) {
          loop_entry:
            lean_back_trace(tout() << "current state:\n" << m_state.pp() << "\n";);
            if (!m_state.goals())
                return true;
            if (m_choices.size() >= m_max_depth) {
                lean_back_trace(tout() << "maximum depth reached\n" << m_state.pp() << "\n";);
                if (!backtrack())
                    return false;
                goto loop_entry;
            }
            metavar_decl g = *m_state.get_main_goal_decl();
            expr target    = m_ctx.whnf(g.get_type());
            list<backward_lemma> lemmas = m_lemmas.find(head_index(target));
            if (!lemmas) {
                if (!invoke_leaf_tactic()) {
                    if (!backtrack())
                        return false;
                    goto loop_entry;
                }
            } else {
                if (!try_lemmas(lemmas)) {
                    if (!backtrack())
                        return false;
                    goto loop_entry;
                }
            }
        }
    }

    vm_obj operator()() {
        list<expr> goals = m_initial_state.goals();
        m_state = set_goals(m_initial_state, to_list(head(goals)));
        if (run()) {
            tactic_state final_state = set_goals(m_state, tail(goals));
            return mk_tactic_success(final_state);
        } else {
            return mk_tactic_exception("back_chaining failed, use command 'set_option trace.back_chaining true' to obtain more details", m_initial_state);
        }
    }
};

vm_obj back_chaining(transparency_mode md, bool use_instances, unsigned max_depth,
                     vm_obj const & leaf_tactic, list<expr> const & extra_lemmas, tactic_state const & s) {
    optional<metavar_decl> g = s.get_main_goal_decl();
    if (!g) return mk_no_goals_exception(s);
    return back_chaining_fn(s, md, use_instances, max_depth, leaf_tactic, extra_lemmas)();
}

vm_obj tactic_backward_chaining(vm_obj const & md, vm_obj const & use_instances, vm_obj const & max_depth,
                                vm_obj const & leaf_tactic, vm_obj const & extra_lemmas, vm_obj const & s) {
    return back_chaining(to_transparency_mode(md), to_bool(use_instances),
                         force_to_unsigned(max_depth, std::numeric_limits<unsigned>::max()),
                         leaf_tactic, to_list_expr(extra_lemmas), to_tactic_state(s));
}

void initialize_backward_chaining() {
    DECLARE_VM_BUILTIN(name({"tactic", "backward_chaining_core"}),   tactic_backward_chaining);
    g_backward_chaining_max_depth = new name{"back_chaining", "max_depth"};
    register_unsigned_option(*g_backward_chaining_max_depth, LEAN_DEFAULT_BACKWARD_CHAINING_MAX_DEPTH,
                             "maximum number of nested backward chaining steps");
}

void finalize_backward_chaining() {
    delete g_backward_chaining_max_depth;
}
}