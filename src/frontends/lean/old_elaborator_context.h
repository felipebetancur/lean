/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include "kernel/environment.h"
#include "library/io_state.h"
#include "frontends/lean/local_decls.h"
#include "frontends/lean/local_level_decls.h"
#include "frontends/lean/info_manager.h"

namespace lean {
/** \brief Environment for elaboration, it contains all the information that is "scope-indenpendent" */
class old_elaborator_context {
    environment               m_env;
    io_state                  m_ios;
    local_level_decls         m_lls; // local universe levels
    pos_info_provider const * m_pos_provider;
    info_manager *            m_info_manager;
    // configuration
    options                   m_options;
    bool                      m_check_unassigned;
    bool                      m_flycheck_goals;
    bool                      m_lift_coercions;
    bool                      m_coercions;
    friend class old_elaborator;

    bool     m_show_goal_at;
    unsigned m_show_goal_line;
    unsigned m_show_goal_col;

    bool     m_show_hole_at;
    unsigned m_show_hole_line;
    unsigned m_show_hole_col;

    void set_options(options const & opts);

    /** \brief Support for showing information using hot-keys */
    bool has_show_goal_at(unsigned & line, unsigned & col) const;
    void reset_show_goal_at();

    bool has_show_hole_at(unsigned & line, unsigned & col) const;
    void reset_show_hole_at();
public:
    old_elaborator_context(environment const & env, io_state const & ios, local_level_decls const & lls,
                           pos_info_provider const * pp = nullptr, info_manager * info = nullptr,
                           bool check_unassigned = true);
    old_elaborator_context(old_elaborator_context const & ctx, options const & o);
};
void initialize_old_elaborator_context();
void finalize_old_elaborator_context();
}
