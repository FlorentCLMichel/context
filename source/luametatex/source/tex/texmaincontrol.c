/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex

    We come now to the |main_control| routine, which contains the master switch that causes all the
    various pieces of \TEX\ to do their things, in the right order.

    In a sense, this is the grand climax of the program: It applies all the tools that we have
    worked so hard to construct. In another sense, this is the messiest part of the program: It
    necessarily refers to other pieces of code all over the place, so that a person can't fully
    understand what is going on without paging back and forth to be reminded of conventions that
    are defined elsewhere. We are now at the hub of the web, the central nervous system that
    touches most of the other parts and ties them together.

    The structure of |main_control| itself is quite simple. There's a label called |big_switch|,
    at which point the next token of input is fetched using |get_x_token|. Then the program
    branches at high speed into one of about 100 possible directions, based on the value of the
    current mode and the newly fetched command code; the sum |abs(mode) + cur_cmd| indicates what
    to do next. For example, the case |vmode + letter| arises when a letter occurs in  vertical
    mode (or internal vertical mode); this case leads to instructions that initialize a new
    paragraph and enter horizontal mode.p

    The big |case| statement that contains this multiway switch has been labeled |reswitch|, so
    that the program can |goto reswitch| when the next token has already been fetched. Most of
    the cases are quite short; they call an \quote {action procedure} that does the work for that
    case, and then they either |goto reswitch| or they \quote {fall through} to the end of the
    |case| statement, which returns control back to |big_switch|. Thus, |main_control| is not an
    extremely large procedure, in spite of the multiplicity of things it must do; it is small
    enough to be handled by \PASCAL\ compilers that put severe restrictions on procedure size.

    One case is singled out for special treatment, because it accounts for most of \TEX's
    activities in typical applications. The process of reading simple text and converting it
    into |char_node| records, while looking for ligatures and kerns, is part of \TEX's \quote
    {inner loop}; the whole program runs efficiently when its inner loop is fast, so this part
    has been written with particular care. (This is no longer true in \LUATEX.)

    We leave the |space_factor| unchanged if |sf_code (cur_chr) = 0|; otherwise we set it equal
    to |sf_code (cur_chr)|, except that it should never change from a value less than 1000 to a
    value exceeding 1000. The most common case is |sf_code (cur_chr) = 1000|, so we want that case 
    to be fast.

    All action is done via runners in the function table. Some runners are implemented here,
    others are spread over modules. In due time I will use more prefixes to indicate where they
    belong. Also, more runners will move to their respective modules, a stepwise process. This
    split up is not always consistent which relates to the fact that \TEX\ is a monolithic program
    which in turn means that we keep all the smaller (and more dependent) bits here. There are
    subsystems but they hook into each other, take inserts and adjusts that hook into the builders
    and packagers.

*/

main_control_state_info lmt_main_control_state = {
    .control_state    = goto_next_state,
    .local_level      = 0,
    .after_token      = null,
    .after_tokens     = null,
    .last_par_trigger = 0,
    .last_par_context = 0,
    .loop_iterator    = 0,
    .loop_nesting     = 0,
    .loop_stack_head  = null,
    .loop_stack_tail  = null,
    .quit_loop        = 0,
    .padding          = 0,
};

static inline void tex_aux_big_switch       (int mode, int cmd);
static        void tex_run_prefixed_command (void);

/*tex
    These two helpers, of which the second one is still experimental, actually belong in another
    file so then might be moved. Watch how the first one has the |unsave| call!
*/

static void tex_aux_fixup_directions_and_unsave(void)
{
    int saved_par_state = internal_par_state_par;
    int saved_dir_state = internal_dir_state_par;
    int saved_direction = text_direction_par;
    tex_pop_text_dir_ptr();
    tex_unsave();
    if (cur_mode == hmode) {
        if (saved_dir_state) {
            /* Add local dir node. */
            tex_tail_append(tex_new_dir(cancel_dir_subtype, text_direction_par));
            dir_direction(cur_list.tail) = saved_direction;
        }
        if (saved_par_state) {
            /*tex Add local paragraph node. This resets after a group. */
            tex_tail_append(tex_new_par_node(parameter_par_subtype)); // hmode_par_par_subtype needs checking 
        }
    }
}

static void tex_aux_fixup_directions_only(void)
{
    int saved_dir_state = internal_dir_state_par;
    int saved_direction = text_direction_par;
    tex_pop_text_dir_ptr();
    if (saved_dir_state) {
        /* Add local dir node. */
        tex_tail_append(tex_new_dir(cancel_dir_subtype, saved_direction));
    }
}

static void tex_aux_fixup_math_and_unsave(void)
{
    int saved_math_style = internal_math_style_par;
    int saved_math_scale = internal_math_scale_par;
    tex_unsave();
    if (cur_mode == mmode) { 
        if (saved_math_style >= 0 && saved_math_style != cur_list.math_style) {
            halfword noad = tex_new_node(style_node, (quarterword) saved_math_style);
            cur_list.math_style = saved_math_style;
            tex_tail_append(noad);
        }
        if (saved_math_scale != cur_list.math_scale) {
            halfword noad = tex_new_node(style_node, scaled_math_style);
            style_scale(noad) = saved_math_scale;
            cur_list.math_scale = saved_math_scale;
            tex_tail_append(noad);
        }
    }
}

/*tex

    The |run_| functions hook in the main control handler. Some immediately do something, others
    trigger a follow up scan, driven by the cmd code. Here come some forward declarations; there
    are more that the following |run_| functions. Some runners are defined in other modules. Some
    runners finish what another started, for instance when we see a left brace, depending on state
    another runner can kick in.

*/

/*tex

    Experiment: 

    (1) preceding glyph has option glyph_option_space_factor_overload set 
    (2) factor is larger than 0 and below 1000
    (3) space_factor_overload_par > 0 will force that value to be used 

*/

static inline int tex_aux_use_space_factor_overload(halfword tail, halfword space_factor)
{
    return 
        (space_factor > 0 && space_factor < default_space_factor) &&
        (node_type(tail) == glyph_node) &&
        tex_has_glyph_option(tail, glyph_option_space_factor_overload)
    ;
}

static inline int tex_aux_used_space_factor_overload(halfword space_factor)
{
    return default_space_factor - (space_factor_overload_par ? space_factor_overload_par :  - space_factor);
}

static void tex_aux_adjust_space_factor(halfword chr)
{
    halfword s = tex_get_sf_code(chr);
    if (s == default_space_factor) {
        cur_list.space_factor = default_space_factor;
    } else if (s < default_space_factor) {
        if (s > 0) {
            cur_list.space_factor = s;
        } else {
            /* s <= 0 */
        }
    } else if (tex_aux_use_space_factor_overload(cur_list.tail, cur_list.space_factor)) {
        /* keep it */
    } else if (cur_list.space_factor < default_space_factor) {
        cur_list.space_factor = default_space_factor;
    } else {
        cur_list.space_factor = s;
    }
}

static void tex_aux_run_text_char_number(void)
{
    switch (cur_chr) {
        case char_number_code:
            {
                halfword chr = tex_scan_char_number(0);
                tex_aux_adjust_space_factor(chr);
                tex_tail_append(tex_new_char_node(glyph_unset_subtype, cur_font_par, chr, 1));
                break;
            }
        case glyph_number_code:
            {
                scaled xoffset = glyph_x_offset_par;
                scaled yoffset = glyph_y_offset_par;
                halfword xscale = glyph_x_scale_par;
                halfword yscale = glyph_y_scale_par;
                halfword scale = glyph_scale_par;
                halfword slant = glyph_slant_par;
                halfword weight = glyph_weight_par;
                halfword options = glyph_options_par;
                halfword font = cur_font_par;
                scaled left = 0;
                scaled right = 0;
                scaled raise = 0;
                halfword chr = 0;
                halfword glyph;
                while (1) {
                    switch (tex_scan_character("xyofisklrwXYOFISKLRW", 0, 1, 0)) {
                        case 0:
                            goto DONE;
                        case 'x': case 'X':
                            switch (tex_scan_character("osOS", 0, 0, 0)) {
                                case 'o': case 'O':
                                    if (tex_scan_mandate_keyword("xoffset", 2)) {
                                        xoffset = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                                    }
                                    break;
                                case 's': case 'S':
                                    if (tex_scan_mandate_keyword("xscale", 2)) {
                                        xscale = tex_scan_integer(0, NULL, NULL);
                                    }
                                    break;
                                default:
                                    tex_aux_show_keyword_error("xoffset|xscale");
                                    goto DONE;
                            }
                            break;
                        case 'y': case 'Y':
                            switch (tex_scan_character("osOS", 0, 0, 0)) {
                                case 'o': case 'O':
                                    if (tex_scan_mandate_keyword("yoffset", 2)) {
                                        yoffset = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                                    }
                                    break;
                                case 's': case 'S':
                                    if (tex_scan_mandate_keyword("yscale", 2)) {
                                        yscale = tex_scan_integer(0, NULL, NULL);
                                    }
                                    break;
                                default:
                                    tex_aux_show_keyword_error("yoffset|yscale");
                                    goto DONE;
                            }
                            break;
                        case 'o': case 'O':
                            if (tex_scan_mandate_keyword("options", 1)) {
                                /* resets ! */
                                options = tex_scan_integer(0, NULL, NULL) & glyph_option_valid;
                            }
                            break;
                        case 'k': case 'K':
                            if (tex_scan_mandate_keyword("keepspacing", 1)) {
                                options |= glyph_option_keep_spacing;
                            }
                            break;
                        case 'f': case 'F':
                            if (tex_scan_mandate_keyword("font", 1)) {
                                font = tex_scan_font_identifier(NULL);
                            }
                            break;
                        case 'i': case 'I':
                            if (tex_scan_mandate_keyword("id", 1)) {
                                halfword f = tex_scan_integer(0, NULL, NULL);
                                if (f > 0 && tex_is_valid_font(f)) {
                                    font = f;
                                }
                            }
                            break;
                        case 's': case 'S':
                            switch (tex_scan_character("clCL", 0, 0, 0)) {
                                case 'c': case 'C':
                                    if (tex_scan_mandate_keyword("scale", 2)) {
                                        scale = tex_scan_integer(0, NULL, NULL);
                                    }
                                    break;
                                case 'l': case 'L':
                                    if (tex_scan_mandate_keyword("slant", 2)) {
                                        slant = tex_scan_integer(0, NULL, NULL);
                                    }
                                    break;
                                default:
                                    tex_aux_show_keyword_error("scale|slant");
                                    goto DONE;
                            }
                            break;
                        case 'l': case 'L':
                            if (tex_scan_mandate_keyword("left", 1)) {
                                left = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                            }
                            break;
                        case 'r': case 'R':
                            switch (tex_scan_character("aiAI", 0, 0, 0)) {
                                case 'i': case 'I':
                                    if (tex_scan_mandate_keyword("right", 2)) {
                                        right = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                                    }
                                    break;
                                case 'a': case 'A':
                                    if (tex_scan_mandate_keyword("raise", 2)) {
                                        raise = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                                    }
                                    break;
                                default:
                                    tex_aux_show_keyword_error("right|raise");
                                    goto DONE;
                            }
                            break;
                        case 'w': case 'W':
                            if (tex_scan_mandate_keyword("weight", 1)) {
                                weight = tex_scan_integer(0, NULL, NULL);
                            }
                            break;
                        default:
                            goto DONE;
                    }
                }
              DONE:
                chr = tex_scan_char_number(0);
                if (! (options & glyph_option_keep_spacing)) { 
                    tex_aux_adjust_space_factor(chr);
                }
                glyph = tex_new_char_node(glyph_unset_subtype, font, chr, 1);
                set_glyph_options(glyph, options);
                set_glyph_x_scale(glyph, xscale);
                set_glyph_y_scale(glyph, yscale);
                set_glyph_scale(glyph, scale);
                set_glyph_left(glyph, left);
                set_glyph_right(glyph, right);
                set_glyph_raise(glyph, raise);
                set_glyph_x_offset(glyph, xoffset);
                set_glyph_y_offset(glyph, yoffset);
                set_glyph_weight(glyph, weight);
                set_glyph_slant(glyph, slant);
                tex_tail_append(glyph);
                break;
            }
    }
}

static void tex_aux_run_text_letter(void) 
{
    tex_aux_adjust_space_factor(cur_chr);
    tex_tail_append(tex_new_char_node(glyph_unset_subtype, cur_font_par, cur_chr, 1));
}

/*tex

    Here are all the functions that are called from |main_control| that are not already defined
    elsewhere. For the moment, this list simply in the order that the appear in |init_main_control|,
    below.

*/

static void tex_aux_run_node(void) 
{
    halfword n = cur_chr;
    if (node_token_flagged(n)) {
        tex_get_token();
        n = node_token_sum(n,cur_chr);
    }
    if (copy_lua_input_nodes_par) {
        n = tex_copy_node_list(n, null);
    }
    tex_tail_append(n);
    if (tex_nodetype_has_attributes(node_type(n)) && ! node_attr(n)) {
        attach_current_attribute_list(n);
    }
    while (node_next(n)) {
        n = node_next(n);
        tex_tail_append(n);
        if (tex_nodetype_has_attributes(node_type(n)) && ! node_attr(n)) {
            attach_current_attribute_list(n);
        }
    }
}

/* */

static inline void lmt_bytecode_run(int index)
{
    strnumber u = tex_save_cur_string();
    lmt_token_state.luacstrings = 0;
    lmt_bytecode_call(index);
    tex_restore_cur_string(u);
    if (lmt_token_state.luacstrings > 0) {
        tex_lua_string_start();
    }
}

static inline void lmt_lua_run(int reference, int prefix)
{
    strnumber u = tex_save_cur_string();
    lmt_token_state.luacstrings = 0;
    lmt_function_call(reference, prefix);
    tex_restore_cur_string(u);
    if (lmt_token_state.luacstrings > 0) {
        tex_lua_string_start();
    }
}

static void tex_aux_run_lua_protected_call(void) 
{
    if (cur_chr > 0) {
        lmt_lua_run(cur_chr, 0);
    } else {
        tex_normal_error("luacall", "invalid number in protected call");
    }
}

void tex_aux_lua_call(halfword cmd, halfword chr) 
{
    (void) cmd;
    if (chr > 0) {
        lmt_lua_run(chr, 0);
    } else {
        tex_normal_error("luacall", "invalid number in unprotected call");
    }
}

static void tex_aux_set_lua_value(int a) 
{
    if (cur_chr > 0) {
        lmt_lua_run(cur_chr, a);
    } else {
        tex_normal_error("luavalue", "invalid number");
    }
}

/*tex

    The occurrence of blank spaces is almost part of \TEX's inner loop, since we usually encounter
    about one space for every five non-blank characters. Therefore |main_control| gives second
    highest priority to ordinary spaces.

    When a glue parameter like |\spaceskip| is set to |0pt|, we will see to it later that the
    corresponding glue specification is precisely |zero_glue|, not merely a pointer to some
    specification that happens to be full of zeroes. Therefore it is simple to test whether a glue
    parameter is zero or~not.

    There is a special treatment for spaces when |space_factor <> 1000|.

*/

static void tex_aux_run_math_space(void) 
{
    if (! no_spaces_par && node_type(cur_list.tail) == simple_noad) {
        noad_options(cur_list.tail) |= noad_option_followed_by_space;
    }
}

static void tex_aux_run_space(void) 
{
    switch (no_spaces_par) {
        case 1:
            /*tex Don't inject anything, not even zero skip. */
            return;
        case 2:
            /*tex Inject nothing but zero glue. */
            tex_tail_append(tex_new_glue_node(zero_glue, zero_space_skip_glue)); /* todo: subtype, zero_space_glue? */
            glue_font(cur_list.tail) = cur_font_par;
            break;
        case 3:
            tex_aux_adjust_space_factor(cur_chr);
            tex_tail_append(tex_new_char_node(glyph_unset_subtype, cur_font_par, space_char_par, 1));
            break;
        case 4: 
            {
                halfword p = tex_get_scaled_glue(cur_font_par);
                glue_options(p) |= glue_option_no_auto_break;
                glue_font(p) = cur_font_par;
                tex_tail_append(p);
            }
            break;
        case 5: 
            {
                halfword p = tex_get_scaled_glue(cur_font_par);
                glue_options(p) |= glue_option_no_auto_break;
                glue_font(p) = cur_font_par;
                glue_stretch(p) = 0;
                glue_shrink(p) = 0;
                tex_tail_append(p);
            }
            break;
        case 6: 
            {
                halfword p = tex_new_glue_node(zero_glue, space_skip_glue);
                glue_amount(p) = tex_font_x_scaled(tex_char_width_from_font(cur_font_par, '0'));
                glue_font(p) = cur_font_par;
                glue_options(p) |= glue_option_no_auto_break;
                tex_tail_append(p);
            }
            break;
        default:
            /*tex
                The traditional treatment. A difference with other \TEX's is that we store the spacing
                in the node instead of using the (end of) paragraph bound value.
            */
            {
                halfword p;
                if (cur_mode == hmode && cur_cmd == spacer_cmd && cur_list.space_factor != default_space_factor) {
                    if ((cur_list.space_factor >= space_factor_threshold) && (! tex_glue_is_zero(xspace_skip_par))) {
                        p = tex_get_scaled_parameter_glue(xspace_skip_code, xspace_skip_glue);
                    } else {
                        halfword font = cur_font_par; 
                        if (tex_glue_is_zero(space_skip_par)) {
                            p = tex_get_scaled_glue(font);
                        } else {
                            p = tex_get_parameter_glue(space_skip_code, space_skip_glue); /* not scaled */
                        }
                        /* Modify the glue specification in |q| according to the space factor */
                        if (cur_list.space_factor >= space_factor_threshold) {
                            glue_amount(p) += tex_get_scaled_extra_space(font);
                        } else if (tex_aux_use_space_factor_overload(cur_list.tail, cur_list.space_factor)) {
                            /* what with stretch and shrink */
                            cur_list.space_factor = tex_aux_used_space_factor_overload(cur_list.space_factor);
                            glue_amount(p) = tex_xn_over_d(glue_amount(p), cur_list.space_factor, scaling_factor);
                        }
                        glue_options(p) |= glue_option_has_factor;
                        if (space_factor_stretch_limit_par >= scaling_factor && cur_list.space_factor > scaling_factor) {
                            glue_options(p) |= glue_option_is_limited;
                            glue_stretch(p) = tex_xn_over_d(glue_stretch(p), space_factor_stretch_limit_par, scaling_factor);
                        } else {                   
                            glue_stretch(p) = tex_xn_over_d(glue_stretch(p), cur_list.space_factor, scaling_factor);
                        }
                        if (space_factor_shrink_limit_par >= scaling_factor && cur_list.space_factor > scaling_factor) {
                            glue_options(p) |= glue_option_is_limited;
                            switch (space_factor_mode_par) { 
                                case 1: 
                                    glue_shrink(p) = tex_xn_over_d(glue_shrink(p), space_factor_shrink_limit_par, scaling_factor);
                                    break;
                                case 2 :
                                    glue_shrink(p) = tex_xn_over_d(glue_shrink(p), 2*scaling_factor, space_factor_shrink_limit_par);
                                    break;
                                default:
                                    glue_shrink(p) = tex_xn_over_d(glue_shrink(p), scaling_factor, space_factor_shrink_limit_par);
                                    break;
                            }
                        } else {                   
                            switch (space_factor_mode_par) { 
                                case 1: 
                                    glue_shrink(p) = tex_xn_over_d(glue_shrink(p), cur_list.space_factor, scaling_factor);
                                    break;
                                case 2 :
                                    glue_shrink(p) = tex_xn_over_d(glue_shrink(p), 2*scaling_factor, cur_list.space_factor);
                                    break;
                                default:
                                    glue_shrink(p) = tex_xn_over_d(glue_shrink(p), scaling_factor, cur_list.space_factor);
                                    break;
                            }
                        }
                    }
                } else if (tex_glue_is_zero(space_skip_par)) {
                    /*tex Find the glue specification for text spaces in the current font. */
                    p = tex_get_scaled_glue(cur_font_par);
                } else {
                    /*tex Append a normal inter-word space to the current list. */
                    p = tex_get_parameter_glue(space_skip_code, space_skip_glue); /* not scaled */
                }
                glue_font(p) = cur_font_par;
                tex_tail_append(p);
            }
            break;
        }
}

/*tex A fast one, also used to silently ignore |\par|s in a math formula. */

static void tex_aux_run_relax(void) 
{
    return;
}

static void tex_aux_run_active(void) 
{
    if ((cur_mode == mmode || lmt_nest_state.math_mode) && tex_check_active_math_char(cur_chr)) {
        /*tex We have an intercept. */
        tex_back_input(cur_tok);
    } else {
        cur_cs = tex_active_to_cs(cur_chr, ! lmt_hash_state.no_new_cs);
        cur_cmd = eq_type(cur_cs);
        cur_chr = eq_value(cur_cs);
        tex_x_token();
        tex_back_input(cur_tok);
    }
}

/*tex

    |ignore_spaces| is a special case: after it has acted, |get_x_token| has already fetched the
    next token from the input, so that operation in |main_control| should be skipped.

*/

static void tex_aux_run_ignore_something(void) 
{
    switch (cur_chr) {
        case ignore_space_code:
            /*tex Get the next non-blank call. */
            do {
                tex_get_x_token();
            } while (cur_cmd == spacer_cmd);
            lmt_main_control_state.control_state = goto_skip_token_state;
            break;
        case ignore_par_code:
            /*tex Get the next non-blank/par call. */
            do {
                tex_get_x_token();
            } while (cur_cmd == spacer_cmd || cur_cmd == end_paragraph_cmd);
            lmt_main_control_state.control_state = goto_skip_token_state;
            break;
        case ignore_argument_code:
            /*tex There is nothing to show here. */
            break;
        case ignore_upto_code:
            {
                halfword token = tex_get_token();
                do {
                    tex_get_token();
                } while (cur_tok != token);
                break;
            }
        case ignore_nested_upto_code:
            {
                halfword starttoken = tex_get_token();
                halfword stoptoken = tex_get_token();
                int level = 1;
                do {
                    tex_get_token();
                    if (cur_tok == starttoken) { 
                        ++level;
                    } else if (cur_tok == stoptoken) { 
                        --level;
                    }
                } while (level);
                break;
            }
        case ignore_rest_code:
            tex_quit_token_list();
            break;
        default:
            break;
    }
}

/* */

static void tex_aux_run_math_non_math(void)
{
    if (tracing_commands_par >= 4) {
        tex_begin_diagnostic();
        tex_print_format("[math: pushing back %C]", cur_cmd, cur_chr);
        tex_end_diagnostic();
    }
    tex_back_input(cur_tok);
    tex_begin_paragraph(1, math_char_par_trigger);
}

/*tex

    The most important parts of |main_control| are concerned with \TEX's chief mission of box
    making. We need to control the activities that put entries on vlists and hlists, as well as
    the activities that convert those lists into boxes. All of the necessary machinery has already
    been developed; it remains for us to \quote {push the buttons} at the right times.

    As an introduction to these routines, let's consider one of the simplest cases: What happens
    when |\hrule| occurs in vertical mode, or |\vrule| in horizontal mode or math mode? The code
    in |main_control| is short, since the |scan_rule_spec| routine already does most of what is
    required; thus, there is no need for a special action procedure.

    Note that baselineskip calculations are disabled after a rule in vertical mode, by setting
    |prev_depth := ignore_depth|.

    First we define a procedure that returns a pointer to a rule node. This routine is called just
    after \TEX\ has seen |\hrule| or |\vrule|; therefore |cur_cmd| will be either |hrule| or
    |vrule|. The idea is to store the default rule dimensions in the node, then to override them if
    |height| or |width| or |depth| specifications are found (in any order).

    For a moment I considered this:

    \starttyping
    if (scan_keyword("to")) {
        scan_dimension(0, 0, 0, 0); rule_width(q)  = cur_val;
        scan_dimension(0, 0, 0, 0); rule_height(q) = cur_val;
        scan_dimension(0, 0, 0, 0); rule_depth(q)  = cur_val;
        return q;
    }
    \stoptyping

*/


/*tex

    Many of the actions related to box-making are triggered by the appearance of braces in the
    input. For example, when the user says |\hbox to 100pt {<hlist>}| in vertical mode, the
    information about the box size (100pt, |exactly|) is put onto |save_stack| with a level
    boundary word just above it, and |cur_group:=adjusted_hbox_group|; \TEX\ enters restricted
    horizontal mode to process the hlist. The right brace eventually causes |save_stack| to be
    restored to its former state, at which time the information about the box size (100pt,
    |exactly|) is available once again; a box is packaged and we leave restricted horizontal mode,
    appending the new box to the current list of the enclosing mode (in this case to the current
    list of vertical mode), followed by any vertical adjustments that were removed from the box by
    |hpack|.

    The next few sections of the program are therefore concerned with the treatment of left and
    right curly braces.

    If a left brace occurs in the middle of a page or paragraph, it simply introduces a new level
    of grouping, and the matching right brace will not have such a drastic effect. Such grouping
    affects neither the mode nor the current list.

*/

static void tex_aux_run_left_brace(void) 
{
    tex_new_save_level(simple_group);
    update_tex_internal_par_state(0);
    update_tex_internal_dir_state(0);
}

/*tex

    The |also_simple_group| variant is triggered by |\beginsimplegroup|. It permits a mixed group
    ending model:

    \starttyping
    \def\foo{\beginsimplegroup\bf\let\next}  \foo{text}
    \stoptyping

    So, such a group can end with |\endgroup| as well as |\egroup| or equivalents. This trick is
    mostly meant for math where a complex group produces a list which in turn influences spacing.

*/

static void tex_aux_run_begin_group(void) 
{
    switch (cur_chr) {
        case semi_simple_group_code:
        case also_simple_group_code:
            tex_new_save_level(cur_chr ? also_simple_group : semi_simple_group);
            update_tex_internal_par_state(0);
            update_tex_internal_dir_state(0);
            break;
        case math_simple_group_code:
            tex_new_save_level(math_simple_group);
            update_tex_internal_math_style(cur_mode == mmode ? cur_list.math_style : -1);
            update_tex_internal_math_scale(cur_mode == mmode ? cur_list.math_scale : 0);
            break;
    }
}

static void tex_aux_run_end_group(void) 
{
    switch (cur_group) {
        case semi_simple_group:
        case also_simple_group:
            tex_aux_fixup_directions_and_unsave(); /*tex Includes the |save()| call! */
            break;
        case math_simple_group:
            tex_aux_fixup_math_and_unsave(); /*tex Includes the |save()| call! */
            break;
        default:
            tex_off_save(); /*tex Recover with error. */
            break;
    }
}

/*tex

    Constructions that require a box are started by calling |scan_box| with a specified context
    code. The |scan_box| routine verifies that a |make_box| command comes next and then it calls
    |begin_box|.

    Maybe we should just have three variants as sharing this makes it messy: |cur_cmd| combined
    with |cur_chr| and funny flags for leaders. Due to grouping we have a shared |box_end| so
    it doesn't become much prettier anyway.

 */

static void tex_aux_scan_box(int boxcontext, int optional_equal, scaled shift, halfword slot, halfword callback, halfword leaders)
{
    /*tex Get the next non-blank non-relax... and optionally skip an equal sign */
    while (1) {
        tex_get_x_token();
        if (cur_cmd == spacer_cmd) {
            /*tex Go on. */
        } else if (cur_cmd == relax_cmd) {
            optional_equal = 0;
        } else if (optional_equal && cur_tok == equal_token) {
            optional_equal = 0;
        } else {
            break;
        }
    }
    switch (cur_cmd) {
        case make_box_cmd:
            {
                tex_begin_box(boxcontext, shift, slot, callback, leaders);
                return;
            }
        case vcenter_cmd:
            {
                tex_run_vcenter();
                return;
            }
        case lua_call_cmd:
        case lua_protected_call_cmd:
        case lua_semi_protected_call_cmd:
            {
                if (box_leaders_flag(boxcontext)) {
                    tex_aux_run_lua_protected_call();
                    tex_get_next();
                    if (cur_cmd == node_cmd) {
                        /*tex So we only fetch the tail; the rest can mess up in the current list! */
                        halfword boxnode = null;
                        tex_aux_run_node();
                        boxnode = tex_pop_tail();
                        if (boxnode) {
                            switch (node_type(boxnode)) {
                                case hlist_node:
                                case vlist_node:
                                case rule_node:
                                case glyph_node:
                                    tex_box_end(boxcontext, boxnode, shift, unset_noad_class, slot, callback, leaders);
                                    return;
                            }
                        }
                    }
                    tex_formatted_error("lua", "invalid function call, proper leader content expected");
                    return;
                } else {
                    break;
                }
            }
        case lua_value_cmd:
            {
                halfword v = tex_scan_lua_value(cur_chr);
                switch (v) {
                    case no_val_level:
                        tex_box_end(boxcontext, null, shift, unset_noad_class, slot, callback, leaders);
                        return;
                    case list_val_level:
                        if (box_leaders_flag(boxcontext)) {
                            switch (node_type(cur_val)) {
                                case hlist_node:
                                case vlist_node:
                                case rule_node:
                             // case glyph_node:
                                    tex_box_end(boxcontext, cur_val, shift, unset_noad_class, slot, callback, leaders);
                                    return;
                            }
                        } else {
                            switch (node_type(cur_val)) {
                                case hlist_node:
                                case vlist_node:
                                    tex_box_end(boxcontext, cur_val, shift, unset_noad_class, slot, callback, leaders);
                                    return;
                            }
                        }
                }
                tex_formatted_error("lua", "invalid function call, return type %i instead of %i", v, list_val_level);
                return;
            }
        case hrule_cmd:
        case vrule_cmd:
            {
                if (box_leaders_flag(boxcontext)) {
                    halfword rulenode = tex_aux_scan_rule_spec(cur_cmd == hrule_cmd ? h_rule_type : (cur_cmd == vrule_cmd ? v_rule_type : m_rule_type), cur_chr);
                    tex_box_end(boxcontext, rulenode, shift, unset_noad_class, slot, callback, leaders);
                    return;
                } else {
                    break;
                }
            }
        case char_number_cmd:
            {
                if (cur_mode == hmode && box_leaders_flag(boxcontext)) {
                    /*tex We cheat by just appending to the current list. */
                    halfword boxnode = null;
                    tex_aux_run_text_char_number();
                    boxnode = tex_pop_tail();
                    tex_box_end(boxcontext, boxnode, shift, unset_noad_class, slot, callback, leaders);
                    return;
                } else {
                    break;
                }
            }
    }
    tex_handle_error(
        back_error_type,
        "A <box> was supposed to be here",
        "I was expecting to see \\hbox or \\vbox or \\copy or \\box or something like\n"
        "that. So you might find something missing in your output. But keep trying; you\n"
        "can fix this later."
    );
    if (boxcontext == lua_scan_flag) { /* hm, why after error */
        tex_box_end(boxcontext, null, shift, unset_noad_class, slot, callback, leaders);
    }
}

/*tex
    The |tex_aux_scan_box| call takes a |context| parameter and that is is somewhat weird: it
    can be a box number, a flag signaling a special kind of box like a leader, or it can be the
    shift in a move. It all relates to passing something in a way that make it possible to pick
    it up later.
*/

static void tex_aux_run_move(void) 
{
    int code = cur_chr;
    halfword val = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
    tex_aux_scan_box(direct_box_flag, 0, code == move_forward_code ? val : - val, -1, 0, 0);
}

static int leader_flags[] = {
    a_leaders_flag,
    c_leaders_flag,
    x_leaders_flag,
    g_leaders_flag,
    u_leaders_flag,
};

static void tex_aux_run_leader(void) 
{
    halfword code = cur_chr; 
    halfword callback = 0; 
    halfword options = 0;
    if (code == u_leaders_code) { 
        while (1) {
            switch (tex_scan_character("clnCLN", 0, 1, 0)) {
                case 0:
                    goto DONE;
                case 'l': case 'L':
                    if (tex_scan_mandate_keyword("line", 1)) {
                        options |= glue_option_u_leaders_line;
                    }
                    break;
                case 'n': case 'N':
                    if (tex_scan_mandate_keyword("nobreak", 1)) {
                        options |= glue_option_no_auto_break;
                    }
                    break;
                case 'c': case 'C':
                    if (tex_scan_mandate_keyword("callback", 1)) {
                        callback = tex_scan_integer(0, NULL, NULL);
                    }
                    break;
                default:
                    tex_aux_show_keyword_error("callback|line|nobreak");
                    goto DONE;
            }
        }
      DONE: ;
    }
    tex_aux_scan_box(leader_flags[code], 0, null_flag, -1, callback, options);
}

static void tex_aux_run_legacy(void) 
{
    switch (cur_chr) {
        case shipout_code:
            tex_aux_scan_box(shipout_flag, 0, null_flag, -1, 0, 0);
            break;
        default:
            /* cant_happen */
            break;
    }
}

static void tex_aux_run_local_box(void) 
{
    switch (cur_chr) { 
    case local_reset_boxes_code:
            tex_reset_local_box(local_reset_boxes_code);
            break;
        default:
            tex_aux_scan_local_box(cur_chr);
            break;
    }
}

static void tex_aux_run_make_box(void) 
{
    tex_begin_box(direct_box_flag, null_flag, -1, 0, 0);
}

/*tex

    There is a really small patch to add a new primitive called |\quitvmode|. In vertical modes, it
    is identical to |\indent|, but in horizontal and math modes it is really a no-op (as opposed to
    |\indent|, which executes the |indent_in_hmode| procedure).

    A paragraph begins when horizontal-mode material occurs in vertical mode, or when the paragraph
    is explicitly started by |\quitvmode|, |\indent| or |\noindent|. We can revert this to zero
    while at the same time keeping the node.

    To be considered: delay (as with parfilskip), skip + boundary, pre/post anchor etc.

*/

static void tex_aux_insert_parindent(int indented)
{
    if (normalize_line_mode_option(parindent_skip_mode)) {
        /*tex We cannot use |new_param_glue| yet, because it's a dimen */
        halfword glue = tex_new_glue_node(zero_glue, indent_skip_glue);
        if (indented) {
            glue_amount(glue) = par_indent_par;
        }
        tex_tail_append(glue);
    } else if (indented) {
        halfword box = tex_new_null_box_node(hlist_node, indent_list);
        box_direction(box) = (singleword) par_direction_par;
        box_width(box) = par_indent_par;
        tex_tail_append(box);
    }
}

static void tex_aux_remove_parindent(void)
{
    halfword tail = cur_list.tail;
    switch (node_type(tail)) {
        case glue_node:
            if (tex_is_par_init_glue(tail)) {
                glue_amount(tail) = 0;
            }
            break;
        case hlist_node:
            if (node_subtype(tail) == indent_list) {
                box_width(tail) = 0;
            }
            break;
    }
}

static void tex_aux_run_begin_paragraph_vmode(void) 
{
    switch (cur_chr) {
        case noindent_par_code:
            tex_begin_paragraph(0, no_indent_par_trigger);
            break;
        case indent_par_code:
            tex_begin_paragraph(1, indent_par_trigger);
            break;
        case quitvmode_par_code:
            tex_begin_paragraph(1, force_par_trigger);
            break;
        case snapshot_par_code:
            /* silently ignore */
            tex_scan_integer(0, NULL, NULL);
            break;
        case attribute_par_code:
            /* silently ignore */
            tex_scan_attribute_register_number();
            tex_scan_integer(1, NULL, NULL);
            break;
        case options_par_code:
            /* silently ignore */
            tex_scan_integer(1, NULL, NULL);
            break;
        case wrapup_par_code:
            tex_you_cant_error(NULL);
            break;
    }
}

static bool tex_aux_scan_more_toks(halfword *h)
{
    int reverse = tex_scan_optional_keyword("reverse"); /* maybe not _optional_ */
    do {
        tex_get_x_token();
    } while (cur_cmd == spacer_cmd);
    if (cur_cmd == left_brace_cmd) {
        halfword source = tex_scan_toks_normal(1, NULL);
        if (source) {
            if (! *h) {
                *h = source;
            } else if (reverse) {
                token_link(tex_tail_of_token_list(*h)) = token_link(source);
                tex_put_available_token(source);
            } else { 
                token_link(tex_tail_of_token_list(source)) = token_link(*h);
                tex_put_available_token(*h);
                *h = source;
            }
        }
        return true;
    } else { 
        return false;
    }
}

static void tex_aux_run_begin_paragraph_hmode(void) 
{
    switch (cur_chr) {
        case noindent_par_code:
            /*tex We do as traditional \TEX, so no zero skip either when normalizing */
            break;
        case indent_par_code:
            /*tex We can have |\hbox {\indent x\indent x\indent}| */
            tex_aux_insert_parindent(1);
            break;
        case undent_par_code:
            tex_aux_remove_parindent();
            break;
        case snapshot_par_code:
            {
                halfword tag = tex_scan_integer(0, NULL, NULL);
                halfword par = tex_find_par_par(cur_list.head);
                if (par) {
                    tex_snapshot_par(par, tag);
                }
                break;
            }
        case attribute_par_code:
            {
                halfword att = tex_scan_attribute_register_number();
                halfword val = tex_scan_integer(1, NULL, NULL);
                halfword par = tex_find_par_par(cur_list.head);
                if (par) {
                    if (val == unused_attribute_value) {
                        tex_unset_attribute(par, att, val);
                    } else {
                        tex_set_attribute(par, att, val);
                    }
                }
                break;
            }
        case options_par_code:
            {
                halfword val = tex_scan_integer(1, NULL, NULL);
                halfword par = tex_find_par_par(cur_list.head);
                if (par && val >= 0) {
                    par_options(par) |= (singleword) val;
                }
                break;
            }
        case wrapup_par_code:
            {
                /* maybe also a prepend */
                halfword par = tex_find_par_par(cur_list.head);
                if (par) {
                    halfword h = par_end_par_tokens(par);
                    if (tex_aux_scan_more_toks(&h)) {
                        par_end_par_tokens(par) = h;
                    } else {
                        tex_handle_error(
                            normal_error_type,
                            "I expected a {",
                            "The '\\wrapuppar' command only accepts an explicit token list."
                        );
                    }
                }
                break;
            }
    }
}

static void tex_aux_run_begin_paragraph_mmode(void) 
{
    switch (cur_chr) {
        case indent_par_code:
            {
                /*tex This is kind of weird, we could also support a skip here. */
                halfword p = tex_new_null_box_node(hlist_node, indent_list);
                box_width(p) = par_indent_par;
                p = tex_new_sub_box(p);
                tex_tail_append(p);
                break;
            }
        case snapshot_par_code:
            /* silently ignore */
            tex_scan_integer(0, NULL, NULL);
            break;
        case attribute_par_code:
            /* silently ignore */
            tex_scan_attribute_register_number();
            tex_scan_integer(1, NULL, NULL);
            break;
        case wrapup_par_code:
            tex_you_cant_error(NULL);
            break;
    }
}

static void tex_aux_run_new_paragraph(void) 
{
    int context;
    switch (cur_cmd) {
        case char_given_cmd:
        case other_char_cmd:
        case letter_cmd:
        case accent_cmd:
        case char_number_cmd:
        case discretionary_cmd:
            context = char_par_trigger;
            break;
        case boundary_cmd:
            context = boundary_par_trigger;
            break;
        case explicit_space_cmd:
            context = space_par_trigger;
            break;
        case math_shift_cmd:
        case math_shift_cs_cmd:
            context = math_par_trigger;
            break;
        case hskip_cmd:
            context = hskip_par_trigger;
            break;
        case kern_cmd:
            context = kern_par_trigger;
            break;
        case un_hbox_cmd:
            context = un_hbox_char_par_trigger;
            break;
        case valign_cmd:
            context = valign_char_par_trigger;
            break;
        case vrule_cmd:
            context = vrule_char_par_trigger;
            break;
        default:
            context = normal_par_trigger;
            break;
    }
    if (tracing_commands_par >= 4) {
        tex_begin_diagnostic();
        tex_print_format("[text: pushing back %C]", cur_cmd, cur_chr);
        tex_end_diagnostic();
    }
    tex_back_input(cur_tok);
    tex_begin_paragraph(1, context);
}

/*tex
    Append a |boundary_node|. The |page_boundary| case is kind of special. It adds a node node to
    the list of contributions and triggers the page builder (that only kicks in when there is some
    contribution). That itself can result in firing up the output routine if the page is filled up.
    An alternative is to inject a penalty but we don't want anything to stay behind and using some
    special penalty would be incompatible.

    In order to really trigger a check we change the boundary node into zero penalty in the builder
    when it still present (as the callback can decide to wipe it). It's a bit weird mechanism but
    it closely relates to triggering something that gets logged in the core engine. Anyway, we
    basically have a zero penalty equivalent (but one that doesn't register as last node).
*/

void tex_page_boundary_message(const char *s, halfword n)
{
    if (tracing_pages_par > 0) {
        tex_begin_diagnostic();
        tex_print_format("[page: boundary, %s, trigger %i]", s, n);
        tex_end_diagnostic();
    }
}

static void tex_aux_run_par_boundary(void) 
{
    switch (cur_chr) {
        case page_boundary:
            {   
                halfword n = tex_scan_integer(0, NULL, NULL);
                if (lmt_nest_state.nest_data.ptr == 0 && ! lmt_page_builder_state.output_active) {
                    halfword boundary = tex_new_node(boundary_node, page_boundary);
                    boundary_data(boundary) = n;
                    tex_tail_append(boundary);
                    if (cur_list.mode == vmode) {
                        tex_page_boundary_message("build triggered", n);
                        tex_build_page(boundary_page_context, n);
                    } else {
                        tex_page_boundary_message("appended", n);
                    }
                } else {
                    tex_page_boundary_message("ignored", n);
                }
                break;
            }
        case balance_boundary:
            {
                halfword n = tex_scan_integer(0, NULL, NULL);
                halfword m = tex_scan_integer(0, NULL, NULL);
                if (cur_list.mode == vmode && ! lmt_page_builder_state.output_active) {
                    halfword boundary = tex_new_node(boundary_node, balance_boundary);
                    boundary_data(boundary) = n;
                    boundary_reserved(boundary) = m;
                    tex_tail_append(boundary);
                }
                break;
            }
        /*tex Not yet, first I need a proper use case. */ /*
        case par_boundary:
            {
                halfword boundary = tex_new_node(boundary_node, par_boundary);
                boundary_data(boundary) = tex_scan_integer(0, NULL, NULL);
                tex_tail_append(boundary);
                break;
            }
        */
        default:
            /*tex Go into horizontal mode and try again (was already the modus operandi). */
            tex_aux_run_new_paragraph();
            break;
    }
}

static void tex_aux_run_text_boundary(void) 
{
    halfword boundary = tex_new_node(boundary_node, (quarterword) cur_chr);
    switch (cur_chr) {
        case user_boundary:
        case protrusion_boundary:
        case optional_boundary:
            boundary_data(boundary) = tex_scan_integer(0, NULL, NULL);
            break;
        case lua_boundary:
        case attribute_boundary:
            boundary_data(boundary) = tex_scan_integer(0, NULL, NULL);
            boundary_reserved(boundary) = tex_scan_integer(0, NULL, NULL);
            break;
        case balance_boundary:
            /*tex Maybe we should force vmode? For now we just ignore the value. */
            tex_scan_integer(0, NULL, NULL);
            /* fall through */
        case page_boundary:
            /*tex Maybe we should force vmode? For now we just ignore the value. */
            tex_scan_integer(0, NULL, NULL);
            break;
        default:
            break;
    }
    tex_tail_append(boundary);
}

static void tex_aux_run_math_boundary(void) 
{
    switch (cur_chr) {
        case user_boundary:
            {
                halfword boundary = tex_new_node(boundary_node, (quarterword) cur_chr);
                boundary_data(boundary) = tex_scan_integer(0, NULL, NULL);
                tex_tail_append(boundary);
                break;
            }
        case lua_boundary:
        case attribute_boundary:
            {
                halfword boundary = tex_new_node(boundary_node, (quarterword) cur_chr);
                boundary_data(boundary) = tex_scan_integer(0, NULL, NULL);
                boundary_reserved(boundary) = tex_scan_integer(0, NULL, NULL);
                tex_tail_append(boundary);
                break;
            }
        case math_boundary:
            {
                halfword boundary = tex_new_node(boundary_node, (quarterword) cur_chr);
                boundary_data(boundary) = tex_scan_integer(0, NULL, NULL);
                switch (boundary_data(boundary)) {
                    case begin_math_implicit_boundary: 
                    case end_math_implicit_boundary: 
                        /* valid */
                        break;
                    case begin_math_explicit_boundary: 
                    case end_math_explicit_boundary: 
                        /* valid, penalty to add */
                        boundary_reserved(boundary) = tex_scan_integer(0, NULL, NULL);
                        break;
                }
                tex_tail_append(boundary);
                break;
            }
        case protrusion_boundary:
        case page_boundary:
            /*tex We just ignore the values. */
            tex_scan_integer(0, NULL, NULL);
            break;
    }
}

/*tex

    A paragraph ends when a |par_end| command is sensed, or when we are in horizontal mode when
    reaching the right brace of vertical-mode routines like |\vbox|, |\insert|, or |\output|.

*/

static void tex_aux_run_paragraph_end_vmode(void) 
{
 // tex_normal_paragraph(normal_par_context);
    tex_normal_paragraph(vmode_par_context);
    if (cur_list.mode > nomode) {
        tex_build_page(vmode_par_page_context, 0);
    }
}

/*tex We could pass the group and context here if needed and set some parameter. */

int tex_wrapped_up_paragraph(int context, int final) 
{
    halfword par = tex_find_par_par(cur_list.head);
    lmt_main_control_state.last_par_context = context;
    if (par) {
        int done = 0;
        if (par_end_par_tokens(par)) {
            halfword eop = par_end_par_tokens(par);
            par_end_par_tokens(par) = null;
            tex_back_input(cur_tok);
            /*tex We inject the tokens, which increments the ref count; this one has tracing. */
            tex_begin_token_list(eop, end_paragraph_text);
            /*tex So we need to decrement the token ref here. */
            tex_delete_token_reference(eop);
            done = 1;
        }
        /*tex Like |\everypar| this one is not reset, so less housekeeping that wrapping up. */
        if (every_par_end_par) {
            tex_begin_token_list(every_par_end_par, every_par_end_text);
        }
        /*tex This kicks in when packaging a box: */
        if (final && end_of_group_par) {
            if (! done) {
                tex_back_input(cur_tok);
            }
            tex_begin_token_list(end_of_group_par, end_paragraph_text);
            update_tex_end_of_group(null);
            done = 1;
        }
        return done;
    } else {
        return 0;
    }
}

static void tex_aux_run_paragraph_end_hmode(void) 
{
    if (cur_chr == local_break_end_paragraph_code) {
        /*tex Just for fun: an experiment. */
        tex_tail_append(tex_new_par_node(local_break_par_subtype));
    } else if (! tex_wrapped_up_paragraph(normal_par_context, 0)) {
        if (lmt_input_state.align_state < 0) {
            /*tex This tries to recover from an alignment that didn't end properly. */
            tex_off_save();
        }
        /*tex This takes us to the enclosing mode, if |mode > 0|. */
        tex_end_paragraph(bottom_level_group, normal_par_context);
        if (cur_list.mode == vmode) {
            tex_build_page(hmode_par_page_context, 0);
        }
    }
}

/* */

static void tex_aux_run_halign_mmode(void) 
{
    switch (cur_group) { 
        case math_inline_group:
        case math_display_group:
            tex_run_alignment_initialize();
            break;
        default:
            tex_off_save();
            break;
    }
}

/*tex

    The |\afterassignment| command puts a token into the global variable |after_token|. This global
    variable is examined just after every assignment has been performed. It's value is zero, or a
    saved token.

*/

static void tex_aux_run_after_something(void) 
{
    switch (cur_chr) {
        case after_group_code:
            {
                halfword t = tex_get_token(); /* avoid realloc issues */
                t = tex_get_available_token(t);
                tex_save_for_after_group(t);
                break;
            }
        case after_grouped_code:
            {
                do {
                    tex_get_x_token();
                } while (cur_cmd == spacer_cmd);
                if (cur_cmd == left_brace_cmd) {
                    halfword source = tex_scan_toks_normal(1, NULL);
                    if (source) { 
                        if (token_link(source)) {
                            tex_save_for_after_group(token_link(source));
                            token_link(source) = null;
                        }
                        tex_put_available_token(source);
                    }
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "I expected a {",
                        "The '\\aftergrouped' command only accepts an explicit token list."
                    );
                }
                break;
            }
        case after_assignment_code:
            {
                lmt_main_control_state.after_token = tex_get_token();
                break;
            }
        case after_assigned_code:
            {
                do {
                    tex_get_x_token();
                } while (cur_cmd == spacer_cmd);
                if (cur_cmd == left_brace_cmd) {
                    halfword source = tex_scan_toks_normal(1, NULL);
                    if (source) {
                        /*tex Always, also when empty. */
                        lmt_main_control_state.after_tokens = token_link(source);
                        token_link(source) = null;
                        tex_put_available_token(source);
                    }
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "I expected a {",
                        "The '\\afterassigned' command only accepts an explicit token list."
                    );
                }
                break;
            }
        case at_end_of_group_code:
            {
                halfword t = tex_get_token(); /* avoid realloc issues */
                switch (token_cmd(t)) {
                    case left_brace_cmd:
                    case right_brace_cmd:
                        /*tex We don't want to crash. */
                        tex_handle_error(
                            normal_error_type,
                            "I don't expected a { or }",
                            "The '\\atendofgroup' command won't handle this, maybe you want \\atendofgrouped instead."
                        );
                        break;
                    default: 
                        {
                            halfword r = tex_get_available_token(t);
                            if (end_of_group_par) {
                                halfword p = tex_tail_of_token_list(end_of_group_par);
                                token_link(p) = r;
                            } else {
                                halfword p = tex_get_available_token(null);
                                token_link(p) = r;
                                update_tex_end_of_group(p);
                            }
                        }
                        break;
                }
                break;
            }
        case at_end_of_grouped_code:
            {
                do {
                    tex_get_x_token();
                } while (cur_cmd == spacer_cmd);
                if (cur_cmd == left_brace_cmd) {
                    halfword source = tex_scan_toks_normal(1, NULL);
                    if (source) {
                        if (end_of_group_par) {
                            halfword p = tex_tail_of_token_list(end_of_group_par);
                            token_link(p) = token_link(source);
                            token_link(source) = null;
                            tex_put_available_token(source);
                        } else {
                            update_tex_end_of_group(source);
                        }
                    }
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "I expected a {",
                        "The '\\endofgrouped' command only accepts an explicit token list."
                    );
                }
                break;
            }
        case at_end_of_file_code:
            {
                halfword t = tex_get_token(); /* avoid realloc issues */
                halfword r = tex_get_available_token(t);
                halfword h = tex_get_at_end_of_file();
                h = h ? tex_tail_of_token_list(h) : tex_get_available_token(null);
                token_link(h) = r;
                tex_set_at_end_of_file(h);
                break;
            }
        case at_end_of_filed_code:
            {
                halfword h = tex_get_at_end_of_file();
                if (tex_aux_scan_more_toks(&h)) {
                    tex_set_at_end_of_file(h);
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "I expected a {",
                        "The '\\endoffiled' command only accepts an explicit token list."
                    );
                }
                break;
            }
    }
}

static inline void tex_aux_finish_after_assignment(void)
{
    if (lmt_main_control_state.after_token) {
        tex_back_input(lmt_main_control_state.after_token);
        lmt_main_control_state.after_token = null;
    }
    if (lmt_main_control_state.after_tokens) {
        tex_begin_inserted_list(lmt_main_control_state.after_tokens);
        lmt_main_control_state.after_tokens = null;
    }
}

static void tex_aux_invalid_catcode_table_error(void) 
{
    tex_handle_error(
        normal_error_type,
        "Invalid \\catcode table",
        "All \\catcode table ids must be between 0 and " LMT_TOSTRING(max_n_of_catcode_tables - 1)
    );
}

static void tex_aux_overwrite_catcode_table_error(void) 
{
    tex_handle_error(
        normal_error_type,
        "Invalid \\catcode table",
        "You cannot overwrite the current \\catcode table"
    );
}

static void tex_aux_run_catcode_table(void) 
{
    switch (cur_chr) {
        case save_cat_code_table_code:
            {
                halfword v = tex_scan_integer(0, NULL, NULL);
                if ((v < 0) || (v >= max_n_of_catcode_tables)) {
                    tex_aux_invalid_catcode_table_error();
                } else if (v == cat_code_table_par) {
                    tex_aux_overwrite_catcode_table_error();
                } else {
                    tex_copy_cat_codes(cat_code_table_par, v);
                }
                break;
            }
        case restore_cat_code_table_code:
            {
                halfword v = tex_scan_integer(0, NULL, NULL);
                if ((v < 0) || (v >= max_n_of_catcode_tables)) {
                    tex_aux_invalid_catcode_table_error();
                } else {
                    tex_restore_cat_codes(v, cur_level);
                }
                break;
            }
        case init_cat_code_table_code:
            {
                halfword v = tex_scan_integer(0, NULL, NULL);
                if ((v < 0) || (v >= max_n_of_catcode_tables)) {
                    tex_aux_invalid_catcode_table_error();
                } else if (v == cat_code_table_par) {
                    tex_aux_overwrite_catcode_table_error();
                } else {
                    tex_initialize_cat_codes(v);
                }
                break;
            }
            /*
        case dflt_cat_code_table_code:
            {
                halfword v = scan_int(1);
                if ((v < 0) || (v > CATCODE_MAX)) {
                    invalid_catcode_table_error();
                } else {
                    set_cat_code_table_default(cat_code_table_par, v);
                }
            }
            break;
            */
        default:
            break;
    }
}

static void tex_aux_run_end_local(void)
{
    if (tracing_nesting_par > 2) {
        tex_local_control_message("leaving token scanner due to local end token");
    }
    tex_end_local_control();
}

static void tex_aux_run_lua_function_call(void)
{
    switch (cur_chr) {
        case lua_function_call_code:
            {
                halfword v = tex_scan_function_reference(0);
                lmt_lua_run(v, 0);
                break;
            }
        case lua_bytecode_call_code:
            {
                halfword v = tex_scan_bytecode_reference(0);
                lmt_bytecode_run(v);
                break;
            }
        default:
            break;
    }
}

/*tex

    The |main_control| uses a jump table, and |init_main_control| sets that table up. We need to
    assign an entry for {\em each} of the three modes! The jump table is gone. 

    For mode-independent commands, the following macro is useful. Also, there is a list of cases
    where the user has probably gotten into or out of math mode by mistake. \TEX\ will insert a
    dollar sign and rescan the current token, and it makes sense to have a macro for that as well.

    Here is |main_control| itself. It is quite short nowadays.  The initializer is at the end of
    this file which saves a bunch of forward declarations.

 */

int tex_main_control(void)
{
    lmt_main_control_state.control_state = goto_next_state;
    if (every_job_par) {
        tex_begin_token_list(every_job_par, every_job_text);
    }
    while (1) {
        switch (lmt_main_control_state.control_state) { 
            case goto_next_state:
                tex_get_x_token();
                break;
            case goto_skip_token_state:
                lmt_main_control_state.control_state = goto_next_state;
                break;
            case goto_return_state:
                return lmt_main_state.run_state == initializing_state && cur_chr == dump_code;
        }
        /*tex
            Give diagnostic information, if requested. When a new token has just been fetched at
            |big_switch|, we have an ideal place to monitor \TEX's activity.
        */
        if (tracing_commands_par > 0) {
            tex_show_cmd_chr(cur_cmd, cur_chr);
        }
        /*tex Run the command: */
        tex_aux_big_switch(cur_mode, cur_cmd);
    }
    return 0; /* unreachable */
}

/*tex

    We assume a trailing |\relax|: |{...}\relax|, so we don't need a |back_input ()| here.

*/

void tex_local_control_message(const char *s)
{
    tex_begin_diagnostic();
    tex_print_format("[local control: level %i, %s]", lmt_main_control_state.local_level, s);
    tex_end_diagnostic();
}

/*tex

    We can save in two ways but when, for symmetry I want it to happen at the current level, we need
    to use the save stack. It depends a bit on how this will evolve.

    This one is used in the runlocal \LUA\ helper. This local control is in fact like the main loop,
    so it can result in stuff being injected in for instance the main vertical list. I played with
    control over the mode but that gave weird side effects, so I dropped that immediately.

    The implementation of local control in \LUAMETATEX\ is a bit different from \LUATEX\ because we
    use it in several ways.

*/

void tex_local_control(int obeymode)
{
    full_scanner_status saved_full_status = tex_save_full_scanner_status();
    int old_mode = cur_list.mode;
    int at_level = lmt_main_control_state.local_level;
    lmt_main_control_state.local_level += 1;
    lmt_main_control_state.control_state = goto_next_state;
    if (! obeymode) {
        cur_list.mode = restricted_hmode;
    }
    while (1) {
        if (lmt_main_control_state.control_state == goto_skip_token_state) {
            lmt_main_control_state.control_state = goto_next_state;
        } else {
            tex_get_x_token();
        }
        if (tracing_commands_par > 0) {
            tex_show_cmd_chr(cur_cmd, cur_chr);
        }
        tex_aux_big_switch(cur_mode, cur_cmd);
        if (lmt_main_control_state.local_level <= at_level) {
            lmt_main_control_state.control_state = goto_next_state;
            if (tracing_nesting_par > 2) {
                /*tex This is a kind of duplicate message, which can be confusing */
                tex_local_control_message("leaving local control due to level change");
            }
            break;
        } else if (lmt_main_control_state.control_state == goto_return_state) {
            if (tracing_nesting_par > 2) {
                tex_local_control_message("leaving local control due to triggering");
            }
            break;
        }
    }
    if (! obeymode) {
        cur_list.mode = old_mode;
    }
    tex_unsave_full_scanner_status(&saved_full_status);
}

static inline int tex_aux_is_iterator_value(halfword tokeninfo)
{
    if (tokeninfo >= cs_token_flag) {
        halfword cs = tokeninfo - cs_token_flag;
        return eq_type(cs) == some_item_cmd && eq_value(cs) == last_loop_iterator_code;
    } else {
        return 0;
    }
}

static inline void tex_push_stack_entry(void)
{
    halfword state = tex_get_node(loop_state_node_size);
    node_type(state) = loop_state_node;
    loop_state_count(state) = 0;
    if (lmt_main_control_state.loop_stack_head) { 
        node_prev(lmt_main_control_state.loop_stack_head) = state;
        node_next(state) = lmt_main_control_state.loop_stack_head;
    } else { 
        lmt_main_control_state.loop_stack_tail = state;
    }
    lmt_main_control_state.loop_stack_head = state;
}

static inline void tex_pop_stack_entry(void)
{
    halfword state = lmt_main_control_state.loop_stack_head;    
    lmt_main_control_state.loop_stack_head = node_next(state);
    if (! lmt_main_control_state.loop_stack_head) {
        lmt_main_control_state.loop_stack_tail = null;
    }
    tex_free_node(state, loop_state_node_size);
}

halfword tex_nested_loop_iterator(void)
{
    halfword delta = tex_scan_integer(0, NULL, NULL);
    halfword state = lmt_main_control_state.loop_stack_tail;
    while (delta-- > 0 && state) {
        state = node_prev(state);
    }
    return state ? loop_state_count(state) : 0;
}

halfword tex_previous_loop_iterator(void)
{
    halfword delta = tex_scan_integer(0, NULL, NULL);
    halfword state = lmt_main_control_state.loop_stack_head;
    while (delta-- && state) {
        state = node_next(state);
    }
    return state ? loop_state_count(state) : 0;
}

static inline halfword tex_previous_loop_iterator_delta(int delta)
{
    halfword state = lmt_main_control_state.loop_stack_head;
    while (delta-- && state) {
        state = node_next(state);
    }
    return state ? loop_state_count(state) : 0;
}

static inline void tex_update_stack_entry(halfword count)
{
    loop_state_count(lmt_main_control_state.loop_stack_head) = count;
}

void tex_begin_local_control(void)
{
    halfword code = cur_chr;
    if (tracing_nesting_par > 2) {
        tex_local_control_message("entering token scanner via primitive");
    }
    switch (code) {
        case local_control_list_code:
            {
                halfword t;
                halfword h = tex_scan_toks_normal(0, &t);
                halfword r = tex_get_available_token(token_val(end_local_cmd, 0));
                tex_begin_inserted_list(r);
                tex_begin_token_list(h, local_text);
                break;
            }
        case local_control_token_code:
            {
                halfword t = tex_get_token(); /* avoid realloc issues */
                halfword h = get_reference_token();
                halfword r = tex_get_available_token(token_val(end_local_cmd, 0));
                tex_store_new_token(h, t);
                tex_begin_inserted_list(r);
                tex_begin_token_list(h, local_text);
                break;
            }
        /*tex
            For the moment al three are here because they share some code. At some point I might
            move the last two to the |convert_cmd| which is more natural spot but this is easier
            for debugging.

            The align_state hack was tricky and took me a while to figure out because it only was
            an issue with +10K loops (where 10K is this magic state number).

            We support a leading optional equal sign because that can help make robust macros that
            get |\the \dimexpr 1pt| etc fed which can lead to \TEX\ seeing one huge number.

            The repeat variants are just there for convenience: it saves entering the initial value 
            and step. 

        */
        case local_control_loop_code:
        case expanded_loop_code:
        case unexpanded_loop_code:
        case local_control_repeat_code:
        case expanded_repeat_code:
        case unexpanded_repeat_code:
        case local_control_endless_code:
        case expanded_endless_code:
        case unexpanded_endless_code:
            {
                halfword tail;
                int looping = code >= local_control_loop_code && code <= unexpanded_loop_code;
                int endless = code >= local_control_endless_code && code <= unexpanded_endless_code;
                halfword first = looping ? tex_scan_integer(1, NULL, NULL) : 1;
                halfword last = endless ? max_integer : tex_scan_integer(1, NULL, NULL);
                halfword step = looping ? tex_scan_integer(1, NULL, NULL) : 1;
                halfword head = tex_scan_toks_normal(0, &tail);
                if (token_link(head) && step) {
                    int savedloop = lmt_main_control_state.loop_iterator;
                    int savedquit = lmt_main_control_state.quit_loop;
                    lmt_main_control_state.loop_iterator = 0;
                    lmt_main_control_state.quit_loop = 0;
                    ++lmt_main_control_state.loop_nesting;
                    tex_push_stack_entry();
                    switch (code) {
                        case local_control_loop_code:
                        case local_control_repeat_code:
                        case local_control_endless_code:
                            {
                                /*tex:
                                    Appending to tail gives issues at the outer level, for instance
                                    |\dorecurse {3} {\startTEXpage \stopTEXpage}| without |\starttext
                                    \stoptext| wrapping. So, no:
                                */
                                /* tex_store_new_token(tail, token_val(end_local_cmd, 0)); */
                              LOCALCONTROL:
                                for (halfword i = first; step > 0 ? i <= last : i >= last; i += step) {
                                    lmt_main_control_state.loop_iterator = i;
                                    tex_update_stack_entry(i);
                                    lmt_main_control_state.quit_loop = 0;
                                    /*tex But this, so that we get a proper |\end message|: */
                                    tex_begin_inserted_list(tex_get_available_token(token_val(end_local_cmd, 0)));
                                    /*tex ... maybe we need to enforce a level > 0 instead. */
                                    tex_begin_token_list(head, local_loop_text);
                                    tex_local_control(1);
                                    /*tex We need to avoid build-up. */
                                    tex_cleanup_input_state();
                                    if (lmt_main_control_state.quit_loop) {
                                        break;
                                    } else if (endless && i >= last) {
                                        goto LOCALCONTROL;
                                    }
                                }
                                tex_flush_token_list(head);
                                break;
                            }
                        case expanded_loop_code:
                        case expanded_repeat_code:
                        case expanded_endless_code:
                            {
                                halfword h = null;
                                halfword t = null;
                                full_scanner_status saved_full_status = tex_save_full_scanner_status();
                                strnumber u = tex_save_cur_string();
                                tex_store_new_token(tail, right_brace_token + '}');
                              EXPANDED:
                                for (halfword i = first; step > 0 ? i <= last : i >= last; i += step) {
                                    halfword lt = null;
                                    halfword lh = null;
                                    ++lmt_input_state.align_state; /* emulates the { for the } above */
                                    lmt_main_control_state.loop_iterator = i;
                                    tex_update_stack_entry(i);
                                    tex_begin_token_list(head, loop_text); /* ref counted */
                                    lh = tex_scan_toks_expand(1, &lt, 0, 0);
                                    if (token_link(lh)) {
                                        if (h) {
                                            token_link(t) = token_link(lh);
                                        } else {
                                            h = token_link(lh);
                                        }
                                        t = lt;
                                    }
                                    tex_put_available_token(lh);
                                    tex_cleanup_input_state();
                                    if (lmt_main_control_state.quit_loop) {
                                        break;
                                    } else if (endless && i >= last) {
                                        goto EXPANDED;
                                    }
                                }
                                tex_unsave_full_scanner_status(&saved_full_status);
                                tex_restore_cur_string(u);
                                tex_flush_token_list(head);
                                tex_begin_inserted_list(h);
                                break;
                            }
                        case unexpanded_loop_code:
                        case unexpanded_repeat_code:
                        case unexpanded_endless_code:
                            {
                                /*
                                    A |\currentloopiterator| will not adapt itself in this kind of
                                    loop so we can as well replace it by the current one value which
                                    is what we do here. There is some overhead but I can live with
                                    that.
                                */

                                halfword h = token_link(head);
                                halfword tt = null;
                                halfword t = h;
                                halfword b = 0; /* we can count and then break out */
                                while (token_link(t)) {
                                    t = token_link(t);
                                    if (! b && tex_aux_is_iterator_value(token_info(t))) {
                                        b = 1;
                                    }
                                }
                                tt = t;
                              UNEXPANDED:
                                for (halfword i = first + step; step > 0 ? i <= last : i >= last; i += step) {
                                    halfword hh = h;
                                    while (1) {
                                        t = tex_store_new_token(t, token_info(hh));
                                        if (b && tex_aux_is_iterator_value(token_info(t))) {
                                            halfword v = (i < min_iterator_value) ? min_iterator_value : (i > max_iterator_value ? max_iterator_value : i);
                                            token_info(t) = token_val(iterator_value_cmd, v < 0 ? 0x100000 - v : v);
                                        }
                                        if (hh == tt) {
                                            break;
                                        } else {
                                            hh = token_link(hh);
                                        }
                                    }
                                    if (endless && i >= last) {
                                        goto UNEXPANDED;
                                    }
                                }
                                if (b) {
                                    halfword hh = h;
                                    while (1) {
                                        if (tex_aux_is_iterator_value(token_info(hh))) {
                                            halfword v = (first < min_iterator_value) ? min_iterator_value : (first > max_iterator_value ? max_iterator_value : first);
                                            token_info(hh) = token_val(iterator_value_cmd, v < 0 ? 0x100000 - v : v);
                                        }
                                        if (hh == tt) {
                                            break;
                                        } else {
                                            hh = token_link(hh);
                                        }
                                    }
                                }
                                tex_put_available_token(head);
                                tex_begin_inserted_list(h);
                                break;
                            }
                    }
                    tex_pop_stack_entry();
                    --lmt_main_control_state.loop_nesting;
                    lmt_main_control_state.quit_loop = savedquit;
                    lmt_main_control_state.loop_iterator = savedloop;
                    return;
                } else {
                    tex_flush_token_list(head);
                }
                return;
            }
    }
    tex_local_control(1);      /*tex In this case nicer than 0. */
 // tex_cleanup_input_state(); /*tex Yes or no? */
}

void tex_end_local_control(void )
{
    if (lmt_main_control_state.local_level > 0) {
        lmt_main_control_state.local_level -= 1;
    } else {
        tex_local_control_message("redundant end local control");
    }
}

/*tex

    We need to go back to the main loop. This is rather nasty and dirty and counter-intuive code and
    there might be a cleaner way. Basically we trigger the main control state from here.

    \starttyping
     0 0       \directlua{token.scan_box()}\hbox{!}
    -1 0       \setbox0\hbox{x}\directlua{token.scan_box()}\box0
     1 1       \toks0={\directlua{token.scan_box()}\hbox{x}}\directlua{tex.runtoks(0)}
     0 0  1 1  \directlua{tex.box[0]=token.scan_box()}\hbox{x\directlua{node.write(token.scan_box())}\hbox{x}}
     0 0  0 1  \setbox0\hbox{x}\directlua{tex.box[0]=token.scan_box()}\hbox{x\directlua{node.write(token.scan_box())}\box0}
    \stoptyping

    It's rather fragile code so we added some tracing options.

*/

halfword tex_local_scan_box(void)
{
    int old_mode = cur_list.mode;
    int old_level = lmt_main_control_state.local_level;
    cur_list.mode = restricted_hmode;
    tex_aux_scan_box(lua_scan_flag, 0, null_flag, -1, 0, 0);
    if (lmt_main_control_state.local_level == old_level) {
        /*tex |\directlua{print(token.scan_list())}\hbox{!}| (n n) */
        if (tracing_nesting_par > 2) {
            tex_local_control_message("entering at end of box scanning");
        }
        tex_local_control(1);
// tex_cleanup_input_state();
    } else {
        /*tex |\directlua{print(token.scan_list())}\box0| (n-1 n) */
        /*
            if (tracing_nesting_par > 2) {
                local_control_message("setting level after box scanning");
            }
        */
        lmt_main_control_state.local_level = old_level;
    }
    cur_list.mode = old_mode;
    return cur_box;
}

/*tex

    We have an issue with modes when we quit here because we're coming from and still staying at
    the \LUA\ end. So, unless we're already nested, we trigger an end_local_level token (an
    extension code).

*/

static void tex_aux_wrapup_local_scan_box(void)
{
    /*
    if (tracing_nesting_par > 2) {
        local_control_message("leaving box scanner");
    }
    */
    lmt_main_control_state.local_level -= 1;
}

static void tex_aux_run_insert_dollar_sign(void)
{
    tex_back_input(cur_tok);
    cur_tok = dollar_token_m;
    tex_handle_error(
        insert_error_type,
        "Missing $ inserted",
        "I've inserted a begin-math/end-math symbol since I think you left one out.\n"
        "Proceed, with fingers crossed."
    );
}

/*tex

    The |you_cant| procedure prints a line saying that the current command is illegal in the current
    mode; it identifies these things symbolically.

*/

void tex_you_cant_error(const char *helpinfo)
{
    tex_handle_error(
        normal_error_type,
        "You can't use '%C' in %M", cur_cmd, cur_chr, cur_list.mode,
        helpinfo
    );
}

/*tex

    When erroneous situations arise, \TEX\ usually issues an error message specific to the particular
    error. For example, |\noalign| should not appear in any mode, since it is recognized by the
    |align_peek| routine in all of its legitimate appearances; a special error message is given when
    |\noalign| occurs elsewhere. But sometimes the most appropriate error message is simply that the
    user is not allowed to do what he or she has attempted. For example, |\moveleft| is allowed only
    in vertical mode, and |\lower| only in non-vertical modes.

*/

static void tex_aux_run_illegal_case(void)
{
    tex_you_cant_error(
        "Sorry, but I'm not programmed to handle this case;\n"
        "I'll just pretend that you didn''t ask for it.\n"
        "If you're in the wrong mode, you might be able to\n"
        "return to the right one by typing 'I}' or 'I$' or 'I\\par'."
    );
}

/*tex

    Some operations are allowed only in privileged modes, i.e., in cases that |mode > 0|. The
    |privileged| function is used to detect violations of this rule; it issues an error message and
    returns |false| if the current |mode| is negative.

*/

int tex_in_privileged_mode(void)
{
    if (cur_list.mode > nomode) {
        return 1;
    } else {
        tex_aux_run_illegal_case();
        return 0;
    }
}

/*tex

    We don't want to leave |main_control| immediately when a |stop| command is sensed, because it
    may be necessary to invoke an |\output| routine several times before things really grind to a
    halt. (The output routine might even say |\gdef \end {...}|, to prolong the life of the job.)
    Therefore |its_all_over| is |true| only when the current page and contribution list are empty,
    and when the last output was not a \quote {dead cycle}. We do this when |\end| or |\dump|
    occurs. This |stop| is a special case as we want |main_control| to return to its caller if there
    is nothing left to do.

*/

static void tex_aux_run_end_job(void)
{
    if (tex_in_privileged_mode()) {
        if ((page_head == lmt_page_builder_state.tail)
         && (cur_list.head == cur_list.tail)
         && (lmt_page_builder_state.dead_cycles == 0)) {
            /*tex This is the only way out. */
            lmt_main_control_state.control_state = goto_return_state;
        } else {
            /*tex 
                We will try to end again after ejecting residual material and append |\hbox to \hsize
                {}\vfill\penalty-'10000000000|.
            */
            tex_back_input(cur_tok);
            tex_tail_append(tex_new_null_box_node(hlist_node, unknown_list));
            box_width(cur_list.tail) = hsize_par;
            tex_tail_append(tex_new_glue_node(fi_ll_glue, user_skip_glue)); /* todo: subtype, final_skip_glue? */
            tex_tail_append(tex_new_penalty_node(final_penalty, final_penalty_subtype));
            tex_build_page(end_page_context, 0);
        }
    }
}

/*tex

    The |hskip| and |vskip| command codes are used for control sequences like |\hss| and |\vfil| as
    well as for |\hskip| and |\vskip|. The difference is in the value of |cur_chr|.

    All the work relating to glue creation has been delegated to the following subroutine. It does
    not call |build_page|, because it is used in at least one place where that would be a mistake.

    The preset glue primitives are an efficiency feature in a traditional engine but becuase we 
    don't use shared glue in \LUAMETATEX, there is no real need for them except maybe less tracing
    and avoiding lookahead issues. Adding the triple |l| and additional |neg| variants makes no 
    sense because these are seldom used. 

*/

static const int glue_filler_codes[] = { 
    fi_l_glue,
    fi_ll_glue,
    fi_ss_glue,
    fi_l_neg_glue,
};

static void tex_aux_run_glue(void)
{
    halfword code = cur_chr;
    switch (code) {
        case fi_l_code:
        case fi_ll_code:
        case fi_ss_code:
        case fi_l_neg_code:
            tex_tail_append(tex_new_glue_node(glue_filler_codes[code], user_skip_glue));
            break;
        case skip_code:
            {
                halfword v = tex_scan_glue(glue_val_level, 0, 1);
                halfword g = tex_new_glue_node(v, user_skip_glue);
             /* glue_data(g) = glue_data_par; */
                if (cur_mode == mmode) {
                   /*tex This could be an option. */
                   glue_options(g) |= glue_option_no_auto_break;
                }
                tex_tail_append(g);
                tex_flush_node(v);
                break;
            }
        default:
            break;
    }
}

static void tex_aux_run_mglue(void)
{
    switch (cur_chr) {
        case normal_mskip_code:
            {
                halfword v = tex_scan_glue(muglue_val_level, 0, 0);
                tex_tail_append(tex_new_glue_node(v, mu_glue));
                tex_flush_node(v);
                break;
            }
        case atom_mskip_code:
            {
                halfword left = tex_scan_math_class_number(0);
                halfword right = tex_scan_math_class_number(0);
                halfword style = tex_scan_math_style_identifier(0, 0);
                halfword node = tex_math_spacing_glue(left, right, style);
                if (node) {
                    tex_tail_append(node);
                } else { 
                    /*tex This could be an option: */
                    tex_tail_append(tex_new_glue_node(zero_glue, mu_glue));
                }
                break;
            }
    }
}

/*tex

    We have to deal with errors in which braces and such things are not properly nested. Sometimes
    the user makes an error of commission by inserting an extra symbol, but sometimes the user makes
    an error of omission. \TEX\ can't always tell one from the other, so it makes a guess and tries
    to avoid getting into a loop.

    The |off_save| routine is called when the current group code is wrong. It tries to insert
    something into the user's input that will help clean off the top level.

*/

void tex_off_save(void)
{
    if (cur_group == bottom_level_group) {
        /*tex Drop current token and complain that it was unmatched */
        tex_handle_error(normal_error_type, "Extra %C", cur_cmd, cur_chr,
            "Things are pretty mixed up, but I think the worst is over."
        );
    } else {
        const char * helpinfo =
            "I've inserted something that you may have forgotten. (See the <inserted text>\n"
            "above.) With luck, this will get me unwedged.";
        halfword h = tex_get_available_token(null);
        tex_back_input(cur_tok);
        /*tex
            Prepare to insert a token that matches |cur_group|, and print what it is. At this point,
            |link (temp_token_head) = p|, a pointer to an empty one-word node.
        */
        switch (cur_group) {
            case also_simple_group:
            case semi_simple_group:
            case math_simple_group:
                {
                    set_token_info(h, deep_frozen_end_group_token);
                    tex_handle_error(
                        normal_error_type,
                        "Missing \\endgroup inserted",
                        helpinfo
                    );
                    break;
                }
            case math_inline_group:
            case math_display_group:
            case math_equation_number_group:
                {
                    set_token_info(h, math_shift_token + '$');
                    tex_handle_error(
                        normal_error_type,
                        "Missing $ inserted",
                        helpinfo
                    );
                    break;
                }
            case math_fence_group:
                {
                    /*tex
                        We feed the right dummy fence back into the input which is cheaper then 
                        finishing the fence here. We don't use the hard coded period approach 
                        because, even if that is kind of convention and likely configured in 
                        a macro package, one can never be sure. That means that using zero as 
                        signal is just as good.
                    */
                
                 // halfword q = tex_get_available_token(period_token); /* or char 0 */
                    halfword q = tex_get_available_token(token_val(math_char_number_cmd, math_char_ignore_code));
                    halfword f = node_next(cur_list.head);
                    set_token_info(h, deep_frozen_right_token);
                    set_token_link(h, q);
                 // tex_add_noad_option(f, noad_option_ignore);
                    if (! (f && node_type(f) == fence_noad && has_noad_option_nocheck(f))) {
                        tex_handle_error(
                            normal_error_type,
                            "Missing \\right\\nomathchar inserted",
                            helpinfo
                        );
                    }
                    break;
                }
            default:
                {
                    set_token_info(h, right_brace_token + '}');
                    tex_handle_error(
                        normal_error_type,
                        "Missing } inserted",
                        helpinfo
                     );
                    break;
                }
        }
        tex_begin_inserted_list(h);
    }
}

/*tex

    The routine for a |right_brace| character branches into many subcases, since a variety of things
    may happen, depending on |cur_group|. Some types of groups are not supposed to be ended by a
    right brace; error messages are given in hopes of pinpointing the problem. Most branches of this
    routine will be filled in later, when we are ready to understand them; meanwhile, we must prepare
    ourselves to deal with such errors.

    When the right brace occurs at the end of an |\hbox| or |\vbox| or |\vtop| construction, the
    |package| routine comes into action. We might also have to finish a paragraph that hasn't ended.
*/

static void tex_aux_extra_right_brace_error(void)
{
    const char *helpinfo =
        "I've deleted a group-closing symbol because it seems to be spurious, as in\n"
        "'$x}$'. But perhaps the } is legitimate and you forgot something else, as in\n"
        "'\\hbox{$x}'.";
    switch (cur_group) {
        case also_simple_group:
        case semi_simple_group:
            tex_handle_error(
                normal_error_type,
                "Extra }, or forgotten %eendgroup",
                helpinfo
            );
            break;
        case math_simple_group:
            tex_handle_error(
                normal_error_type,
                "Extra }, or forgotten %eendmathgroup",
                helpinfo
            );
            break;
        case math_inline_group:
        case math_display_group:
        case math_equation_number_group:
            tex_handle_error(
                normal_error_type,
                "Extra }, or forgotten $",
                helpinfo
            );
            break;
        case math_fence_group:
            tex_handle_error(
                normal_error_type,
                "Extra }, or forgotten %eright",
                helpinfo
            );
            break;
    }
    ++lmt_input_state.align_state;
}

static inline void tex_aux_finish_hbox(void)
{
    tex_aux_fixup_directions_only();
    tex_package(hbox_code);
}

static inline void tex_aux_finish_adjusted_hbox(void)
{
    lmt_packaging_state.post_adjust_tail = post_adjust_head;
    lmt_packaging_state.pre_adjust_tail = pre_adjust_head;
    lmt_packaging_state.post_migrate_tail = post_migrate_head;
    lmt_packaging_state.pre_migrate_tail = pre_migrate_head;
    tex_package(hbox_code);
}

static inline void tex_aux_finish_vbox(void)
{

    if (! tex_wrapped_up_paragraph(vbox_par_context, 1)) {
        tex_end_paragraph(vbox_group, vbox_par_context);
        tex_package(vbox_code);
    }
}

static inline void tex_aux_finish_vtop(void)
{
    if (! tex_wrapped_up_paragraph(vtop_par_context, 1)) {
        tex_end_paragraph(vtop_group, vtop_par_context);
        tex_package(vtop_code);
    }
}

static inline void tex_aux_finish_dbox(void)
{
    if (! tex_wrapped_up_paragraph(dbox_par_context, 1)) {
        tex_end_paragraph(dbox_group, dbox_par_context);
        tex_package(dbox_code);
    }
}

static inline void tex_aux_finish_simple_group(void)
{
    tex_aux_fixup_directions_and_unsave();
}

static void tex_aux_finish_bottom_level_group(void)
{
    tex_handle_error(
        normal_error_type,
        "Too many }'s",
        "You've closed more groups than you opened. Such booboos are generally harmless,\n"
        "so keep going."
    );
}

static inline void tex_aux_finish_output(void)
{
    tex_pop_text_dir_ptr();
    tex_resume_after_output();
}

static void tex_aux_run_right_brace(void)
{
    switch (cur_group) {
        case bottom_level_group:
            tex_aux_finish_bottom_level_group();
            break;
        case simple_group:
            tex_aux_finish_simple_group();
            break;
        case hbox_group:
            tex_aux_finish_hbox();
            break;
        case adjusted_hbox_group:
            tex_aux_finish_adjusted_hbox();
            break;
        case vbox_group:
            tex_aux_finish_vbox();
            break;
        case vtop_group:
            tex_aux_finish_vtop();
            break;
        case dbox_group:
            tex_aux_finish_dbox();
            break;
        case align_group:
            tex_finish_alignment_group();
            break;
        case no_align_group:
            tex_finish_no_alignment_group();
            break;
        case output_group:
            tex_aux_finish_output();
            break;
        case math_group:
        case math_component_group:
        case math_stack_group:
            tex_finish_math_group();
            break;
        case discretionary_group:
            tex_finish_discretionary();
            break;
        case insert_group:
            tex_finish_insert_group();
            break;
        case vadjust_group:
            tex_finish_vadjust_group();
            break;
        case vcenter_group:
            tex_finish_vcenter_group();
            break;
        case math_fraction_group:
            tex_finish_math_fraction();
            break;
        case math_radical_group:
            tex_finish_math_radical();
            break;
        case math_operator_group:
            tex_finish_math_operator();
            break;
        case math_choice_group:
            tex_finish_math_choice();
            break;
        case also_simple_group:
        case math_simple_group:
         // cur_group = semi_simple_group; /* probably not needed */
            tex_aux_run_end_group();
            break;
        case semi_simple_group:
        case math_inline_group:
        case math_display_group:
        case math_equation_number_group:
        case math_fence_group: /*tex See above, let's see when we are supposed to end up here. */
            tex_aux_extra_right_brace_error();
            break;
        case local_box_group:
            tex_aux_finish_local_box();
            break;
        default:
            tex_confusion("right brace");
            break;
    }
}

/*tex

    Here is where we clear the parameters that are supposed to revert to their default values after
    every paragraph and when internal vertical mode is entered.

    The multiple widow penalties work from the bottom and club penalties work from the top. Both are 
    persistent arrays while interline penalties acts like for instance looseness and is a more 
    temporary feature. One can argue about how much sense all this makes because it not only affects
    the next paragraph but nested ones too. 

    One argument for the reset i.e. only set inside a paragraph is that  it then can't influence 
    a nested one. In that case resetting the widow and club penalties also makes sense but on the 
    other hand one often wants these to operate over more paragraphs. One solution is to make these 
    parameters current list specific, that is: wipe them as soon as they are bound to the par node, 
    although we can emulate that: frozen set and unfrozen reset. 

*/

void tex_normal_paragraph(int context)
{
    int ignore = 0;
    lmt_main_control_state.last_par_context = context;
    lmt_paragraph_context_callback(context, &ignore);
    if (! ignore) {
        /*tex Why only this subset, maybe we have to check some more now. */
        if (looseness_par) {
            update_tex_looseness(0);
        }
        if (hang_indent_par) {
            update_tex_hang_indent(0);
        }
        if (hang_after_par != 1) {
            update_tex_hang_after(1);
        }
        if (par_shape_par) {
            update_tex_par_shape(null);
        }
        if (inter_line_penalties_par && ! normalize_par_mode_option(keep_interline_penalties_mode)) { 
            update_tex_inter_line_penalties(null);
        }
        if (emergency_left_skip_par) {
            update_tex_emergency_left_skip(null);
        }
        if (emergency_right_skip_par) {
            update_tex_emergency_right_skip(null);
        }
        if (local_interline_penalty_par) {
            update_tex_local_interline_penalty(0);
        }
        if (local_broken_penalty_par) {
            update_tex_local_broken_penalty(0);
        }
        if (local_tolerance_par) {
            update_tex_local_tolerance(0);
        }
        if (local_pre_tolerance_par) {
            update_tex_local_pre_tolerance(0);
        }
        if (single_line_penalty_par) {
            update_tex_single_line_penalty(0);
        }
        if (par_passes_exception_par) {
            update_tex_par_passes_exception(null);
        }
    }
}

/*tex

    The global variable |cur_box| will point to a newly-made box. If the box is void, we will have
    |cur_box = null|. Otherwise we will have |type(cur_box) = hlist_node| or |vlist_node| or
    |rule_node|; the |rule_node| case can occur only with leaders.

    The |box_end| procedure does the right thing with |boxnode|, if |boxcontext| represents the
    context as explained above. The |boxnode| variable is either a list node or a register index.
    In some cases we communicate via a state variable.

*/

static void tex_aux_wrapup_leader_box(halfword boxcontext, halfword boxnode, halfword callback, halfword leaders)
{
    /*tex Append a new leader node that uses |box| and get the next non-blank non-relax. */
    do {
        tex_get_x_token();
    } while (cur_cmd == spacer_cmd || cur_cmd == relax_cmd);
    if ((cur_cmd == hskip_cmd && cur_mode != vmode) || (cur_cmd == vskip_cmd && cur_mode == vmode)) {
        tex_aux_run_glue(); /* uses cur_chr */
        switch (boxcontext) {
            case a_leaders_flag:
                node_subtype(cur_list.tail) = a_leaders;
                break;
            case c_leaders_flag:
                node_subtype(cur_list.tail) = c_leaders;
                break;
            case x_leaders_flag:
                node_subtype(cur_list.tail) = x_leaders;
                break;
            case g_leaders_flag:
                node_subtype(cur_list.tail) = g_leaders;
                break;
            case u_leaders_flag:
                switch (node_type(boxnode)) {
                    case hlist_node:
                        if (cur_mode != vmode) {
                            node_subtype(cur_list.tail) = u_leaders;
                            glue_amount(cur_list.tail) += box_width(boxnode);
                            glue_callback(cur_list.tail) = callback;
                            glue_options(cur_list.tail) = leaders;
                        } else {
                            node_subtype(cur_list.tail) = a_leaders;
                        }
                        break;
                    case vlist_node:
                        if (cur_mode == vmode) {
                            node_subtype(cur_list.tail) = u_leaders;
                            glue_amount(cur_list.tail) += box_total(boxnode);
                            glue_callback(cur_list.tail) = callback;
                            glue_options(cur_list.tail) = leaders;
                        } else {
                            node_subtype(cur_list.tail) = a_leaders;
                        }
                        break;
                    default:
                        /* yet unsupported */
                        node_subtype(cur_list.tail) = a_leaders;
                        break;
                }
                break;
        }
        glue_leader_ptr(cur_list.tail) = boxnode;
    } else {
        tex_handle_error(
            back_error_type,
            "Leaders not followed by proper glue",
            "You should say '\\leaders <box or rule><hskip or vskip>'. I found the <box or\n"
            "rule>, but there's no suitable <hskip or vskip>, so I'm ignoring these leaders."
        );
        tex_flush_node_list(boxnode);
    }
}

void tex_box_end(int boxcontext, halfword boxnode, scaled shift, halfword mainclass, halfword slot, halfword callback, halfword leaders)
{
    cur_box = boxnode;
    switch (boxcontext) {
        case direct_box_flag:
            /*tex

                Append box |boxnode| to the current list, shifted by |boxcontext|. The global variable
                |adjust_tail| will be non-null if and only if the current box might include adjustments
                that should be appended to the current vertical list.

                Having shift in the box context is kind of strange but as long as we stay below |\maxdimen|
                it works. We now pass the shift directly, so no |boxcontext| trick here.

            */
            if (boxnode) {
                if (shift != null_flag) {
                    box_shift_amount(boxnode) = shift;
                }
                switch (cur_mode) {
                    case vmode:
                        if (lmt_packaging_state.pre_adjust_tail) {
                            if (pre_adjust_head != lmt_packaging_state.pre_adjust_tail) {
                                tex_inject_adjust_list(pre_adjust_head, pre_append_adjust_context, 1, boxnode, NULL);
                            }
                            lmt_packaging_state.pre_adjust_tail = null;
                        }
                        if (lmt_packaging_state.pre_migrate_tail) {
                            if (pre_migrate_head != lmt_packaging_state.pre_migrate_tail) {
                                tex_append_list(pre_migrate_head, lmt_packaging_state.pre_migrate_tail);
                            }
                            lmt_packaging_state.pre_migrate_tail = null;
                        }
                        tex_append_to_vlist(boxnode, lua_key_index(box), NULL);
                        if (lmt_packaging_state.post_migrate_tail) {
                            if (post_migrate_head != lmt_packaging_state.post_migrate_tail) {
                                tex_append_list(post_migrate_head, lmt_packaging_state.post_migrate_tail);
                            }
                            lmt_packaging_state.post_migrate_tail = null;
                        }
                        if (lmt_packaging_state.post_adjust_tail) {
                            if (post_adjust_head != lmt_packaging_state.post_adjust_tail) {
                                tex_inject_adjust_list(post_adjust_head, post_append_adjust_context, 1, null, NULL);
                            }
                            lmt_packaging_state.post_adjust_tail = null;
                        }
                        /* what to do here */
                        lmt_packaging_state.except = 0;
                        if (cur_list.mode > nomode) {
                            tex_build_page(box_page_context, 0);
                        }
                        break;
                    case hmode:
                        if (! (box_options(boxnode) & box_option_keep_spacing)) { 
                            cur_list.space_factor = default_space_factor;
                        }
                        tex_couple_nodes(cur_list.tail, boxnode);
                        cur_list.tail = boxnode;
                        break;
                    /* case mmode: */
                    default:
                        boxnode = tex_new_sub_box(boxnode);
                        tex_couple_nodes(cur_list.tail, boxnode);
                        cur_list.tail = boxnode;
                        if (mainclass != unset_noad_class) {
                            tex_set_noad_classes(boxnode, mainclass);
                        }
                        break;
                }
            } else {
                /* just scanning */
            }
            break;
        case box_flag:
            /*tex Store |box| in a local box register  */
            update_tex_box_local(slot, boxnode);
            break;
        case global_box_flag:
            /*tex Store |box| in a global box register  */
            update_tex_box_global(slot, boxnode);
            break;
        case shipout_flag:
            /*tex This normally can't happen as some backend code needs to kick in. */
            if (boxnode) {
                /*tex We just show the box ... */
                tex_begin_diagnostic();
                tex_show_node_list(boxnode, max_integer, max_integer);
                tex_end_diagnostic();
                /*tex ... and wipe it when it's a register ... */
                if (box_register(boxnode)) {
                    tex_flush_node_list(boxnode);
                    box_register(boxnode) = null;
                }
                /*tex ... so there is at least an indication that we flushed. */
            }
            break;
        case left_box_flag:
        case right_box_flag:
        case middle_box_flag:
            /*tex Actualy, this cannot happen ... will go away. */
            tex_aux_finish_local_box();
            break;
        case lua_scan_flag:
            /*tex We are done with scanning so let's return to the caller. */
            tex_aux_wrapup_local_scan_box();
            cur_box = boxnode;
            break;
        case a_leaders_flag:
        case c_leaders_flag:
        case x_leaders_flag:
        case g_leaders_flag:
        case u_leaders_flag:
            tex_aux_wrapup_leader_box(boxcontext, boxnode, callback, leaders);
            break;
        default:
            /* fatal error */
            break;
    }
}

/*tex

    The canonical \TEX\ engine(s) inject an indentation box, so there is always something at the 
    beginning that also acts as a boundary. However, when snapshotting was introduced it made also 
    sense to turn the |\parindent| related hlist into a glue. We might need to adapt the parbuilder 
    but it looks like hat is not needed. Of course, an |\unskip| will now also remove the
    |\parindent| but there are ways to prevent this. I'll test it for a while, which is why we have 
    a way to enable it. The glue is {\em always} injected, also when it's zero.

*/

void tex_begin_paragraph(int doindent, int context)
{
    int indented = doindent;
    int isvmode = cur_list.mode == vmode;
    lmt_main_control_state.last_par_trigger = context; /* added */
    if (isvmode || cur_list.head != cur_list.tail) {
        /*tex
            Actually we could remove the callback and hook it into the |\everybeforepar| but that one
            started out as a |tex.expandmacro| itself and we don't want the callback overhead every
            time, so now we have both. However, in the end I decided to do this one {\em before} the
            parskip is injected.
        */
        if (every_before_par_par) {
            tex_begin_inserted_list(tex_get_available_token(token_val(end_local_cmd, 0)));
            tex_begin_token_list(every_before_par_par, every_before_par_text);
            if (tracing_nesting_par > 2) {
                tex_local_control_message("entering local control via \\everybeforepar");
            }
            tex_local_control(1);
// tex_cleanup_input_state();
        }
        tex_tail_append(tex_new_param_glue_node(par_skip_code, par_skip_glue));
    }
    lmt_begin_paragraph_callback(isvmode, &indented, context);
    /*tex We'd better not messed up things in the callback! */
    { 
        halfword prev_graf = cur_list.prev_graf;
        cur_list.prev_graf = 0;
        tex_push_nest();
        cur_list.mode = hmode;
        cur_list.space_factor = default_space_factor;
        /*tex Add local paragraph node */
        tex_tail_append(tex_new_par_node(vmode_par_par_subtype));
        par_prev_graf(cur_list.tail) = prev_graf;
        /*tex Dir nodes end up before the indent box. */
        tex_append_dir_state();
    }
    tex_aux_insert_parindent(indented);
    if (tracing_paragraph_lists) {
        tex_begin_diagnostic();       
     // tex_print_format("[paragraph: start, context %i]", context);
        tex_print_format("[paragraph: start, context %s]", lmt_interface.par_trigger_values[context].name);
        tex_show_box(node_next(cur_list.head));
        tex_end_diagnostic();
    }
    /*tex The |\everypar| tokens are injected after all these nodes have been added. */
    if (every_par_begin_par) {
        tex_begin_token_list(every_par_begin_par, every_par_begin_text);
    }
    if (every_par_par) {
        tex_begin_token_list(every_par_par, every_par_text);
    }
    if (lmt_nest_state.nest_data.ptr == 1) {
        /*tex put |par_skip| glue on current page */
        tex_build_page(begin_paragraph_page_context, 0);
    }
}

void tex_insert_paragraph_token(void)
{
    if (auto_paragraph_mode_par > 0) {
        cur_tok = token_val(end_paragraph_cmd, inserted_end_paragraph_code);
     // cur_tok = token_val(end_paragraph_cmd, normal_end_paragraph_code);
     // cur_cs  = null;
    } else {
        cur_tok = lmt_token_state.par_token;
    }
}

static void tex_aux_run_head_for_vmode(void)
{
    if (cur_list.mode >= nomode) {
        tex_back_input(cur_tok);
        /*tex
            We could have a callback here but on the other hand, we really need to be in vmode
            afterwards! Also, a macro package can just test for the mode at that spot which is
            less hassle than making a callback identify what is needed. A return value would
            indicate to not inject a par when we're in vmode and only very dirty \LUA\ code can
            change modes here by messing with the list so far. So, unless I find a real use case
            we just continue.
        */
        tex_insert_paragraph_token();
        /* 
            An old cheat: we need to allocate a token which is what back_input does, but then we 
            mark it as inserted. 
        */
        tex_back_input(cur_tok);
        lmt_input_state.cur_input.token_type = inserted_text;
        /* 
            tex_insert_input(tex_get_available_token(cur_tok)); // cleaner 
        */
    } else if (cur_cmd != hrule_cmd) {
        tex_off_save();
    } else {
        tex_handle_error(
            normal_error_type,
            "You can't use '\\hrule' here except with leaders",
            "To put a horizontal rule in an hbox or an alignment, you should use \\leaders or\n"
            "\\hrulefill (see The TeXbook)."
        );
    }
}

/*tex

    We don't have |hkern_cmd| and |vkern_cmd| and it makes no sense to introduce them now so instead
    of handling modes in the big switch we do it here. Because we need to be compatible we would end
    up with three |cmd| codes anyway. The rationale for |\hkern| and |\vkern| is consistency of
    primitives, while |\nonzerowidthkern| can make node lists smaller which is nice for \LUA\ based
    juggling.

*/

/*
static void tex_aux_run_kern(void)
{
    halfword val = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
    tex_tail_append(tex_new_kern_node(val, explicit_kern));
}
*/

static void tex_aux_run_kern(void)
{
    halfword code = cur_chr;
    switch (code) {
        /*tex Finally enabled: */
        case h_kern_code:
            if (cur_mode == vmode) {
                tex_back_input(token_val(kern_cmd, normal_kern_code));
                tex_back_input(token_val(begin_paragraph_cmd, quitvmode_par_code));
                return;
            } else { 
                break;
            }
        case v_kern_code:
            if (cur_mode == hmode) {
                tex_back_input(token_val(kern_cmd, normal_kern_code));
                tex_back_input(token_val(end_paragraph_cmd, normal_end_paragraph_code));
                return;
            } else { 
                break;
            }
    }
    { 
        scaled val = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
        if (code == non_zero_width_kern_code && ! val) { 
            return;
        } else { 
            tex_tail_append(tex_new_kern_node(val, explicit_kern_subtype));
        }
    }
}

static void tex_aux_run_mkern(void)
{
    halfword val = tex_scan_dimension(1, 0, 0, 0, NULL, NULL);
    tex_tail_append(tex_new_kern_node(val, explicit_math_kern_subtype));
}

/*tex

    |cur_list.dirs| would have been set by |line_break| by means of |post_line_break|, but this is
    not done right now, as it introduces pretty heavy memory leaks. This means the current code
    might be wrong in some way that relates to in-paragraph displays.

    Instead of |tex_aux_only_dirs| we now use a configurable |tex_is_effectively_empty| so that we
    can test cq .\ document this trickery. It also gives bit more control over hoe to deal with 
    indentation (old school \TEX). 

*/

void tex_end_paragraph(int group, int context)
{
    if (cur_list.mode == hmode) {
        if (cur_list.head == cur_list.tail) {
            /*tex |null| paragraphs are ignored, all contain a |par| node */
            tex_pop_nest();
        } else if (empty_paragraph_mode_par && tex_is_effectively_empty(node_next(cur_list.head), empty_paragraph_mode_par)) {
            tex_flush_node_list(node_next(cur_list.head));
         /* cur_list.tail = cur_list.head; */
            tex_pop_nest();
         // if (cur_list.head == cur_list.tail || node_next(cur_list.head) == cur_list.tail) {
         //     if (node_next(cur_list.head) == cur_list.tail) {
         //         tex_flush_node(node_next(cur_list.head));
         //      // cur_list.tail = cur_list.head;
         //     }
         //     tex_pop_nest();
        } else {
            halfword tail = cur_list.tail;
            while (tail != cur_list.head && node_type(tail) == par_node && node_subtype(tail) == local_break_par_subtype) { 
                cur_list.tail = node_prev(tail);
                node_next(cur_list.tail) = null;
                tex_flush_node(tail);
                tail = cur_list.tail;
            }
            tex_line_break(group, context, 0);
        }
        if (cur_list.direction_stack) {
            tex_flush_node_list(cur_list.direction_stack);
            cur_list.direction_stack = null;
        }
        tex_normal_paragraph(context);
        lmt_error_state.error_count = 0;
    }
}

static void tex_aux_run_penalty(void)
{
    halfword code = cur_chr;
    switch (code) {
        /*tex Finally enabled: */
        case h_penalty_code:
            if (cur_mode == vmode) {
                tex_back_input(token_val(penalty_cmd, normal_penalty_code));
                tex_back_input(token_val(begin_paragraph_cmd, quitvmode_par_code));
                return;
            } else { 
                break;
            }
        case v_penalty_code:
            if (cur_mode == hmode) {
                tex_back_input(token_val(penalty_cmd, normal_penalty_code));
                tex_back_input(token_val(end_paragraph_cmd, normal_end_paragraph_code));
                return;
            } else { 
                break;
            }
    }
    {
        halfword value = tex_scan_integer(0, NULL, NULL);
        tex_tail_append(tex_new_penalty_node(value, user_penalty_subtype));
        if (cur_list.mode == vmode) {
            tex_build_page(penalty_page_context, 0);
        }
    }
}

/*tex

    When |delete_last| is called, |cur_chr| is the |type| of node that will be deleted, if present.
    The |remove_item| command removes a penalty, kern, or glue node if it appears at the tail of
    the current list, using a brute-force linear scan. Like |\lastbox|, this command is not allowed
    in vertical mode (except internal vertical mode), since the current list in vertical mode is
    sent to the page builder. But if we happen to be able to implement it in vertical mode, we do.

*/

static void tex_aux_run_remove_item(void)
{
    halfword code = cur_chr;
    halfword head = cur_list.head;
    halfword tail = cur_list.tail;
    if (cur_list.mode == vmode && tail == head) {
        /*tex
            Apologize for inability to do the operation now, unless |\unskip|
            follows non-glue. It's a bit weird test.
        */
        if ((code != skip_item_code) || (lmt_page_builder_state.last_glue != max_halfword)) {
            switch (code) {
                case kern_item_code:
                    tex_you_cant_error(
                        "Sorry...I usually can't take things from the current page.\n"
                        "Try '\\kern-\\lastkern' instead."
                    );
                    break;
                case penalty_item_code:
                case boundary_item_code:
                    tex_you_cant_error(
                        "Sorry...I usually can't take things from the current page.\n"
                        "Perhaps you can make the output routine do it."
                    );
                    break;
                case skip_item_code:
                    tex_you_cant_error(
                        "Sorry...I usually can't take things from the current page.\n"
                        "Try '\\vskip-\\lastskip' instead."
                    );
                    break;
            }
        }
//    } else if (node_type(tail) != glyph_node) {
//        /*tex
//            Officially we don't need to check what we remove because it can be only one of
//            three, unless one creates one incidentally (in \LUA). So, we just do check and
//            silently ignore bad code.
//        */
//        halfword p;
//        switch (code) {
//            case kern_item_code    : if (node_type(tail) != kern_node   ) { return; } else { break; }
//            case penalty_item_code : if (node_type(tail) != penalty_node) { return; } else { break; }
//            case skip_item_code    : if (node_type(tail) != glue_node   ) { return; } else { break; }
//        }
//        /*tex
//            There is some magic testing here that makes sure we don't mess up any discretionary
//            nodes. But why do we care?
//        */
//        do {
//            p = head;
//            if (p == tail && node_type(head) == disc_node) {
//                return;
//            } else {
//                head = node_next(p);
//            }
//        } while (head != tail);
//        node_next(p) = null;
//        tex_flush_node_list(tail);
//        cur_list.tail = p;
//    }
    } else {
//  } else if (tail != head) {
        /*tex
            Officially we don't need to check what we remove because it can be only one of
            three, unless one creates one incidentally (in \LUA). So, we just do check and
            silently ignore bad code.
        */
        switch (node_type(tail)) {
            case kern_node     :
                if (code == kern_item_code) {
                    break;
                } else {
                    return;
                }
            case penalty_node  :
                if (code == penalty_item_code) {
                    break;
                } else {
                    return;
                }
            case glue_node     :
                if (code == skip_item_code) {
                    break;
                } else {
                    return;
                }
            case boundary_node :
                if (code == boundary_item_code && node_subtype(tail) == user_boundary) {
                    break;
                } else {
                    return;
                }
            default:
                return;
        }
        tex_flush_node(tail);
        tex_pop_tail();
//        {
//            /*tex
//                There is some magic testing here that makes sure we don't mess up any discretionary
//                nodes. But why do we care?
//            */
//            halfword p;
//            do {
//                p = head;
//                if (p == tail && node_type(head) == disc_node) {
//                    return;
//                } else {
//                    head = node_next(p);
//                }
//            } while (head != tail);
//            node_next(p) = null;
//            tex_flush_node_list(tail);
//            cur_list.tail = p;
//        }
    }
}

/*tex

    Italic corrections are converted to kern nodes when the |italic_correction| command follows a
    character. In math mode the same effect is achieved by appending a kern of zero here, since
    italic corrections are supplied later.

    The callback can take care of |\glyphslant| usage because it is up to the macro package to come 
    up with a decent heuristic. 

*/

static void tex_aux_run_text_italic_correction(void)
{
    halfword tail = cur_list.tail;
    scaled kern = 0;
    quarterword subtype = 0;
    switch (cur_chr) { 
        case italic_correction_code: 
            if (tail == cur_list.head) { 
                return; 
            } else {
                subtype = italic_kern_subtype;
                break;
            }
        case left_correction_code:
            subtype = left_correction_kern_subtype;
            break;
        case right_correction_code:
            subtype = right_correction_kern_subtype;
            break;
        default: 
            return;
    }
    if (subtype == italic_kern_subtype) {
        switch (node_type(tail)) { 
            case glyph_node: 
                kern = tex_char_italic_from_glyph(tail); /* scaled */
                if (cur_mode == mmode) { 
                    /*tex 
                        Here we are just compatible and in \CONTEXT\ we don't have italic 
                        corrections in math anyway. 
                    */
                    break;
                } else if (tex_has_glyph_option(tail, glyph_option_no_italic_correction)) { /* && cur_chr != right_correction_code */
                    break;
                } else {
                    /*tex
                        We pass the identified value but in the case of \OPENTYPE\ fonts or slant
                        control we have to cook up some value ourselves (if at all). 
                    */
                    kern = lmt_italic_correction_callback(tail, kern, subtype);
                    if (kern || ! tex_has_glyph_option(tail, glyph_option_no_zero_italic_correction)) { 
                        break;
                    } else { 
                        return;
                    }
                }
            case disc_node: 
                /*tex 
                    Just in case we want this but here checking has to be done in the callback 
                    unless we check replace and post here. 
                */
                kern = lmt_italic_correction_callback(tail, 0, subtype);
                if (kern) { 
                    break;
                } else { 
                    return;
                }
            default: 
                return; 
        }
    }
    /*tex 
        When we end up here the decision has been made to inject a kern, it being zero or 
        otherwise. What to do with multiple corrections. Best not mess with heuristics here. 
    */
    tex_tail_append(tex_new_kern_node(kern, subtype));
    /*tex 
        We might want to add some properties to the kern node or maybe register it as a todo. The 
        overhead of a callback can be neglected because we don't have them many of them. We could 
        assign a returned value but ... in the end these correction kerns are just signals. 
    */
    if (subtype != italic_kern_subtype) {
        lmt_italic_correction_callback(cur_list.tail, kern, subtype);
    }
}

/*tex

    The positioning of accents is straightforward but tedious. Given an accent of width |a|,
    designed for characters of height |x| and slant |s|; and given a character of width |w|,
    height |h|, and slant |t|: We will shift the accent down by |x - h|, and we will insert kern
    nodes that have the effect of centering the accent over the character and shifting the accent
    to the right by $\delta = {1 \over 2} (w-a) + h \cdot t - x \cdot s$. If either character is
    absent from the font, we will simply use the other, without shifting.

    While much is delegated to builders this is one of the few places where the action happens
    directly. Of course, in a \UNICODE\ engine this command is not really relevant but here we
    even extended it with optional offsets!

*/

static void tex_aux_run_text_accent(void)
{
    halfword fnt = cur_font_par;
    halfword accent = null;
    halfword base = null;
    scaled xoffset = 0;
    scaled yoffset = 0;
    while (1) {
        switch (tex_scan_character("xyXY", 0, 1, 0)) {
            case 'x': case 'X':
                if (tex_scan_mandate_keyword("xoffset", 1)) {
                    xoffset = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            case 'y': case 'Y':
                if (tex_scan_mandate_keyword("yoffset", 1)) {
                    yoffset = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                }
                break;
            default:
                goto DONE;
        }
    }
  DONE:
    accent = tex_new_char_node(glyph_unset_subtype, fnt, tex_scan_char_number(0), 1);
    if (accent) {
        /*tex
            Create a character node |q| for the next character, but set |q := null| if problems
            arise.
        */
        scaled x = tex_get_scaled_ex_height(fnt);
        double s = (double) (tex_get_font_slant(fnt)) / (double) (65536);
        scaled a = tex_glyph_width(accent);
        /*tex
            Here we had |handle_assignments| which is a bit confusing one so we inlined it, probably
            at the cost of some error recovery compatibility, which we don't worry too much about.
            It looks like skipping spaces and relax is okay. The (original \TEX\ idea is that one
            can change a font in between which is why the |fnt| variable gets set again. Because in
            practice switching a font can involve more than assignments wd could be more tolerant
            and often wrapping in |\localcontrolled| is more robust then.
        */
     /* handle_assignments(); */
        fnt = cur_font_par;
      PICKUP:
        switch (cur_cmd) {
            case spacer_cmd:
            case relax_cmd:
                tex_get_x_token();
                goto PICKUP;
            case letter_cmd:
            case other_char_cmd:
            case char_given_cmd:
                base = tex_new_glyph_node(glyph_unset_subtype, fnt, cur_chr, accent);
                break;
            case char_number_cmd:
                /* We don't accept keywords for |\glyph|. */
                base = tex_new_glyph_node(glyph_unset_subtype, fnt, tex_scan_char_number(0), accent);
                break;
            default:
                /* compatibility hack, not that useful nowadays  */
                if (cur_cmd <= max_non_prefixed_cmd) {
                    tex_back_input(cur_tok);
                    break;
                } else {
                    lmt_error_state.set_box_allowed = 0;
                    tex_run_prefixed_command();
                    lmt_error_state.set_box_allowed = 0;
                    goto PICKUP;
                }
        }
        if (base) {
            /*tex
                Append the accent with appropriate kerns, then set |p := q|. The kern nodes
                appended here must be distinguished from other kerns, lest they be wiped away by
                the hyphenation algorithm or by a previous line break. The two kerns are computed
                with (machine dependent) |real| arithmetic, but their sum is machine independent;
                the net effect is machine independent, because the user cannot remove these nodes
                nor access them via |\lastkern|.

                This goes away: not listening to scaled yet.

             */
            double t = (double) (tex_get_font_slant(fnt)) / (double) (65536); /* amount of slant */
            scaled w = tex_glyph_width(base);
            scaled h = tex_glyph_height(base);
            scaled delta = glueround((double) (w - a) / (double) (2) + h * t - x * s);
            halfword left = tex_new_kern_node(delta, accent_kern_subtype);
            halfword right = tex_new_kern_node(- a - delta, accent_kern_subtype);
            glyph_x_offset(accent) = xoffset;
            glyph_y_offset(accent) = yoffset;
            if (h != x) {
                /*tex the accent must be shifted up or down */
             // accent = hpack(accent, 0, packing_additional, direction_unknown);
             // box_shift_amount(accent) = x - h;
                glyph_y_offset(accent) += x - h;
            }
            tex_couple_nodes(cur_list.tail, left);
            tex_couple_nodes(left, accent);
            tex_couple_nodes(accent, right);
            tex_couple_nodes(right,base);
            cur_list.tail = base;
        } else {
            tex_couple_nodes(cur_list.tail, accent);
            cur_list.tail = accent;
        }
        cur_list.space_factor = default_space_factor;
    }
}

/*tex Finally, |\endcsname| is not supposed to get through to |main_control|. */

static void tex_aux_run_cs_error(void)
{
    tex_handle_error(
        normal_error_type,
        "Extra \\endcsname",
        "I'm ignoring this, since I wasn't doing a \\csname."
    );
}

/*tex

    Assignments to values in |eqtb| can be global or local. Furthermore, a control sequence can
    be defined to be |\long|, |\protected|, or |\outer|, and it might or might not be expanded.
    The prefixes |\global|, |\long|, |\protected|, and |\outer| can occur in any order. Therefore
    we assign binary numeric codes, making it possible to accumulate the union of all specified
    prefixes by adding the corresponding codes. (\PASCAL's |set| operations could also have been
    used.)

    Every prefix, and every command code that might or might not be prefixed, calls the action
    procedure |prefixed_command|. This routine accumulates a sequence of prefixes until coming to
    a non-prefix, then it carries out the command.

*/

void tex_inject_text_or_line_dir(int val, int check_glue)
{
    if (cur_mode == hmode && internal_dir_state_par > 0) {
        /*tex |tail| is non zero but we test anyway. */
        halfword dirn = tex_new_dir(cancel_dir_subtype, text_direction_par);
        halfword tail = cur_list.tail;
        if (check_glue && tail && node_type(tail) == glue_node) { // && node_subtype(tail) != indent_skip_glue
            halfword prev = node_prev(tail);
            tex_couple_nodes(prev, dirn);
            tex_couple_nodes(dirn, tail);
        } else {
            tex_tail_append(dirn);
        }
    }
    tex_push_text_dir_ptr(val);
    if (cur_mode == hmode) {
        halfword dir = tex_new_dir(normal_dir_subtype, val);
        dir_level(dir) = cur_level;
        tex_tail_append(dir);
    }
}

static void tex_aux_show_frozen_error(halfword cs)
{
    if (cs) {
        tex_handle_error(
            normal_error_type,
            "You can't redefine the frozen macro %S.", cs,
            NULL
        );
    } else {
        tex_handle_error(
            normal_error_type,
            "You can't redefine a frozen macro.",
            NULL
        );
    }
}

/*tex

    We use the fact that |register| $<$ |advance| $<$ |multiply| $<$ |divide| We compute the
    register location |l| and its type |p| but |return| if invalid. Here we use the fact that
    the consecutive codes |int_val .. mu_val| and |assign_int .. assign_mu_glue| correspond
    to each other nicely.

*/

static inline halfword tex_aux_get_register_index(int level, halfword *index)
{
    switch (level) {
        case integer_val_level:
            *index = tex_scan_integer_register_number();
            return register_integer_location(*index);
        case dimension_val_level:
            *index = tex_scan_dimension_register_number();
            return register_dimension_location(*index);
        case attribute_val_level:
            *index = tex_scan_attribute_register_number();
            return register_attribute_location(*index);
        case posit_val_level:
            *index = tex_scan_posit_register_number();
            return register_posit_location(*index);
        case glue_val_level:
            *index = tex_scan_glue_register_number();
            return register_glue_location(*index);
        case muglue_val_level:
            *index = tex_scan_muglue_register_number();
            return register_muglue_location(*index);
        case token_val_level:
            *index = tex_scan_toks_register_number();
            return register_toks_location(*index);
        default:
            return 0;
    }
}

static inline halfword tex_aux_get_register_value(int level, int optionalequal)
{
    switch (level) {
        case integer_val_level:
        case attribute_val_level:
            return tex_scan_integer(optionalequal, NULL, NULL);
        case posit_val_level:
            return tex_scan_posit(optionalequal);
        case dimension_val_level:
            return tex_scan_dimension(0, 0, 0, optionalequal, NULL, NULL);
        default:
            return tex_scan_glue(level, optionalequal, 1);
    }
}

static int tex_aux_valid_arithmic(int cmd, int *index, int *level, int *varcmd, int *simple, int *original, halfword *slot)
{
    /*tex So: |\multiply|, |\divide| or |\advance|. */
    tex_get_x_token();
    *varcmd = cur_cmd;
 /* *simple = 0; */
    switch (cur_cmd) {
        case register_integer_cmd:
        case internal_integer_cmd:
            *index = cur_chr;
            *level = integer_val_level;
            *original = eq_value(*index);
            return 1;
        case register_attribute_cmd:
        case internal_attribute_cmd:
            *index = cur_chr;
            *level = attribute_val_level;
            *original = eq_value(*index);
            return 1;
        case register_posit_cmd:
        case internal_posit_cmd:
            *index = cur_chr;
            *level = posit_val_level;
            *original = eq_value(*index);
            return 1;
        case register_dimension_cmd:
        case internal_dimension_cmd:
            *index = cur_chr;
            *level = dimension_val_level;
            *original = eq_value(*index);
            return 1;
        case register_glue_cmd:
        case internal_glue_cmd:
            *index = cur_chr;
            *level = glue_val_level;
            *original = eq_value(*index);
            return 1;
        case register_muglue_cmd:
        case internal_muglue_cmd:
            *index = cur_chr;
            *level = muglue_val_level;
            *original = eq_value(*index);
            return 1;
        case register_cmd:
            *level = cur_chr;
            *index = tex_aux_get_register_index(cur_chr, slot);
            *original = eq_value(*index);
            return 2;
        case integer_cmd:
            *index = cur_cs;
            *level = integer_val_level;
            *original = cur_chr;
            *simple = integer_cmd;
            return 1;
     // case index_cmd:
     //     *index = cur_cs;
     //     *level = integer_val_level;
     //     *original = cur_chr;
     //     *simple = index_cmd;
     //     return 1;
        case dimension_cmd:
            *index = cur_cs;
            *level = dimension_val_level;
            *original = cur_chr;
            *simple = dimension_cmd;
            return 1;
        case posit_cmd:
            *index = cur_cs;
            *level = posit_val_level;
            *original = cur_chr;
            *simple = posit_cmd;
            return 1;
        case gluespec_cmd:
            *index = cur_cs;
            *level = glue_val_level;
            *original = cur_chr;
            *simple = gluespec_cmd;
            return 1;
        case mugluespec_cmd:
            *index = cur_cs;
            *level = muglue_val_level;
            *original = cur_chr;
            *simple = mugluespec_cmd;
            return 1;
        default:
            tex_handle_error(
                normal_error_type,
                "You can't use '%C' after %C",
                cur_cmd, cur_chr, cmd, 0,
                "I'm forgetting what you said and not changing anything."
            );
            return 0;
    }
}

static void tex_aux_arithmic_overflow_error(int level, halfword value)
{
    if (level >= glue_val_level) {
        tex_flush_node(value);
    }
    tex_handle_error(
        normal_error_type,
        "Arithmetic overflow",
        "I can't carry out that multiplication or division, since the result is out of\n"
        "range."
    );
}

static inline void tex_aux_update_register(int a, int level, halfword index, halfword value, halfword cmd)
{
    switch (level) {
        case integer_val_level:
            tex_word_define(a, index, value);
            if (is_frozen(a) && cmd == internal_integer_cmd && cur_mode == hmode) {
                tex_update_par_par(internal_integer_cmd, index - lmt_primitive_state.prim_data[cmd].offset);
            }
            break;
        case attribute_val_level:
            if ((register_attribute_number(index)) > lmt_node_memory_state.max_used_attribute) {
                lmt_node_memory_state.max_used_attribute = register_attribute_number(index);
            }
            tex_change_attribute_register(a, index, value);
            tex_word_define(a, index, value);
            break;
        case posit_val_level:
            tex_word_define(a, index, value);
            if (is_frozen(a) && cmd == internal_posit_cmd && cur_mode == hmode) {
                tex_update_par_par(internal_posit_cmd, index - lmt_primitive_state.prim_data[cmd].offset);
            }
            break;
        case dimension_val_level:
            tex_word_define(a, index, value);
            if (is_frozen(a) && cmd == internal_dimension_cmd && cur_mode == hmode) {
                tex_update_par_par(internal_dimension_cmd, index - lmt_primitive_state.prim_data[cmd].offset);
            }
            break;
        case glue_val_level:
            tex_define(a, index, cmd == internal_glue_cmd ? internal_glue_reference_cmd : register_glue_reference_cmd, value);
            if (is_frozen(a) && cmd == internal_glue_cmd && cur_mode == hmode) {
                tex_update_par_par(internal_glue_cmd,  index - lmt_primitive_state.prim_data[cmd].offset);
            }
            break;
        case muglue_val_level:
            tex_define(a, index, cmd == internal_glue_cmd ? internal_muglue_reference_cmd : register_muglue_reference_cmd, value);
            break;
        default:
            /* can't happen */
            tex_word_define(a, index, value);
            break;
    }
}

static inline void tex_aux_set_register(int a)
{
    halfword level = cur_chr;
    halfword varcmd = cur_cmd;
    halfword slot = 0;
    halfword index = tex_aux_get_register_index(level, &slot);
    halfword value = tex_aux_get_register_value(level, 1);
    tex_aux_update_register(a, level, index, value, varcmd);
}

/*tex 
    The by-less variant is more efficient as there is no push back of the token when there is not 
    such keyword. It goes unnoticed on an average run but not on the total runtime of 15.000 
    (times 2) runs of a 30 page \CONTEXT\ document with plenty of complex tables. So this is one 
    of the few obscure optimizations I grand myself (if only to be able to discuss it). The same is
    true for the zero and one checks, it saves some save stack although we check for that later
    anyway. 
*/

static void tex_aux_arithmic_register(int a, int code)
{
    halfword cmd = cur_cmd;
    halfword level = cur_chr;
    halfword index = 0;
    halfword varcmd = 0;
    halfword simple = 0;
    halfword original = 0;
    halfword slot = 0;
    int okay = tex_aux_valid_arithmic(cmd, &index, &level, &varcmd, &simple, &original, &slot);
    if (okay) {
        if ((okay == 1 && tex_mutation_permitted(index)) || tex_register_permitted(index, slot, cmd)) {
            halfword value = null;
         // lmt_scanner_state.arithmic_error = 0;
            switch (code) {
                case advance_code:
                    tex_scan_optional_keyword("by");
                case advance_by_code:
                    {
                        halfword amount = tex_aux_get_register_value(level, 0);
                        switch (level) {
                            case integer_val_level:
                            case attribute_val_level:
                            case dimension_val_level:
                                if (is_global(a) || amount) {
                                    value = original + amount;
                                    break;
                                } else { 
                                    /* likely a dimension */
                                    return;
                                }
                            case posit_val_level:
                                if (is_global(a) || tex_posit_ne_zero(amount)) {
                                    value = tex_posit_add(original, amount); 
                                    break;
                                } else { 
                                    return;
                                }
                            case glue_val_level:
                            case muglue_val_level:
                                if (is_global(a) || ! tex_glue_is_zero(amount)) {
                                    /* Compute the sum of two glue specs */
                                    glue_amount(amount) += glue_amount(original);
                                    if (glue_stretch(amount) == 0) {
                                        glue_stretch_order(amount) = normal_glue_order;
                                    }
                                    if (glue_stretch_order(amount) == glue_stretch_order(original)) {
                                        glue_stretch(amount) += glue_stretch(original);
                                    } else if ((glue_stretch_order(amount) < glue_stretch_order(original)) && (glue_stretch(original))) {
                                        glue_stretch(amount) = glue_stretch(original);
                                        glue_stretch_order(amount) = glue_stretch_order(original);
                                    }
                                    if (glue_shrink(amount) == 0) {
                                        glue_shrink_order(amount) = normal_glue_order;
                                    }
                                    if (glue_shrink_order(amount) == glue_shrink_order(original)) {
                                        glue_shrink(amount) += glue_shrink(original);
                                    } else if ((glue_shrink_order(amount) < glue_shrink_order(original)) && (glue_shrink(original))) {
                                        glue_shrink(amount) = glue_shrink(original);
                                        glue_shrink_order(amount) = glue_shrink_order(original);
                                    }
                                    value = amount;
                                    break;
                                } else {
                                    tex_flush_node(amount);
                                    return;
                                }
                            default:
                                /* error */
                                break;
                        }
                        /*tex There is no overflow detection for addition, just wraparound. */
                        if (simple) {
                            tex_define(a, index, (singleword) simple, value);
                        } else {
                            tex_aux_update_register(a, level, index, value, varcmd);
                        }
                        break;
                    }
                case multiply_code:
                    tex_scan_optional_keyword("by");
                case multiply_by_code:
                    {
                        halfword amount = tex_scan_integer(0, NULL, NULL);
                        halfword value = 0;
                        if (is_global(a) || amount != 1) {
                            lmt_scanner_state.arithmetic_error = 0;
                            switch (level) {
                                case integer_val_level:
                                case attribute_val_level:
                                    value = tex_multiply_integers(original, amount);
                                    break;
                                case posit_val_level:
                                    value = tex_posit_mul_by(original, amount);
                                    break;
                                case dimension_val_level:
                                    value = tex_nx_plus_y(original, amount, 0);
                                    break;
                                case glue_val_level:
                                case muglue_val_level:
                                    {
                                        halfword newvalue = tex_new_glue_spec_node(original);
                                        glue_amount(newvalue) = tex_nx_plus_y(glue_amount(original), amount, 0);
                                        glue_stretch(newvalue) = tex_nx_plus_y(glue_stretch(original), amount, 0);
                                        glue_shrink(newvalue) = tex_nx_plus_y(glue_shrink(original), amount, 0);
                                        value = newvalue;
                                        break;
                                    }
                                default:
                                    /* error */
                                    break;
                            }
                            if (lmt_scanner_state.arithmetic_error) {
                                tex_aux_arithmic_overflow_error(level, value);
                            } else if (simple) {
                                tex_define(a, index, (singleword) simple, value);
                            } else {
                                tex_aux_update_register(a, level, index, value, varcmd);
                            }
                            break;
                        } else { 
                            return;
                        }
                    }
                case divide_code:
                case r_divide_code:
                case e_divide_code:
                    tex_scan_optional_keyword("by");
                case divide_by_code:
                case r_divide_by_code:
                case e_divide_by_code:
                    {
                        halfword amount = tex_scan_integer(0, NULL, NULL);
                        if (is_global(a) || amount != 1) {
                            bool rounded = code == r_divide_code || code == r_divide_by_code;
                            lmt_scanner_state.arithmetic_error = 0;
                            switch (level) {
                                case dimension_val_level:
                                    if (rounded) {
                                        value = tex_quotient(original >> 16, amount, 1) << 16;
                                        break;
                                    }
                                case integer_val_level:
                                case attribute_val_level:
                                    {
                                        bool asexpr = code == e_divide_code || code == e_divide_by_code;
                                        value = tex_quotient(original, amount, asexpr || rounded);
                                        break;
                                    }
                                case posit_val_level:
                                    value = tex_posit_div_by(original, amount); /* always rounded */
                                    break;
                                case glue_val_level:
                                case muglue_val_level:
                                    {
                                        halfword newvalue = tex_new_glue_spec_node(original);
                                        /* we could shift over 0 when not rounded but .. why bother */
                                        if (rounded) {
                                            glue_amount(newvalue) = tex_quotient(glue_amount(original) >> 16, amount, 1) << 16;
                                            glue_stretch(newvalue) = tex_quotient(glue_stretch(original) >> 16, amount, 1) << 16;
                                            glue_shrink(newvalue) = tex_quotient(glue_shrink(original) >> 16, amount, 1) << 16;
                                        } else {
                                            bool asexpr = code == e_divide_code || code == e_divide_by_code;
                                            glue_amount(newvalue) = tex_quotient(glue_amount(original), amount, asexpr);
                                            glue_stretch(newvalue) = tex_quotient(glue_stretch(original), amount, asexpr);
                                            glue_shrink(newvalue) = tex_quotient(glue_shrink(original), amount, asexpr);
                                        }
                                        value = newvalue;
                                        break;
                                    }
                                default:
                                    /* error */
                                    break;
                            }
                            if (lmt_scanner_state.arithmetic_error) {
                                tex_aux_arithmic_overflow_error(level, value);
                            } else if (simple) {
                                tex_define(a, index, (singleword) simple, value);
                            } else {
                                tex_aux_update_register(a, level, index, value, varcmd);
                            }
                            break;
                        } else { 
                            return;
                        }
                    }
                /*
                case advance_by_plus_one_code:
                case advance_by_minus_one_code:
                    {
                        switch (level) {
                            case integer_val_level:
                            case attribute_val_level:
                                original += code == advance_by_plus_one_code ? 1 : -1;
                                if (simple) {
                                    tex_define(a, index, simple, original);
                                } else {
                                    tex_aux_update_register(a, level, index, original, varcmd);
                                }
                                break;
                        }
                        break;
                    }
                */
            }
        }
    }
}

/*tex
    The value of |c| is 0 for |\deadcycles|, 1 for |\insertpenalties|, etc. In traditional \TEX\
    the interaction mode is set by primitives so no checking is needed. However, in \ETEX\ the
    value can be set. As a consequence there is an error message for wrong values but here we
    just clip the values. After all, we can also set values from \LUA\ so either we bark or we
    just recover. So, gone is:

    \starttyping
    handle_error_int(
        normal_error_type,
        "Bad interaction mode (", val, ")",
        "Modes are 0=batch, 1=nonstop, 2=scroll, and 3=errorstop. Proceed, and I'll ignore\n"
        "this case."
    );
    \stoptyping

    I could have decided to ignore bad values but clipping is probably better.

*/

static inline void tex_aux_set_interaction(halfword mode)
{
    tex_print_ln();
    if (mode < batch_mode) {
        lmt_error_state.interaction = batch_mode;
    } else if (mode > error_stop_mode) {
        lmt_error_state.interaction = error_stop_mode;
    } else {
        lmt_error_state.interaction = mode;
    }
    tex_fixup_selector(lmt_fileio_state.log_opened);
}

static void tex_aux_set_page_property(void)
{
    switch (cur_chr) {
        case page_goal_code:
            lmt_page_builder_state.goal = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_vsize_code:
            lmt_page_builder_state.vsize = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_total_code:
            lmt_page_builder_state.total = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            /*tex Otherwise we have to also set that at the \TEX\ end: */
            lmt_page_builder_state.last_height = lmt_page_builder_state.total;
            /*tex So when setting total and depth first total needs to be set! */
            lmt_page_builder_state.last_depth = 0;
            break;
        case page_depth_code:
            lmt_page_builder_state.depth = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            /*tex Otherwise we have to also set that at the \TEX\ end: */
            lmt_page_builder_state.last_depth = lmt_page_builder_state.depth;
            break;
        case page_excess_code:
            lmt_page_builder_state.excess = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_last_height_code:
            lmt_page_builder_state.last_height = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_last_depth_code:
            lmt_page_builder_state.last_depth = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case dead_cycles_code:
            lmt_page_builder_state.dead_cycles = tex_scan_integer(1, NULL, NULL);
            break;
        case insert_penalties_code:
            lmt_page_builder_state.insert_penalties = tex_scan_integer(1, NULL, NULL);
            break;
        case insert_heights_code:
            lmt_page_builder_state.insert_heights = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case insert_storing_code:
            lmt_insert_state.storing = tex_scan_integer(1, NULL, NULL);
            break;
        case insert_distance_code:
            {
                /*tex
                    We need to scan the index first because when we do that in the call we somehow
                    get an out-of-order issue (index too large). The same is true for teh rest.
                */
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_distance(index, tex_scan_glue(glue_val_level, 1, 1));
            }
            break;
        case insert_multiplier_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_multiplier(index, tex_scan_integer(1, NULL, NULL));
            }
            break;
        case insert_limit_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_limit(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_storage_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_storage(index, tex_scan_integer(1, NULL, NULL));
            }
            break;
        case insert_penalty_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_penalty(index, tex_scan_integer(1, NULL, NULL));
            }
            break;
        case insert_maxdepth_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_maxdepth(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_height_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_height(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_depth_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_depth(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_width_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_width(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_line_height_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_line_height(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_line_depth_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_line_depth(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_direction_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_direction(index, tex_scan_direction(0));
            }
            break;
        case insert_stretch_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_stretch(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case insert_shrink_code:
            {
                int index = tex_scan_integer(0, NULL, NULL);
                tex_set_insert_shrink(index, tex_scan_dimension(0, 0, 0, 1, NULL, NULL));
            }
            break;
        case page_stretch_code:                        
            lmt_page_builder_state.stretch = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_fistretch_code:                    
            lmt_page_builder_state.fistretch = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_filstretch_code:                    
            lmt_page_builder_state.filstretch = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_fillstretch_code:                   
            lmt_page_builder_state.fillstretch = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_filllstretch_code:                  
            lmt_page_builder_state.filllstretch = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        case page_shrink_code:                        
            lmt_page_builder_state.shrink = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            break;
        default:
            tex_confusion("page property");
            break;
    }
}

/*tex
    The |space_factor| or |prev_depth| settings are changed when a |set_aux| command is sensed.
    Similarly, |prev_graf| is changed in the presence of |set_prev_graf|, and |dead_cycles| or
    |insert_penalties| in the presence of |set_page_int|. These definitions are always global.
*/

static void tex_aux_set_auxiliary(int a)
{
    (void) a;
    switch (cur_chr) {
        case space_factor_code:
            if (cur_mode == hmode) {
                cur_list.space_factor = tex_scan_space_factor(1);
            } else {
                tex_aux_run_illegal_case();
            }
            break;
        case prev_depth_code:
            if (cur_mode == vmode) {
                cur_list.prev_depth = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
            } else {
                tex_aux_run_illegal_case();
            }
            break;
        case prev_graf_code:
            lmt_nest_state.nest[tex_vmode_nest_index()].prev_graf = tex_scan_positive_number(1);
            break;
        case interaction_mode_code:
            tex_aux_set_interaction(tex_scan_integer(1, NULL, NULL));
            break;
        case insert_mode_code:
            tex_set_insert_mode(tex_scan_integer(1, NULL, NULL));
            break;
    }
}

/*tex
    When some dimension of a box register is changed, the change isn't exactly global; but \TEX\
    does not look at the |\global| switch.
*/

static void tex_aux_set_box_property(void)
{
    halfword code = cur_chr;
    halfword n = tex_scan_box_register_number();
    halfword b = box_register(n);
    switch (code) {
        case box_width_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_width(b) = v;
                }
                break;
            }
        case box_height_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_height(b) = v;
                }
                break;
            }
        case box_depth_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_depth(b) = v;
                }
                break;
            }
        case box_direction_code:
            {
                halfword v = tex_scan_direction(1);
                if (b) {
                    tex_set_box_direction(b, v);
                }
                break;
            }
        case box_geometry_code:
            {
                halfword v = tex_scan_geometry(1);
                if (b) {
                    box_geometry(b) = (singleword) v;
                }
                break;
            }
        case box_orientation_code:
            {
                halfword v = tex_scan_orientation(1);
                if (b) {
                    box_orientation(b) = v;
                    tex_set_box_geometry(b, orientation_geometry);
                }
                break;
            }
        case box_anchor_code:
        case box_anchors_code:
            {
                halfword v = code == box_anchor_code ? tex_scan_anchor(1) : tex_scan_anchors(1);
                if (b) {
                    box_anchor(b) = v;
                    tex_set_box_geometry(b, anchor_geometry);
                }
                break;
            }
        case box_source_code:
            {
                halfword v = tex_scan_integer(1, NULL, NULL);
                if (b) {
                    box_source_anchor(b) = v;
                    tex_set_box_geometry(b, anchor_geometry);
                }
                break;
            }
        case box_target_code:
            {
                halfword v = tex_scan_integer(1, NULL, NULL);
                if (b) {
                    box_target_anchor(b) = v;
                    tex_set_box_geometry(b, anchor_geometry);
                }
                break;
            }
        case box_xoffset_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_x_offset(b) = v;
                    tex_set_box_geometry(b, offset_geometry);
                }
                break;
            }
        case box_yoffset_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_y_offset(b) = v;
                    tex_set_box_geometry(b, offset_geometry);
                }
                break;
            }
        case box_xmove_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_x_offset(b) = tex_aux_checked_dimension1(box_x_offset(b) + v);
                    box_width(b) = tex_aux_checked_dimension2(box_width(b) + v);
                    tex_set_box_geometry(b, offset_geometry);
                }
                break;
            }
        case box_ymove_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_y_offset(b) = tex_aux_checked_dimension1(box_y_offset(b) + v);
                    box_height(b) = tex_aux_checked_dimension2(box_height(b) + v);
                    box_depth(b) = tex_aux_checked_dimension2(box_depth(b) - v);
                    tex_set_box_geometry(b, offset_geometry);
                }
                break;
            }
        case box_total_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_height(b) = v / 2;
                    box_depth(b) = v - (v / 2);
                }
            }
            break;
        case box_shift_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    box_shift_amount(b) = v;
                }
            }
            break;
        case box_adapt_code:
            {
                scaled v = tex_scan_limited_scale(1);
                if (b) {
                    tex_repack(b, v, packing_adapted);
                }
            }
            break;
        case box_repack_code:
            {
                scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                if (b) {
                    tex_repack(b, v, packing_additional);
                }
            }
            break;
        case box_freeze_code:
            {
                halfword recurse = tex_scan_integer(1, NULL, NULL);
                if (b) {
                    tex_freeze(b, recurse, -1, 0);
                }
            }
            break;
        case box_migrate_code:
            {
                halfword what = tex_scan_integer(1, NULL, NULL);
                if (b) {
                    /* maybe check for post */
                    halfword first = null; 
                    halfword last = null; 
                    tex_migrate(b, &first, &last, what & auto_migrate_insert, what & auto_migrate_mark);
                    if (first) { 
                        box_post_migrated(b) = first;
                    }
                }
            }
            break;
        case box_limitate_code:
            {
                halfword recurse = tex_scan_integer(1, NULL, NULL);
                if (b) {
                    tex_freeze(b, recurse, node_type(b), 0);
                }
            }
            break;
        case box_finalize_code:
            {
                halfword factor = tex_scan_integer(0, NULL, NULL);
                if (b) {
                    tex_freeze(b, 0, -1, factor); /* recurse makes no sense here */
                }
            }
            break;
        case box_limit_code:
            {
                if (b && box_list(b)) {
                    tex_limit(b);
                }
            }
            break;
        case box_stretch_code:
        case box_shrink_code:
            /* ignore: maybe apply some factor */
            break;
        case box_subtype_code:
            /* ignore: maybe set it (limited subset) */
            break;
        case box_attribute_code:
            {
                halfword att = tex_scan_attribute_register_number();
                halfword val = tex_scan_integer(1, NULL, NULL);
                if (b) {
                    if (val == unused_attribute_value) {
                        tex_unset_attribute(b, att, val);
                    } else {
                        tex_set_attribute(b, att, val);
                    }
                }
            }
            break;
        case box_vadjust_code: 
            if (b) { 
                tex_set_vadjust(b);
            } else { 
                tex_run_vadjust(); /* maybe error */
            }
            break;
        case box_inserts_code:
            break;
        default:
            break;
    }
}

/*tex
    The processing of boxes is somewhat different, because we may need to scan and create an entire
    box before we actually change the value of the old one.
*/

static void tex_aux_set_box(int a)
{
    halfword slot = tex_scan_box_register_number();
    if (lmt_error_state.set_box_allowed) {
        if (tex_define_permitted(register_box_location(slot), a)) {
            tex_aux_scan_box(is_global(a) ? global_box_flag : box_flag, 1, null_flag, slot, 0, 0);
            if (is_immutable(a)) { 
                eq_flag(register_box_location(slot)) |= immutable_flag_bit;
            }
        }
    } else {
        tex_handle_error(
            normal_error_type,
            "Improper \\setbox",
            "Sorry, \\setbox is not allowed after \\halign in a display, between \\accent and\n"
            "an accented character, or in immediate assignments."
        );
    }
}

/*tex
    We temporarily define |p| to be |relax|, so that an occurrence of |p| while scanning the
    definition will simply stop the scanning instead of producing an \quote {undefined control
    sequence} error or expanding the previous meaning. This allows, for instance, |\chardef
    \foo = 123\foo|.
*/

static void tex_aux_set_shorthand_def(int a, int force)
{
    halfword code = cur_chr;
    /*
    switch (code) { 
        case integer_def_csname_code:
        case dimension_def_csname_code:
            cur_cs = tex_create_csname();
            break;
        default:           
            tex_get_r_token();
            break;
    }
    */
    tex_get_r_token();
    if (force || tex_define_permitted(cur_cs, a)) {
        /* can we optimize the dual define, like no need to destroy in second call */
        halfword p = cur_cs;
        tex_define(a, p, relax_cmd, relax_code);
        tex_scan_optional_equals();
        switch (code) {
            case char_def_code:
                {
                    halfword chr = tex_scan_char_number(0);
                    tex_define_again(a, p, char_given_cmd, chr);
                    break;
                }
            case math_char_def_code:
                {
                    mathcodeval mval = tex_scan_mathchar(tex_mathcode);
                    tex_define_again(a, p, mathspec_cmd, tex_new_math_spec(mval, tex_mathcode));
                    break;
                }
            case math_uchar_def_code:
                {
                    mathcodeval mval = tex_scan_mathchar(umath_mathcode);
                    tex_define_again(a, p, mathspec_cmd, tex_new_math_spec(mval, umath_mathcode));
                    break;
                }
            case math_dchar_def_code:
                {
                    mathdictval dval = tex_scan_mathdict();
                    mathcodeval mval = tex_scan_mathchar(umath_mathcode);
                    tex_define_again(a, p, mathspec_cmd, tex_new_math_dict_spec(dval, mval, umath_mathcode));
                    break;
                }
            case count_def_code:
                {
                    halfword n = tex_scan_integer_register_number();
                    tex_define_again(a, p, register_integer_cmd, register_integer_location(n));
                    break;
                }
            case attribute_def_code:
                {
                    halfword n = tex_scan_attribute_register_number();
                    tex_define_again(a, p, register_attribute_cmd, register_attribute_location(n));
                    break;
                }
            case dimen_def_code:
                {
                    scaled n = tex_scan_dimension_register_number();
                    tex_define_again(a, p, register_dimension_cmd, register_dimension_location(n));
                    break;
                }
            case skip_def_code:
                {
                    halfword n = tex_scan_glue_register_number();
                    tex_define_again(a, p, register_glue_cmd, register_glue_location(n));
                    break;
                }
            case muskip_def_code:
                {
                    halfword n = tex_scan_muglue_register_number();
                    tex_define_again(a, p, register_muglue_cmd, register_muglue_location(n));
                    break;
                }
            case toks_def_code:
                {
                    halfword n = tex_scan_toks_register_number();
                    tex_define_again(a, p, register_toks_cmd, register_toks_location(n));
                    break;
                }
            case float_def_code:
                {
                    scaled n = tex_scan_posit_register_number();
                    tex_define_again(a, p, register_posit_cmd, register_posit_location(n));
                    break;
                }
            case lua_def_code:
                {
                    halfword v = tex_scan_function_reference(1);
                    tex_define_again(a, p, is_protected(a) ? lua_protected_call_cmd : (is_semiprotected(a) ? lua_semi_protected_call_cmd : lua_call_cmd), v);
                    break;
                }
            case integer_def_code:
         /* case integer_def_csname_code: */
                {
                    halfword v = tex_scan_integer(1, NULL, NULL);
                    tex_define_again(a, p, integer_cmd, v);
                    break;
                }
            case dimension_def_code:
         /* case dimension_def_csname_code: */
                {
                    scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                    tex_define_again(a, p, dimension_cmd, v);
                    break;
                }
            case gluespec_def_code:
                {
                    halfword v = tex_scan_glue(glue_val_level, 1, 1);
                    tex_define_again(a, p, gluespec_cmd, v);
                    break;
                }
            case mugluespec_def_code:
                {
                    halfword v = tex_scan_glue(muglue_val_level, 1, 0);
                    tex_define_again(a, p, mugluespec_cmd, v);
                    break;
                }
            case posit_def_code:
         /* case posit_def_csname_code: */
                {
                    scaled v = tex_scan_posit(1);
                    tex_define_again(a, p, posit_cmd, v);
                    break;
                }
            case parameter_def_code:
         /* case index_def_csname_code: */
                {
                    halfword v = tex_get_parameter_index(tex_scan_parameter_index());
                    tex_define_again(a, p, index_cmd, v);
                    break;
                }
            /*
            case mathspec_def_code:
                {
                    halfword v = tex_scan_math_spec(1);
                    tex_define(a, p, mathspec_cmd, v);
                    break;
                }
            */
            case fontspec_def_code:
                {
                    halfword v = tex_scan_font(1);
                    tex_define(a, p, fontspec_cmd, v);
                    break;
                }
            case specification_def_code:
                {
                    halfword v = tex_scan_specifier();
                    tex_define(a, p, specificationspec_cmd, v);
                    break;
                }
            default:
                tex_confusion("shorthand definition");
                break;
        }
    }
}

static void tex_aux_set_association(int flags, int force)
{
    switch (cur_chr) { 
        case unit_association_code: 
            { 
                tex_get_r_token();
                if (tex_valid_userunit(cur_cmd, cur_chr, cur_cs)) {
                    halfword cs = cur_cs;
                    halfword index = tex_scan_unit_register_number(1);
                    if (tex_get_unit_class(index)) { 
                        tex_handle_error(
                            normal_error_type,
                            "Imvalid \\associateunit, unit %i is already taken", index, 
                            "Units can only be bound once and not overload built-in ones."
                        );
                    } else if (force || tex_define_permitted(cs, flags)) {
                        unit_parameter(index) = cs;
                    }
                } else { 
                    tex_handle_error(
                        normal_error_type,
                        "Invalid \\associateunit target",
                        "Only existing dimension equivalent commands are accepted."
                    );
                }
                break;
            }
    }
}

/*tex
    All of \TEX's parameters are kept in |eqtb| except the font and language information, including
    the hyphenation tables; these are strictly global.
*/

static void tex_aux_set_hyph_data(void)
{
    switch (cur_chr) {
        case hyphenation_code:
            { 
                halfword result = tex_scan_toks_expand(0, NULL, 0, 0);
                tex_load_tex_hyphenation(language_par, result);
                tex_flush_token_list(result);
                break;
            }
        case patterns_code:
            { 
                halfword result = tex_scan_toks_expand(0, NULL, 0, 0);
                tex_load_tex_patterns(language_par, result);
                tex_flush_token_list(result);
                break;
            }
        case prehyphenchar_code:
            tex_set_pre_hyphen_char(language_par, tex_scan_integer(1, NULL, NULL));
            break;
        case posthyphenchar_code:
            tex_set_post_hyphen_char(language_par, tex_scan_integer(1, NULL, NULL));
            break;
        case preexhyphenchar_code:
            tex_set_pre_exhyphen_char(language_par, tex_scan_integer(1, NULL, NULL));
            break;
        case postexhyphenchar_code:
            tex_set_post_exhyphen_char(language_par, tex_scan_integer(1, NULL, NULL));
            break;
        case hyphenationmin_code:
            tex_set_hyphenation_min(language_par, tex_scan_integer(1, NULL, NULL));
            break;
        case hjcode_code:
            {
                halfword lan = tex_scan_integer(0, NULL, NULL);
                halfword val = tex_scan_integer(1, NULL, NULL);
                tex_set_hj_code(language_par, lan, val, -1);
            }
            break;
        default:
            break;
    }
}

/*tex move to font */

static void tex_aux_set_font_property(void)
{
    halfword code = cur_chr;
    switch (code) {
        case font_hyphen_code:
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword val = tex_scan_integer(1, NULL, NULL);
                set_font_hyphen_char(fnt, val);
                break;
            }
        case font_skew_code:
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword val = tex_scan_integer(1, NULL, NULL);
                set_font_skew_char(fnt, val);
                break;
            }
        case font_lp_code:
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                tex_set_lpcode_in_font(fnt, chr, val);
                break;
            }
        case font_rp_code:
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                tex_set_rpcode_in_font(fnt, chr, val);
                break;
            }
        case font_ef_code:
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_integer(1, NULL, NULL);
                tex_set_efcode_in_font(fnt, chr, val);
                break;
            }
        case font_cf_code:
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_integer(1, NULL, NULL);
                tex_set_cfcode_in_font(fnt, chr, val);
                break;
            }
        case font_dimension_code:
            {
                tex_set_font_dimension();
                break;
            }
        case scaled_font_dimension_code:
        case scaled_font_slant_per_point_code:
        case scaled_font_interword_space_code:
        case scaled_font_interword_stretch_code:
        case scaled_font_interword_shrink_code:
        case scaled_font_ex_height_code:
        case scaled_font_em_width_code:
        case scaled_font_extra_space_code:
            {
                tex_set_scaled_font_dimension(code - scaled_font_dimension_code);
                break;
            }
        default:
            break;
    }
}

/*tex
    Here is where the information for a new font gets loaded. We start with fonts. Unfortunately,
    they aren't all as simple as this.
*/

static void tex_aux_set_font(int a)
{
    tex_set_cur_font(a, cur_chr);
}

static void tex_aux_set_define_font(int a)
{
    if (! tex_tex_def_font(a)) {
        tex_aux_show_frozen_error(cur_cs);
    }
}

/*tex
    When a |def| command has been scanned, |cur_chr| is odd if the definition is supposed to be
    global, and |cur_chr >= 2| if the definition is supposed to be expanded. Remark: this is
    different in \LUAMETATEX.
*/

static void tex_aux_set_def(int flags, int force)
{
    int expand = 0;
    switch (cur_chr) {
        case expanded_def_code:
            expand = 1;
            break;
        case def_code:
            break;
        case global_expanded_def_code:
            expand = 1;
            // fall through
        case global_def_code:
            flags = add_global_flag(flags);
            break;
        case expanded_def_csname_code:
            expand = 1;
            // fall through
        case def_csname_code:
            cur_cs = tex_create_csname();
            goto DONE;
        case global_expanded_def_csname_code:
            expand = 1;
            // fall through
        case global_def_csname_code:
            cur_cs = tex_create_csname();
            flags = add_global_flag(flags);
            goto DONE;
        case constant_def_code:
            expand = 2;
            flags = add_constant_flag(flags);
            break;
        case constant_def_csname_code:
            expand = 2;
            cur_cs = tex_create_csname();
            flags = add_constant_flag(flags);
            goto DONE;
    }
    tex_get_r_token();
    DONE:
# if 1
    if (global_defs_par) {
        flags = global_defs_par > 0 ? add_global_flag(flags) : remove_global_flag(flags);
    }
# else
    /*tex Tracing discussed @ tex-implementors but differently. It makes not that sense anyway. */
    if (global_defs_par) {
        if (global_defs_par < 0) {
            if (is_global(flags)) {
                remove_global_flag(flags);
                if (tracing_commands_par) {
                    begin_diagnostic();
                    tprint_nl("{\global canceled}");
                    end_diagnostic();
                }
            }
        } else {
            if (! is_global(flags)) {
                add_global_flag(flags);
                if (tracing_commands_par) {
                    begin_diagnostic();
                    tprint_nl("{\global enforced}");
                    end_diagnostic();
                }
            }
        }
    }
# endif 
    if (force || tex_define_permitted(cur_cs, flags)) {
        halfword p = cur_cs;
        halfword t = expand == 2 ? tex_scan_toks_expand(0, null, 1, 0) : (expand ? tex_scan_macro_expand() : tex_scan_macro_normal());
        tex_define(flags, p, tex_flags_to_cmd(flags), t);
    }
}

static void tex_aux_set_let(int flags, int force)
{
    halfword code = cur_chr;
    halfword cs = null;
    switch (code) {
        case global_let_code:
            /*tex |\glet| */
            if (global_defs_par >= 0) {
                flags = add_global_flag(flags);
            }
            // fall through
        case let_code:
            /*tex |\let| */
            tex_get_r_token();
          LETINDEED:
            if (force || tex_define_permitted(cur_cs, flags)) {
                cs = cur_cs;
                do {
                    tex_get_token();
                } while (cur_cmd == spacer_cmd);
                if (cur_tok == equal_token) {
                    tex_get_token();
                    if (cur_cmd == spacer_cmd) {
                        tex_get_token();
                    }
                }
            }
            break;
        case future_let_code:
        case future_def_code:
            /*tex |\futurelet| */
            tex_get_r_token();
            /*tex
                Checking for a frozen macro here is tricky but not doing it would be kind of weird.
            */
             if (force || tex_define_permitted(cur_cs, flags)) {
                 halfword q;
                 cs = cur_cs;
                 q = tex_get_token();
                 tex_back_input(tex_get_token());
                 /*tex
                     We look ahead and then back up. Note that |back_input| doesn't affect |cur_cmd|,
                     |cur_chr|.
                 */
                 tex_back_input(q);
                 if (code == future_def_code) {
                     halfword result = get_reference_token();
                     halfword r = result;
                     r = tex_store_new_token(r, cur_tok);
                     cur_cmd = tex_flags_to_cmd(flags);
                     cur_chr = result;
                 }
            }
            break;
        case let_charcode_code:
            /*tex |\letcharcode| (todo: protection) */
            {
                halfword character = tex_scan_integer(0, NULL, NULL);
                if (character > 0) {
                    cs = tex_active_to_cs(character, 1);
                    do {
                        tex_get_token();
                    } while (cur_cmd == spacer_cmd);
                    if (cur_tok == equal_token) {
                        tex_get_token();
                        if (cur_cmd == spacer_cmd) {
                            tex_get_token();
                        }
                    }
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "invalid number for \\letcharcode",
                        NULL
                    );
                }
                break;
            }
        case swap_cs_values_code:
            {
                /*tex
                    There is no real gain in performance but it looks nicer when tracing when we
                    just swap natively (like no save and restore of a temporary variable and
                    such). Maybe we should be more restrictive but it's a cheap experiment anyway.

                    Flags should match and should not contain permanent, primitive or immutable.
                */
                halfword s1, s2;
                tex_get_r_token();
                s1 = cur_cs;
                tex_get_r_token();
                s2 = cur_cs;
                tex_define_swapped(flags, s1, s2, force);
                return;
            }
        case let_protected_code:
            tex_get_r_token();
            if (force || tex_define_permitted(cur_cs, flags)) {
                switch (cur_cmd) {
                    case call_cmd:
                    case semi_protected_call_cmd:
                        set_eq_type(cur_cs, protected_call_cmd);
                        break;
                    case tolerant_call_cmd:
                    case tolerant_semi_protected_call_cmd:
                        set_eq_type(cur_cs, tolerant_protected_call_cmd);
                        break;
                }
            }
            return;
        case unlet_protected_code:
            tex_get_r_token();
            if (force || tex_define_permitted(cur_cs, flags)) {
                switch (cur_cmd) {
                    case protected_call_cmd:
                    case semi_protected_call_cmd:
                        set_eq_type(cur_cs, call_cmd);
                        break;
                    case tolerant_call_cmd:
                    case tolerant_semi_protected_call_cmd:
                        set_eq_type(cur_cs, tolerant_call_cmd);
                        break;
                }
            }
            return;
        case let_frozen_code:
            tex_get_r_token();
            if (is_call_cmd(cur_cmd) && (force || tex_define_permitted(cur_cs, flags))) {
                set_eq_flag(cur_cs, add_frozen_flag(eq_flag(cur_cs)));
            }
            return;
        case unlet_frozen_code:
            tex_get_r_token();
            if (is_call_cmd(cur_cmd) && (force || tex_define_permitted(cur_cs, flags))) {
                set_eq_flag(cur_cs, remove_frozen_flag(eq_flag(cur_cs)));
            }
            return;
        case global_let_csname_code:
            if (global_defs_par >= 0) {
                flags = add_global_flag(flags);
            }
            // fall through
        case let_csname_code:
            cur_cs = tex_create_csname();
            goto LETINDEED;
        case global_let_to_nothing_code:
            if (global_defs_par >= 0) {
                flags = add_global_flag(flags);
            }
            // fall through
        case let_to_nothing_code:
            tex_get_r_token();
         LETTONOTHING:
            if (force || tex_define_permitted(cur_cs, flags)) {
             // /*tex 
             //     The commented line permits plenty empty definitions, a |\let| can run out of 
             //     ref count so maybe some day \unknown 
             // */
             // halfword empty = get_reference_token();
             // tex_add_token_reference(empty);
                halfword empty = lmt_token_state.empty;
                tex_define(flags, cur_cs, tex_flags_to_cmd(flags), empty);
            }
            return;
        case let_to_last_named_cs_code:
            /*tex 
                There is no real reason for this primitive but it might be more intuitive to see 
                |\lettolastcsname \foo| than |\edef \foo {\lastnamedcs}. The gain in performance is 
                irrelevant here, it's more about readability and the amount of extra code can be 
                neglected. 
            */
            if (lmt_scanner_state.last_cs_name == null_cs) {
                tex_get_r_token();
                goto LETTONOTHING;
            } else {
                /*tex 
                    Do we need to bump the ref count already? We anyway need to save the current
                    value. Let's assume sane usage which is somewhat hard to imagine with primitives
                    like |\lastnamedcs|. 
                */
                halfword lastcs = lmt_scanner_state.last_cs_name;
                tex_get_r_token();
                cs = cur_cs;
                cur_cs = lastcs;
                cur_cmd = eq_type(lastcs); 
                cur_chr = eq_value(lastcs);
                break;
            }
        default:
            /*tex We please the compiler. */
            tex_confusion("let");
            break;
    }
    if (is_referenced_cmd(cur_cmd)) {
        tex_add_token_reference(cur_chr);
    } else if (is_nodebased_cmd(cur_cmd)) {
        cur_chr = cur_chr ? tex_copy_node(cur_chr) : null;
    }
 // if (cs && cur_cmd >= relax_cmd) {
    if (cs && cur_cmd >= 0) {
        singleword oldf = eq_flag(cur_cs);
        singleword newf = 0;
        singleword cmd = (singleword) cur_cmd;
        if (is_aliased(flags)) {
            /*tex 
                Aliases only work for non constants: else make a |\def| of it or we need some 
                pointer to the original but as the meaning can change. Too tricky. 
            */
            newf = oldf;
         // if (is_untraced(flags)) {
         //  // newf = add_untraced_flag(newf);
         //     newf |= untraced_flag_bit;
         // } 
        } else {
            oldf = remove_overload_flags(oldf);
            newf = oldf | make_eq_flag_bits(flags);
        }
        if (is_protected(flags)) {
            switch (cmd) {
                case call_cmd:
                    cmd = protected_call_cmd;
                    break;
                case tolerant_call_cmd:
                    cmd = tolerant_protected_call_cmd;
                    break;
            }
        }
        tex_define_inherit(flags, cs, (singleword) newf, (singleword) cmd, cur_chr);
    } else {
        tex_define(flags, cs, (singleword) cur_cmd, cur_chr); 
    }
}

/*tex
    The token-list parameters, |\output| and |\everypar|, etc., receive their values in the
    following way. (For safety's sake, we place an enclosing pair of braces around an |\output|
    list.)
*/

static void tex_aux_set_assign_toks(int a, halfword cmd, halfword cs, halfword loc) // better just pass cmd and chr
{
    halfword chr;
    halfword tail;
    /*tex
        Skip an optional equal sign and get the next non-blank non-relax non-call token.
    */
    int n = 1 ;
    while (1) {
        tex_get_x_token();
        if (cur_cmd == spacer_cmd) {
            /*tex Go on! */
        } else if (cur_cmd == relax_cmd) {
            n = 0;
        } else if (n && cur_tok == equal_token) {
            n = 0;
        } else {
            break;
        }
    }
    if (cur_cmd != left_brace_cmd) {
        /*tex
            If the right-hand side is a token parameter or token register, finish
            the assignment and |goto done|
        */
        if (cur_cmd == register_cmd && cur_chr == token_val_level) {
            chr = eq_value(register_toks_location(tex_scan_toks_register_number()));
            if (chr) {
                tex_add_token_reference(chr);
            }
            goto DEFINE;
        } else if (cur_cmd == register_toks_cmd || cur_cmd == internal_toks_cmd) {
            chr = eq_value(cur_chr);
            if (chr) {
                tex_add_token_reference(chr);
            }
            goto DEFINE;
        } else {
            /*tex Recover possibly with error message. */
            tex_back_input(cur_tok);
            cur_cs = cs;
            chr = tex_scan_toks_normal(0, &tail);
        }
    } else {
        cur_cs = cs;
        chr = tex_scan_toks_normal(1, &tail);
    }
    if (! token_link(chr)) {
        tex_put_available_token(chr);
        chr = null;
    } else if (loc == internal_toks_location(output_routine_code)) {
        halfword head = token_link(chr);
        halfword list = tex_store_new_token(null, left_brace_token + '{');
        tex_store_new_token(tail, right_brace_token + '}');
        set_token_link(list, head);
        set_token_link(chr, list);
    }
  DEFINE:
    tex_define(a, loc, cmd == internal_toks_cmd ? internal_toks_reference_cmd : register_toks_reference_cmd, chr);
}

static void tex_aux_set_register_toks(int a, int force) // better just pass cmd and chr
{
    halfword cs = cur_cs;
    halfword index = tex_scan_toks_register_number();
    halfword p = register_toks_location(index);
    if (force || tex_register_permitted(p, index, register_toks_cmd)) {
        tex_aux_set_assign_toks(a, register_toks_cmd, cs, p);
    }
}

static void tex_aux_set_internal_toks(int a, int force) // better just pass cmd and chr
{
    if (force || tex_mutation_permitted(cur_chr)) {
        tex_aux_set_assign_toks(a, cur_cmd, cur_cs, cur_chr);
    }
}

/*tex Let |n| be the largest legal code value, based on |cur_chr| */

static void tex_aux_set_define_char_code(int a) /* maybe make |a| already a boolean */
{
    switch (cur_chr) {
        case catcode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_category_code(1);
                tex_set_cat_code(cat_code_table_par, chr, val, global_or_local(a));
            }
            break;
        case lccode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_char_number(1);
                tex_set_lc_code(chr, val, global_or_local(a));
            }
            break;
        case uccode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_char_number(1);
                tex_set_uc_code(chr, val, global_or_local(a));
            }
            break;
        case sfcode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_space_factor(1);
                tex_set_sf_code(chr, val, global_or_local(a));
            }
            break;
        case hccode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_char_number(1);
                tex_set_hc_code(chr, val, global_or_local(a));
            }
            break;
        case hmcode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_math_discretionary_number(1);
                tex_set_hm_code(chr, val, global_or_local(a));
            }
            break;
        case amcode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_category_code(1);
                tex_set_am_code(chr, val, global_or_local(a));
            }
            break;
        case cccode_charcode:
            {
                halfword chr = tex_scan_char_number(0);
                halfword val = tex_scan_classification_code(1);
                tex_set_cc_code(chr, val, global_or_local(a));
            }
            break;
        case mathcode_charcode:
            tex_scan_extdef_math_code(global_or_local(a), tex_mathcode);
            break;
        case extmathcode_charcode:
            tex_scan_extdef_math_code(global_or_local(a), umath_mathcode);
            break;
        case delcode_charcode:
            tex_scan_extdef_del_code(global_or_local(a), tex_mathcode);
            break;
        case extdelcode_charcode:
            tex_scan_extdef_del_code(global_or_local(a), umath_mathcode);
            break;
        default:
            break;
    }
}

static void tex_aux_skip_optional_equal(void)
{
    do {
        tex_get_x_token();
    } while (cur_cmd == spacer_cmd);
    if (cur_tok == equal_token) {
        tex_get_x_token();
    }
}

static void tex_aux_set_math_parameter(int a)
{
    halfword code = cur_chr;
    halfword value = null; /* can also be scaled */
    switch (code) {
        case math_parameter_reset_spacing:
            {
                tex_reset_all_styles(global_or_local(a));
                return;
            }
        case math_parameter_set_spacing:
        case math_parameter_set_atom_rule:
            {
                halfword left = tex_scan_math_class_number(0);
                halfword right = tex_scan_math_class_number(0);
                switch (code) {
                    case math_parameter_set_spacing:
                        code = tex_to_math_spacing_parameter(left, right);
                        break;
                    case math_parameter_set_atom_rule:
                        code = tex_to_math_rules_parameter(left, right);
                        break;
                }
                if (code < 0) {
                    tex_handle_error(
                        normal_error_type,
                        "Invalid math class pair",
                        "I'm going to assume ordinary atoms."
                    );
                    switch (code) {
                        case math_parameter_set_spacing:
                            code = tex_to_math_spacing_parameter(ordinary_noad_subtype, ordinary_noad_subtype);
                            break;
                        case math_parameter_set_atom_rule:
                            code = tex_to_math_rules_parameter(ordinary_noad_subtype, ordinary_noad_subtype);
                            break;
                    }
                }
                break;
            }
        case math_parameter_let_spacing:
        case math_parameter_let_atom_rule:
            {
                halfword mathclass = tex_scan_math_class_number(0);
                halfword display = tex_scan_math_class_number(1);
                halfword text = tex_scan_math_class_number(0);
                halfword script = tex_scan_math_class_number(0);
                halfword scriptscript = tex_scan_math_class_number(0);
                if (valid_math_class_code(mathclass)) {
                    switch (code) {
                        case math_parameter_let_spacing:
                            code = internal_integer_location(first_math_class_code + mathclass);
                            break;
                        case math_parameter_let_atom_rule:
                            code = internal_integer_location(first_math_atom_code + mathclass);
                            break;
                    }
                    value = (display << 24) + (text << 16) + (script << 8) + scriptscript;
                 // tex_assign_internal_integer_value(a, code, value);
                    tex_word_define(a, code, value);
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "Invalid math class",
                        "I'm going to ignore this alias."
                    );
                }
                return;
            }
        case math_parameter_copy_spacing:
        case math_parameter_copy_atom_rule:
        case math_parameter_copy_parent:
            {
                halfword mathclass = tex_scan_math_class_number(0);
                halfword parent = tex_scan_math_class_number(1);
                if (valid_math_class_code(mathclass) && valid_math_class_code(parent)) {
                    switch (code) {
                        case math_parameter_copy_spacing:
                            code = internal_integer_location(first_math_class_code + mathclass);
                            value = count_parameter(first_math_class_code + parent);
                            break;
                        case math_parameter_copy_atom_rule:
                            code = internal_integer_location(first_math_atom_code + mathclass);
                            value = count_parameter(first_math_atom_code + parent);
                            break;
                        case math_parameter_copy_parent:
                            code = internal_integer_location(first_math_parent_code + mathclass);
                            value = count_parameter(first_math_parent_code + parent);
                            break;
                    }
                    tex_word_define(a, code, value);
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "Invalid math class",
                        "I'm going to ignore this alias."
                    );
                }
                return;
            }
        case math_parameter_set_pre_penalty:
        case math_parameter_set_post_penalty:
        case math_parameter_set_display_pre_penalty:
        case math_parameter_set_display_post_penalty:
            {
                halfword mathclass = tex_scan_math_class_number(0);
                halfword penalty = tex_scan_integer(1, NULL, NULL);
                if (valid_math_class_code(mathclass)) {
                    switch (code) {
                        case math_parameter_set_pre_penalty:
                            code = internal_integer_location(first_math_pre_penalty_code + mathclass);
                            break;
                        case math_parameter_set_post_penalty:
                            code = internal_integer_location(first_math_post_penalty_code + mathclass);
                            break;
                        case math_parameter_set_display_pre_penalty:
                            code = internal_integer_location(first_math_display_pre_penalty_code + mathclass);
                            break;
                        case math_parameter_set_display_post_penalty:
                            code = internal_integer_location(first_math_display_post_penalty_code + mathclass);
                            break;
                    }
                    tex_word_define(a, code, penalty);
                 // tex_assign_internal_integer_value(a, code, penalty);
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "Invalid math class",
                        "I'm going to ignore this atom penalty."
                    );
                }
                return;
            }
        case math_parameter_let_parent:
            {
                halfword mathclass = tex_scan_math_class_number(0);
                halfword pre = tex_scan_math_class_number(1);
                halfword post = tex_scan_math_class_number(0);
                halfword options = tex_scan_math_class_number(0);
                halfword reserved = tex_scan_math_class_number(0);
                if (valid_math_class_code(mathclass)) {
                    code = internal_integer_location(first_math_parent_code + mathclass);
                    value = (reserved << 24) + (options << 16) + (pre << 8) + post;
                    tex_word_define(a, code, value);
                 // tex_assign_internal_integer_value(a, code, value);
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "Invalid math class",
                        "I'm going to ignore this penalty alias."
                    );
                }
                return;
            }
        case math_parameter_ignore:
            {
                halfword param = tex_scan_math_parameter();
                if (param >= 0) {
                    code = internal_integer_location(first_math_ignore_code + param);
                    value = tex_scan_integer(1, NULL, NULL);
                    tex_word_define(a, code, value);
                }
                return;
            }
        case math_parameter_options:
            {
                halfword mathclass = tex_scan_math_class_number(0);
                if (valid_math_class_code(mathclass)) {
                    code = internal_integer_location(first_math_options_code + mathclass);
                    value = tex_scan_integer(1, NULL, NULL);
                    tex_word_define(a, code, value);
                 // tex_assign_internal_integer_value(a, code, value);
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "Invalid math class",
                        "I'm going to ignore these options."
                    );
                }
                return;
            }
        case math_parameter_set_defaults:
            tex_set_default_math_codes();
            return;
    }
    {
        halfword style = tex_scan_math_style_identifier(0, 1);
        halfword indirect = indirect_math_regular;
        int freeze = is_frozen(a) && cur_mode == mmode;
        if (! freeze && is_inherited(a)) {
            tex_aux_skip_optional_equal();
            /*tex
                Maybe also let inherit from another math parameter but that can become circular.
            */
            switch (math_parameter_value_type(code)) {
                case math_integer_parameter:
                    switch (cur_cmd) {
                        case integer_cmd:
                            value = cur_cs;
                            indirect = indirect_math_integer;
                            break;
                        case register_integer_cmd:
                            value = cur_chr;
                            indirect = indirect_math_register_integer;
                            break;
                    }
                    break;
                case math_dimension_parameter:
                    switch (cur_cmd) {
                        case dimension_cmd:
                            value = cur_cs;
                            indirect = indirect_math_dimension;
                            break;
                        case register_dimension_cmd:
                            value = cur_chr;
                            indirect = indirect_math_register_dimension;
                            break;
                    }
                    break;
                case math_muglue_parameter:
                    switch (cur_cmd) {
                        case mugluespec_cmd:
                            value = cur_cs;
                            indirect = indirect_math_mugluespec;
                            break;
                        case register_muglue_cmd:
                            value = cur_chr;
                            indirect = indirect_math_register_mugluespec;
                            break;
                        case internal_muglue_cmd:
                            value = cur_chr;
                            indirect = indirect_math_internal_mugluespec;
                            break;
                        case dimension_cmd:
                            value = cur_cs;
                            indirect = indirect_math_dimension;
                            break;
                        case register_dimension_cmd:
                            value = cur_chr;
                            indirect = indirect_math_register_dimension;
                            break;
                        case gluespec_cmd:
                            value = cur_cs;
                            indirect = indirect_math_gluespec;
                            break;
                        case register_glue_cmd:
                            value = cur_chr;
                            indirect = indirect_math_register_gluespec;
                            break;
                        case internal_glue_cmd:
                            value = cur_chr;
                            indirect = indirect_math_internal_gluespec;
                            break;
                    }
                    break;
                case math_pair_parameter:
                    {
                        halfword left = tex_scan_math_class_number(0);
                        halfword right = tex_scan_math_class_number(0);
                        value = (left << 16) + right;
                    }
                    break;
            }
            if (indirect == indirect_math_regular) {
                tex_handle_error(
                    normal_error_type,
                    "Invalid inherited math parameter type",
                    "The inheritance type should match the math parameter type"
                );
                return;
            }
        } else {
            switch (math_parameter_value_type(code)) {
                case math_integer_parameter:
                    value = tex_scan_integer(1, NULL, NULL);
                    break;
                case math_dimension_parameter:
                    value = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                    break;
                case math_muglue_parameter:
                    value = tex_scan_glue(muglue_val_level, 1, 0);
                    break;
                case math_style_parameter:
                    value = tex_scan_integer(1, NULL, NULL);
                    if (value < 0 || value > last_math_style_variant) {
                        /* maybe a warning */
                        value = math_normal_style_variant;
                    }
                    break;
                case math_pair_parameter:
                    {
                        halfword left = tex_scan_math_class_number(0);
                        halfword right = tex_scan_math_class_number(0);
                        value = (left << 16) + right;
                    }
                    break;
                default:
                    tex_confusion("math parameter type");
                    return;
            }
        }
        if (freeze) {
            halfword n = tex_new_node(parameter_node, (quarterword) style);
            parameter_name(n) = code;
            parameter_value(n) = value;
            attach_current_attribute_list(n);
            tex_tail_append(n);
     // } else {
        } {
            switch (style) {
                case all_display_styles:
                    tex_set_display_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_text_styles:
                    tex_set_text_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_script_styles:
                    tex_set_script_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_script_script_styles:
                    tex_set_script_script_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_math_styles:
                    tex_set_all_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_main_styles:
                    tex_set_main_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_split_styles:
                    tex_set_split_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_unsplit_styles:
                    tex_set_unsplit_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_uncramped_styles:
                    tex_set_uncramped_styles(code, value, global_or_local(a), indirect);
                    break;
                case all_cramped_styles:
                    tex_set_cramped_styles(code, value, global_or_local(a), indirect);
                    break;
                default:
                    tex_def_math_parameter(style, code, value, global_or_local(a), indirect, 0);
                    break;
            }

        }
    }
}

/* */

static void tex_aux_set_define_family(int a)
{
    halfword p = cur_chr;
    halfword fnt;
    halfword fam = tex_scan_math_family_number();
    tex_scan_optional_equals();
    fnt = tex_scan_font_identifier(NULL);
    tex_def_fam_fnt(fam, p, fnt, global_or_local(a));
}

/*tex Similar routines are used to assign values to the numeric parameters. */

static void tex_aux_set_internal_integer(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_integer(1, NULL, NULL);
        tex_assign_internal_integer_value(a, p, v);
    }
}

static void tex_aux_set_register_integer(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_integer(1, NULL, NULL);
        tex_word_define(a, p, v);
    }
}

static void tex_aux_set_internal_posit(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        scaled v = tex_scan_posit(1);
        tex_assign_internal_posit_value(a, p, v);
    }
}

static void tex_aux_set_register_posit(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        scaled v = tex_scan_posit(1);
        tex_word_define(a, p, v);
    }
}

static void tex_aux_set_internal_attribute(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_integer(1, NULL, NULL);
        if (internal_attribute_number(p) > lmt_node_memory_state.max_used_attribute) {
            lmt_node_memory_state.max_used_attribute = internal_attribute_number(p);
        }
        tex_change_attribute_register(a, p, v);
        tex_word_define(a, p, v);
    }
}

static void tex_aux_set_register_attribute(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_integer(1, NULL, NULL);
        if (register_attribute_number(p) > lmt_node_memory_state.max_used_attribute) {
            lmt_node_memory_state.max_used_attribute = register_attribute_number(p);
        }
        tex_change_attribute_register(a, p, v);
        tex_word_define(a, p, v);
    }
}

static void tex_aux_set_internal_dimension(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
        tex_assign_internal_dimension_value(a, p, v);
    }
}

static void tex_aux_set_register_dimension(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        scaled v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
        tex_word_define(a, p, v);
    }
}

static void tex_aux_set_internal_glue(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_glue(glue_val_level, 1, 0);
     // define(a, p, internal_glue_ref_cmd, v);
        tex_assign_internal_skip_value(a, p, v);
    }
}

static void tex_aux_set_register_glue(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_glue(glue_val_level, 1, 1);
        tex_define(a, p, register_glue_reference_cmd, v);
    }
}

static void tex_aux_set_internal_muglue(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_glue(muglue_val_level, 1, 0);
        tex_define(a, p, internal_muglue_reference_cmd, v);
    }
}

static void tex_aux_set_register_muglue(int a, int force)
{
    halfword p = cur_chr;
    if (force || tex_mutation_permitted(p)) {
        halfword v = tex_scan_glue(muglue_val_level, 1, 0);
        tex_define(a, p, register_muglue_reference_cmd, v);
    }
}

/*tex
    We ignore prefixes that don't apply as we might apply then in the future: just like |\immediate|
    so it's not that alien. And maybe frozen can be applied some day in other cases as well. As
    reference we keep the old code (long and outer code has been removed elsewhere.) Most of the
    calls are the only call so the functions are likely to be inlined.

*/

static void tex_aux_set_combine_toks(halfword a, int force)
{
    if (is_global(a)) {
        switch (cur_chr) {
            case expanded_toks_code:         cur_chr = global_expanded_toks_code; break;
            case append_toks_code:           cur_chr = global_append_toks_code; break;
            case append_expanded_toks_code:  cur_chr = global_append_expanded_toks_code; break;
            case prepend_toks_code:          cur_chr = global_prepend_toks_code; break;
            case prepend_expanded_toks_code: cur_chr = global_prepend_expanded_toks_code; break;
        }
    }
    tex_run_combine_the_toks(force);
}

static int tex_aux_set_some_item(void)
{
    switch (cur_chr) {
        case lastpenalty_code:  
            lmt_page_builder_state.last_penalty = tex_scan_integer(1, NULL, NULL);
            return 1;
        case lastkern_code:
            lmt_page_builder_state.last_kern = tex_scan_integer(1, NULL, NULL);
            return 1;
        case lastskip_code:
            if (lmt_page_builder_state.last_glue != max_halfword) {
                tex_flush_node(lmt_page_builder_state.last_glue);
                lmt_page_builder_state.last_glue = tex_scan_glue(glue_val_level, 1, 0);
            }
            return 1;
        case lastboundary_code:
            lmt_page_builder_state.last_boundary = tex_scan_integer(1, NULL, NULL);
            return 1;
        case last_node_type_code:
            lmt_page_builder_state.last_node_type = tex_scan_integer(1, NULL, NULL);
            return 1;
        case last_node_subtype_code:
            lmt_page_builder_state.last_node_subtype = tex_scan_integer(1, NULL, NULL);
            return 1;
        case last_left_class_code:
            lmt_math_state.last_left = tex_scan_math_class_number(1);
            return 1;
        case last_right_class_code:
            lmt_math_state.last_right = tex_scan_math_class_number(1);
            return 1;
        case last_atom_class_code:
            lmt_math_state.last_atom = tex_scan_math_class_number(1);
            return 1;
        default: 
            return 0;
    }
}

static void tex_aux_set_constant_register(halfword cmd, halfword cs, halfword flags, int force)
{
    if (force || tex_mutation_permitted(cs)) {
        halfword v = null;
        switch(cmd) {
            case integer_cmd:
                v = tex_scan_integer(1, NULL, NULL);
                break;
            case dimension_cmd:
                v = tex_scan_dimension(0, 0, 0, 1, NULL, NULL);
                break;
            case posit_cmd:
                v = tex_scan_posit(1);
                break;
            case gluespec_cmd:
                v = tex_scan_glue(glue_val_level, 1, 1);
                break;
            case mugluespec_cmd:
                v = tex_scan_glue(muglue_val_level, 1, 0);
                break;
        }
        tex_define(flags, cs, (singleword) cmd, v);
    }
}

static void tex_run_prefixed_command(void)
{
    /*tex accumulated prefix codes so far */
    int flags = 0;
    int force = 0;
    halfword lastprefix = -1;
    while (cur_cmd == prefix_cmd) {
        switch (cur_chr) {
            case frozen_code       : flags = add_frozen_flag       (flags); break;
            case permanent_code    : flags = add_permanent_flag    (flags); break;
            case immutable_code    : flags = add_immutable_flag    (flags); break;
            case mutable_code      : flags = add_mutable_flag      (flags); break;
            case noaligned_code    : flags = add_noaligned_flag    (flags); break;
            case instance_code     : flags = add_instance_flag     (flags); break;
            case untraced_code     : flags = add_untraced_flag     (flags); break;
            case global_code       : flags = add_global_flag       (flags); break;
            case tolerant_code     : flags = add_tolerant_flag     (flags); break;
            case protected_code    : flags = add_protected_flag    (flags); break;
            case overloaded_code   : flags = add_overloaded_flag   (flags); break;
            case aliased_code      : flags = add_aliased_flag      (flags); break;
            case immediate_code    : flags = add_immediate_flag    (flags); break;
         // case deferred_code     : break;
            case semiprotected_code: flags = add_semiprotected_flag(flags); break;            
            case always_code       : flags = add_aliased_flag      (flags); force = 1; break; /*tex This one is bound. */
            case inherited_code    : flags = add_inherited_flag    (flags); break;
            case constant_code     : flags = add_constant_flag     (flags); break;
            case retained_code     : flags = add_retained_flag     (flags); break;
            case constrained_code  : flags = add_constrained_flag  (flags); break;
         // case long_code         : break;  
         // case outer_code        : break;
            default:
                goto PICKUP;
        }
        lastprefix = cur_chr;
      PICKUP:
        /*tex We no longer report prefixes. */
        do {
            tex_get_x_token();
        } while (cur_cmd == spacer_cmd || cur_cmd == relax_cmd);
        if (tracing_commands_par > 2) {
            tex_show_cmd_chr(cur_cmd, cur_chr);
        }
    }

    /*tex: Here we can quit when we have a constant! */

    /*tex
        Adjust for the setting of |\globaldefs|. A negative value removed global, also for the 
        |g*| and |x*| primitives. 
    */
    if (global_defs_par) {
        flags = global_defs_par > 0 ? add_global_flag(flags) : remove_global_flag(flags);
    }
    /*tex
        Now we arrived at all the def variants. We only apply the prefixes that make sense (for
        now).
    */
    switch (cur_cmd) {
        case some_item_cmd: 
            if (! tex_aux_set_some_item()) {
                tex_aux_run_illegal_case();
            } 
            break;
        case internal_toks_cmd:
        case register_toks_cmd:
            tex_aux_set_internal_toks(flags, force);
            break;
        case internal_integer_cmd:
            tex_aux_set_internal_integer(flags, force);
            break;
        case register_integer_cmd:
            tex_aux_set_register_integer(flags, force);
            break;
        case internal_attribute_cmd:
            tex_aux_set_internal_attribute(flags, force);
            break;
        case register_attribute_cmd:
            tex_aux_set_register_attribute(flags, force);
            break;
        case internal_posit_cmd:
            tex_aux_set_internal_posit(flags, force);
            break;
        case register_posit_cmd:
            tex_aux_set_register_posit(flags, force);
            break;
        case internal_dimension_cmd:
            tex_aux_set_internal_dimension(flags, force);
            break;
        case register_dimension_cmd:
            tex_aux_set_register_dimension(flags, force);
            break;
        case internal_glue_cmd:
            tex_aux_set_internal_glue(flags, force);
            break;
        case register_glue_cmd:
            tex_aux_set_register_glue(flags, force);
            break;
        case internal_muglue_cmd:
            tex_aux_set_internal_muglue(flags, force);
            break;
        case register_muglue_cmd:
            tex_aux_set_register_muglue(flags, force);
            break;
        case lua_value_cmd:
            tex_aux_set_lua_value(flags);
            break;
        case font_property_cmd:
            tex_aux_set_font_property();
            break;
        case auxiliary_cmd:
            tex_aux_set_auxiliary(flags);
            break;
        case hyphenation_cmd:
            tex_aux_set_hyph_data();
            break;
        case page_property_cmd:
            tex_aux_set_page_property();
            break;
        case box_property_cmd:
            tex_aux_set_box_property();
            break;
        case specification_cmd:
            tex_aux_set_specification(flags, cur_chr);
            break;
        case define_char_code_cmd:
            tex_aux_set_define_char_code(flags);
            break;
        case define_family_cmd:
            tex_aux_set_define_family(flags);
            break;
        case math_parameter_cmd:
            tex_aux_set_math_parameter(flags);
            break;
        case set_font_cmd:
            tex_aux_set_font(flags);
            break;
        case define_font_cmd:
            tex_aux_set_define_font(flags);
            break;
        case integer_cmd:
        case posit_cmd:
        case dimension_cmd:
        case gluespec_cmd:
        case mugluespec_cmd:
            tex_aux_set_constant_register(cur_cmd, cur_cs, flags, force);
            break;
        case index_cmd: 
            /*tex  
                This one is special because in this usage scenario it is not set but does 
                something instead. 
            */
            tex_inject_parameter(cur_chr); 
            break;
        case association_cmd:
            tex_aux_set_association(flags, force);
            break;
        case interaction_cmd:
            tex_aux_set_interaction(cur_chr);
            break;
        case register_cmd:
            if (cur_chr == token_val_level) {
                tex_aux_set_register_toks(flags, force);
            } else {
                tex_aux_set_register(flags);
            }
            break;
        case combine_toks_cmd:
            tex_aux_set_combine_toks(flags, force);
            break;
        case arithmic_cmd:
            tex_aux_arithmic_register(flags, cur_chr);
            break;
        case let_cmd:
            tex_aux_set_let(flags, force);
            break;
        case shorthand_def_cmd:
            tex_aux_set_shorthand_def(flags, force);
            break;
        case def_cmd:
            tex_aux_set_def(flags, force);
            break;
        case set_box_cmd:
            tex_aux_set_box(flags);
            break;
        default:
            if (lastprefix < 0) {
                tex_confusion("prefixed command");
            } else {
                tex_handle_error(
                    normal_error_type,
                    "You can't use a prefix %C with %C",
                    prefix_cmd, lastprefix, cur_cmd, cur_chr,
                    "A prefix should be followed by a quantity that can be assigned to. Intermediate\n"
                    "spaces and \\relax tokens are gobbled in the process.\n"
                );
                break;
            }
    }
    /*tex
        End of assignments cases. We insert a token saved by |\afterassignment|, if any.
    */
    tex_aux_finish_after_assignment();
}

/*tex

    When a control sequence is to be defined, by |\def| or |\let| or something similar, the
    |get_r_token| routine will substitute a special control sequence for a token that is not
    redefinable.

*/

void tex_get_r_token(void)
{
  RESTART:
    do {
        tex_get_token();
    } while (cur_tok == space_token);
    if (eqtb_invalid_cs(cur_cs)) {
        if (cur_cmd == active_char_cmd) {
            cur_cs = tex_active_to_cs(cur_chr, 1);
            cur_cmd = eq_type(cur_cs);
            cur_chr = eq_value(cur_cs);
         // tex_x_token();
         //  if (! eqtb_invalid_cs(cur_cs)) {
            return;
         //  }
        }
        if (cur_cs == 0) {
            tex_back_input(cur_tok);
        }
        cur_tok = deep_frozen_protection_token;
        /*tex 
            Moved down but this might interfere with input on the console which we don't use 
            anyway. 
        */
        tex_handle_error(
            insert_error_type,
            "Missing control sequence inserted",
            "Please don't say '\\def cs{...}', say '\\def\\cs{...}'. I've inserted an\n"
            "inaccessible control sequence so that your definition will be completed without\n"
            "mixing me up too badly.\n"
        );
        goto RESTART;
 // } else if (cur_cmd == cs_name_cmd && cur_chr == cs_name_code) { 
 //     /*tex 
 //         This permits 
 //             |\someassignment\csname foo\endcsname| 
 //         over 
 //             |\expandafter\someassignment\csname foo\endcsname| 
 //         but it actually doesn't happen that frequently so even if we measure a gain of some 
 //         10\percent on some cases (with millions of iterations) for now I will not enable this
 //         feature. Of course it is also incompatible but I don't expect anyone to redefine csname
 //         so it is actually a pretty safe extension. Of course it also reduces tracing. I will 
 //         come back to it when I suspect a performance gain in \CONTEXT\ (or want my code to look 
 //         better).
 //     */
 //     cur_cs = tex_create_csname();    
    }
}

/*tex
    Some of the internal int values need a special treatment. This used to be a more complex
    function, also dealing with other registers than didn't really need a check, also because we
    now split into internals and registers.

    Beware: the post binary and relation penalties are not synchronzed here because we assume a
    proper overload of the primitive. They can still be set and their setting is reflected in the
    atom panalties but that's all. No need for more code.
*/

void tex_assign_internal_integer_value(int a, halfword p, int val)
{
    switch (internal_integer_number(p)) {
        case par_direction_code:
        case math_direction_code:
            {
                check_direction_value(val);
                tex_word_define(a, p, val);
            }
            break;
        case text_direction_code:
            {
                check_direction_value(val);
                tex_inject_text_or_line_dir(val, 0);
                tex_word_define(a, p, val);
                /*tex Plus: */
                update_tex_internal_dir_state(internal_dir_state_par + 1);
            }
            break;
        case line_direction_code:
            {
                check_direction_value(val);
                tex_inject_text_or_line_dir(val, 1);
                p = internal_integer_location(text_direction_code);
                tex_word_define(a, p, val);
                /*tex Plus: */
                update_tex_internal_dir_state(internal_dir_state_par + 1);
            }
            break;
        case cat_code_table_code:
            if (tex_valid_catcode_table(val)) {
                if (val != cat_code_table_par) {
                    tex_word_define(a, p, val);
                }
            } else {
                tex_handle_error(
                    normal_error_type,
                    "Invalid \\catcode table",
                    "You can only switch to a \\catcode table that is initialized using\n"
                    "\\savecatcodetable or \\initcatcodetable, or to table 0"
                );
            }
            break;
        case glyph_scale_code:
        case glyph_x_scale_code:
        case glyph_y_scale_code:
            /* todo: check for reasonable */
            if (val) {
                tex_word_define(a, p, val);
            } else {
                /* maybe an error message */
            }
            break;
     // case glyph_slant_code: 
     // case glyph_weight_code: 
     //     /* maybe test for maxima */
     //     break;
        case glyph_text_scale_code:
        case glyph_script_scale_code:
        case glyph_scriptscript_scale_code:
            /* here zero is a signal */
            if (val < min_math_style_scale || val > max_math_style_scale) {
                tex_handle_error(
                    normal_error_type,
                    "Invalid \\glyph..scale",
                    "The value for \\glyph..scale has to be between " LMT_TOSTRING(min_math_style_scale) " and " LMT_TOSTRING(max_math_style_scale) " where\n"
                    "a value of zero forces font percentage scaling to be used."
                );
                val = max_limited_scale;
            }
            tex_word_define(a, p, val);
            break;
        case math_begin_class_code:
        case math_end_class_code:
        case math_left_class_code:
        case math_right_class_code:
            if (! valid_math_class_code(val)) {
                val = unset_noad_class;
            }
            tex_word_define(a, p, val);
            break;
        case output_box_code:
            if (val < 0 || val > max_box_index) {
                tex_handle_error(
                    normal_error_type,
                    "Invalid \\outputbox",
                    "The value for \\outputbox has to be between 0 and " LMT_TOSTRING(max_box_index) "."
                );
            } else {
                tex_word_define(a, p, val);
            }
            break;
        case new_line_char_code:
            if (val > max_newline_character) {
                tex_handle_error(
                    normal_error_type,
                    "Invalid \\newlinechar",
                    "The value for \\newlinechar has to be no higher than " LMT_TOSTRING(max_newline_character) ".\n"
                    "Your invalid assignment will be ignored."
                );
            } else {
                tex_word_define(a, p, val);
            }
            break;
        case end_line_char_code:
            if (val > max_endline_character) {
               tex_handle_error(
                   normal_error_type,
                   "Invalid \\endlinechar",
                   "The value for \\endlinechar has to be no higher than " LMT_TOSTRING(max_endline_character) "."
               );
            } else {
                tex_word_define(a, p, val);
            }
            break;
        case language_code:
            /* this is |\language| */
            if (val < 0) {
                val = 0;
            }
            if (tex_is_valid_language(val)) {
                update_tex_language(a, val);
            } else {
                tex_handle_error(
                    normal_error_type,
                    "Invalid \\language",
                    "The value for \\language has to be defined and in the range 0 .. " LMT_TOSTRING(max_n_of_languages) "."
                );
            }
            break;
        case font_code:
            if (val < 0) {
                val = 0;
            }
            if (tex_is_valid_font(val)) {
                tex_set_cur_font(a, val);
            } else {
                tex_handle_error(
                    normal_error_type,
                    "Invalid \\fontid",
                    "The value for \\fontid has to be defined and in the range 0 .. " LMT_TOSTRING(max_n_of_fonts) "."
                );
            }
            break;
        case hyphenation_mode_code:
            if (val < 0) {
                val = 0;
            }
            /* We don't update |\uchyph| here. */
            tex_word_define(a, p, val);
            break;
        case uc_hyph_code:
            /*tex For old times sake. */
            tex_word_define(a, p, val);
            /*tex But we do use this instead. */
            val = val ? set_hyphenation_mode(hyphenation_mode_par, uppercase_hyphenation_mode) : unset_hyphenation_mode(hyphenation_mode_par, uppercase_hyphenation_mode);
            tex_word_define(a, internal_integer_location(hyphenation_mode_code), val);
            break;
        case local_interline_penalty_code:
        case local_broken_penalty_code:
        case local_tolerance_code:
        case local_pre_tolerance_code:
            if (cur_mode == hmode) {
                /*tex 
                    We can check if the last node is a parameter par node and just patch that 
                    one but we have a callback on a new one so for now we just add redundant 
                    nodes. It's a bit inefficient when we have local boxes, so if I really start 
                    using this feature I might go for efficiency. 
                */
                tex_word_define(a, p, val);
                tex_tail_append(tex_new_par_node(parameter_par_subtype));
                update_tex_internal_par_state(internal_par_state_par + 1);
            } else { 
                /*tex
                    Here is an old comment: If we are defining subparagraph penalty levels while 
                    we are in hmode, then we put out a whatsit (here a par node) immediately, 
                    otherwise we leave it alone. This mechanism might not be sufficiently powerful,
                    and some other algorithm, searching down the stack, might be necessary. This 
                    is a good first step. Well, in \LUAMETATEX\ we no longer do this and silently 
                    ignore this setting. 
                */
            }
            break;
        case adjust_spacing_code:
            if (val < adjust_spacing_off) {
                val = adjust_spacing_off;
            }
            else if (val > adjust_spacing_font) {
                val = adjust_spacing_font;
            }
            goto DEFINE; /* par property */
        case protrude_chars_code:
            if (val < protrude_chars_off) {
                val = protrude_chars_off;
            }
            else if (val > protrude_chars_advanced) {
                val = protrude_chars_advanced;
            }
            goto DEFINE; /* par property */
        case glyph_options_code:
            val &= glyph_option_valid;
            tex_word_define(a, p, val);
            break;
        case discretionary_options_code:
            val &= disc_option_valid;
            tex_word_define(a, p, val);
            break;
        case overload_mode_code:
         // if (overload_mode_par != 255) {
                tex_word_define(a, p, val);
         // }
            break;
        /* We only synchronize these four one way. */
        case post_binary_penalty_code:
            tex_word_define(a, internal_integer_location(first_math_post_penalty_code + binary_noad_subtype), val);
            tex_word_define(a, internal_integer_location(first_math_display_post_penalty_code + binary_noad_subtype), val);
            break;
        case post_relation_penalty_code:
            tex_word_define(a, internal_integer_location(first_math_post_penalty_code + relation_noad_subtype), val);
            tex_word_define(a, internal_integer_location(first_math_display_post_penalty_code + relation_noad_subtype), val);
            break;
        case pre_binary_penalty_code:
            tex_word_define(a, internal_integer_location(first_math_pre_penalty_code + binary_noad_subtype), val);
            tex_word_define(a, internal_integer_location(first_math_display_pre_penalty_code + binary_noad_subtype), val);
            break;
        case pre_relation_penalty_code:
            tex_word_define(a, internal_integer_location(first_math_pre_penalty_code + relation_noad_subtype), val);
            tex_word_define(a, internal_integer_location(first_math_display_pre_penalty_code + relation_noad_subtype), val);
            break;
        /* We could do this, but then we also need to do day and check it per month. */ /*
        case month_code:
            if (val < 1) {
                val = 1;
            } else if (val > 12) {
                val = 12;
            }
            goto DEFINE;
        */
        case eu_factor_code:
            if (val < eu_min_factor) {
                val = eu_min_factor;
            } else if (val > eu_max_factor) { 
                val = eu_max_factor;
            }
            tex_word_define(a, p, val);
            break;
        default:
          DEFINE:
            tex_word_define(a, p, val);
            if (is_frozen(a) && cur_mode == hmode) {
                tex_update_par_par(internal_integer_cmd, internal_integer_number(p));
            }
            break;
    }
}

void tex_assign_internal_attribute_value(int a, halfword p, int val)
{
    if (register_attribute_number(p) > lmt_node_memory_state.max_used_attribute) {
        lmt_node_memory_state.max_used_attribute = register_attribute_number(p);
    }
    tex_change_attribute_register(a, p, val);
    tex_word_define(a, p, val);
}

void tex_assign_internal_posit_value(int a, halfword p, int val)
{
    tex_word_define(a, p, val);
 // if (is_frozen(a) && cur_mode == hmode) {
 //     tex_update_par_par(internal_posit_cmd, internal_posit_number(p));
 // }
}

void tex_assign_internal_dimension_value(int a, halfword p, int val)
{
    tex_word_define(a, p, val);
    if (is_frozen(a) && cur_mode == hmode) {
        tex_update_par_par(internal_dimension_cmd, internal_dimension_number(p));
    }
}

void tex_assign_internal_skip_value(int a, halfword p, int val)
{
    switch (internal_glue_number(p)) {
        case additional_page_skip_code:
            tex_define(a & global_flag_bit, p, internal_glue_reference_cmd, val);
            if (cur_mode == vmode) {
                tex_additional_page_skip();
            }
            break;
        default:
            tex_define(a, p, internal_glue_reference_cmd, val);
            break;
    }
    if (is_frozen(a) && cur_mode == hmode) {
        tex_update_par_par(internal_glue_cmd, internal_glue_number(p));
    }
}

/*tex

    Here is a procedure that might be called \quotation {Get the next non-blank non-relax non-call
    non-assignment token}. It is a runner used in text accents and math alignments. It probably
    has to be adapted to the additional command codes that we have.

*/

void tex_handle_assignments(void)
{
    while (1) {
        do {
            tex_get_x_token();
        } while (cur_cmd == spacer_cmd || cur_cmd == relax_cmd);
        if (cur_cmd <= max_non_prefixed_cmd)  {
            return;
        } else {
            lmt_error_state.set_box_allowed = 0;
            tex_run_prefixed_command();
            lmt_error_state.set_box_allowed = 1;
        }
    }
}

/*tex Has the long |\errmessage| help been used? */

static strnumber tex_aux_scan_string(void)
{
    int saved_selector = lmt_print_state.selector; /*tex holds |selector| setting */
    halfword result = tex_scan_toks_expand(0, NULL, 0, 0);
 // saved_selector = lmt_print_state.selector;
    lmt_print_state.selector = new_string_selector_code;
    tex_token_show(result);
    tex_flush_token_list(result);
    lmt_print_state.selector = saved_selector;
    return tex_make_string(); /* todo: we can use take_string instead but happens only @ error */
}

static void tex_aux_run_message(void)
{
    switch (cur_chr) {
        case message_code:
            {
                /*tex Print string |s| on the terminal */
                strnumber s = tex_aux_scan_string();
                if ((lmt_print_state.terminal_offset > 0) || (lmt_print_state.logfile_offset > 0)) {
                    tex_print_char(' ');
                }
                tex_print_tex_str(s);
                tex_terminal_update();
                tex_flush_str(s);
                break;
            }
        case error_message_code:
            {
                /*tex
                    Print string |s| as an error message. If |\errmessage| occurs often in
                    |scroll_mode|, without user-defined |\errhelp|, we don't want to give a long
                    help message each time. So we give a verbose explanation only once. These
                    help messages are not expanded because that could itself generate an error.
                */
                strnumber s = tex_aux_scan_string();
                if (error_help_par) {
                    strnumber helpinfo = tex_tokens_to_string(error_help_par);
                    const char *h = tex_to_cstring(helpinfo);
                    tex_handle_error(
                        normal_error_type,
                        "%T",
                        s,
                        h
                    );
                    tex_flush_str(helpinfo);
                } else if (lmt_error_state.long_help_seen) {
                    tex_handle_error(
                        normal_error_type,
                        "%T",
                        s,
                        "(That was another \\errmessage.)"
                    );
                } else {
                    if (lmt_error_state.interaction < error_stop_mode) {
                        lmt_error_state.long_help_seen = 1;
                    }
                    tex_handle_error(
                        normal_error_type,
                        "%T",
                        s,
                        "This error message was generated by an \\errmessage command, so I can't give any\n"
                        "explicit help. Pretend that you're Hercule Poirot: Examine all clues, and deduce\n"
                        "the truth by order and method."
                    );
                }
                tex_flush_str(s);
                break;
            }
    }
}

/*tex

    The |\uppercase| and |\lowercase| commands are implemented by building a token list and then
    changing the cases of the letters in it.

    Change the case of the token in |p|, if a change is appropriate. When the case of a |chr_code|
    changes, we don't change the |cmd|. We also change active characters. (The last fact permits
    trickery.)

*/

static void tex_aux_run_case_shift(void)
{
    tex_run_case_shift(cur_chr);
}

/*tex

    We come finally to the last pieces missing from |main_control|, namely the |\show| commands that
    are useful when debugging.

*/

static void tex_aux_run_show_whatever(void)
{
    int justshow = 1;
    switch (cur_chr) {
        case show_code:
            /*tex Show the current meaning of a token, then |goto common_ending|. */
            {
                tex_get_token();
                tex_print_nlp();
                tex_print_str("> ");
                if (cur_cs != 0) {
                    tex_print_cs(cur_cs);
                    tex_print_char('=');
                }
                tex_print_meaning(meaning_full_code);
                goto COMMON_ENDING;
            }
        case show_box_code:
            /*tex Show the current contents of a box. */
            {
                int nolevels = 0;
                int diagnose = 0;
                int content = 0;
                int online = 0;
                int max = 0;
                while (1) {
                    switch (tex_scan_character("ocdnaOCDNA", 0, 1, 0)) {
                        case 'a': case 'A':
                            if (tex_scan_mandate_keyword("all", 1)) {
                                max = 1;
                            }
                            break;
                        case 'c': case 'C':
                            if (tex_scan_mandate_keyword("content", 1)) {
                                content = 1;
                            }
                            break;
                        case 'd': case 'D':
                            if (tex_scan_mandate_keyword("diagnose", 1)) {
                                diagnose = 1;
                            }
                            break;
                        case 'n': case 'N':
                            if (tex_scan_mandate_keyword("nolevels", 1)) {
                                nolevels = 1;
                            }
                            break;
                        case 'o': case 'O':
                            if (tex_scan_mandate_keyword("online", 1)) {
                                online = 1;
                            }
                            break;
                        default:
                            goto DONE;
                    }
                }
              DONE:
                /*tex This can become a general helper. */
                {
                    halfword n = tex_scan_box_register_number();
                    halfword r = box_register(n);
                    halfword l = tracing_levels_par;
                    halfword o = tracing_online_par;
                    halfword d = show_box_depth_par;
                    halfword b = show_box_breadth_par;
                    if (nolevels) {
                        tracing_levels_par = 0;
                    }
                    if (online) {
                        tracing_online_par = 2;
                    }
                    if (max) {
                        show_box_depth_par = max_integer;
                        show_box_breadth_par = max_integer;
                    }
                    if (diagnose) {
                        tex_begin_diagnostic();
                    }
                    if (! content) {
                        tex_print_format("> \\box%i=",n);
                    }
                    if (r) {
                        tex_show_box(r);
                    } else {
                        tex_print_str("void");
                    }
                    if (diagnose) {
                        tex_end_diagnostic();
                    }
                    tracing_levels_par = l;
                    tracing_online_par = o;
                    show_box_depth_par = d;
                    show_box_breadth_par = b;
                }
                break;
            }
        case show_the_code:
            {
                halfword head = tex_the_value_toks(the_code, NULL, 0);
                tex_print_nlp();
                tex_print_str("> ");
                tex_show_token_list(head, 0, 0);
                tex_flush_token_list(head);
                goto COMMON_ENDING;
            }
       case show_lists_code:
            {
                tex_begin_diagnostic();
                tex_show_activities();
                tex_end_diagnostic();
                break;
            }
        case show_groups_code:
            {
                tex_begin_diagnostic();
                tex_show_save_groups();
                tex_end_diagnostic();
                break;
            }
        case show_stack_code:
            {
                tex_begin_diagnostic();
                tex_show_save_stack();
                tex_end_diagnostic();
                break;
            }
        case show_code_stack_code:
            {
                tex_begin_diagnostic();
                tex_show_code_stack();
                tex_end_diagnostic();
                break;
            }
        case show_tokens_code:
            {
                halfword head = tex_the_detokenized_toks(NULL, 0, 0);
                tex_print_nlp();
                tex_print_str("> ");
                tex_show_token_list(head, 0, 0);
                tex_flush_token_list(head);
                goto COMMON_ENDING;
            }
        case show_ifs_code:
            {
             // if (! justshow) {
                tex_begin_diagnostic();
             // }
                tex_show_ifs();
             // if (! justshow) {
                tex_end_diagnostic();
             // }
                break;
            }
        default:
            /* can't happen */
            break;
    }
    if (justshow) {
        return;
    } else {
        /*tex By default we |justshow| now so the next is dead code. */
    }
    /*tex Complete a potentially long |\show| command: */
    tex_handle_error_message_only("OK");
    if (lmt_print_state.selector == terminal_and_logfile_selector_code && tracing_online_par <= 0) {
        lmt_print_state.selector = terminal_selector_code;
        tex_print_str(" (see the transcript file)"); /*tex Here |transcript| means |log|.*/
        lmt_print_state.selector = terminal_and_logfile_selector_code;
    }
  COMMON_ENDING:
    if (justshow) {
        return;
    } else if (lmt_error_state.interaction < error_stop_mode) {
        tex_handle_error(
            normal_error_type,
            NULL, /* no message */
            NULL  /* no help */
        );
        --lmt_error_state.error_count;
 /* } else if (tracing_online_par > 0) { */
    } else {
        tex_handle_error(
            normal_error_type,
            NULL, /* no message */
            "This isn't an error message; I'm just \\showing something.\n"
        );
    }
}

/*tex */

static inline halfword tex_aux_expand_escaped(halfword tok, halfword *tail)
{
    halfword value = tex_get_available_token(tok);
    if (tail) { 
        *tail = value;
    }
    return value;
}

/*tex So far |I P G H [N] L R S T X [Z]| are sensitive! Maybe under parameter control? */

halfword tex_expand_parameter(halfword tok, halfword *tail)
{
    halfword value = 0;
    switch (tok) { 
        /* loop counters */
        case I_token_l: case I_token_o: value = lmt_main_control_state.loop_iterator; break; /* iterator */
        case P_token_l: case P_token_o: value = tex_previous_loop_iterator_delta(1);  break; /* parent */
        case G_token_l: case G_token_o: value = tex_previous_loop_iterator_delta(2);  break; /* grandparent */
        /* escape tokens */
        case H_token_l: case H_token_o: return tex_aux_expand_escaped(hash_token_o, tail);
        case L_token_l: case L_token_o: return tex_aux_expand_escaped(newline_token_o, tail);
     // case N_token_l: case N_token_o: return tex_aux_expand_escaped(nbsp_token_o, tail);
        case Q_token_l: case Q_token_o: return tex_aux_expand_escaped(double_quote_token_o, tail);
        case R_token_l: case R_token_o: return tex_aux_expand_escaped(return_token_o, tail);
        case S_token_l: case S_token_o: return tex_aux_expand_escaped(space_token_o, tail);
        case T_token_l: case T_token_o: return tex_aux_expand_escaped(tab_token_o, tail);
        case X_token_l: case X_token_o: return tex_aux_expand_escaped(backslash_token_o, tail);
     // case left_brace_token:          return tex_aux_expand_escaped(left_brace_token_o, tail);
     // case right_brace_token:         return tex_aux_expand_escaped(right_brace_token_o, tail);
     // case Z_token_l: case Z_token_o: return tex_aux_expand_escaped(zws_token_o, tail);
        /* rest */
        default:                        return null;
    }
    /*tex This could be a helper. */
    {
        int saved_selector = lmt_print_state.selector;
        lmt_print_state.selector = new_string_selector_code;
        tex_print_int(value);
        lmt_print_state.selector = saved_selector;
        return tex_cur_str_toks(tail);
    }
}

halfword tex_expand_iterator(halfword tok)
{
    switch (tok) { 
        case I_token_l: case I_token_o: return lmt_main_control_state.loop_iterator;
        case P_token_l: case P_token_o: return tex_previous_loop_iterator_delta(1); 
        case G_token_l: case G_token_o: return tex_previous_loop_iterator_delta(2); 
        default:                        return 0;
    }
}

static void tex_aux_run_parameter(void)
{
    tex_get_token();
    {
        int okay = tex_expand_parameter(cur_tok, NULL);
        if (okay) { 
            tex_begin_inserted_list(okay);
        } else { 
            tex_back_input(cur_tok);
            tex_aux_run_illegal_case(); 
        }
    }
}

static void tex_aux_run_mvl(void)
{
    switch (cur_chr) {
        case begin_mvl_code:
            tex_start_mvl(); // todo: use na range specific scanner 
            break;
        case end_mvl_code:
            if (cur_list.mode == hmode) {
                tex_aux_run_paragraph_end_hmode();
            }
            tex_stop_mvl();
            break;
        default:
            break;
    }
}

/*tex

    These procedures get things started properly. The initializer sets up the function table. We
    have a few aliases to run_functions that are also used otherwise.

    We actually only have some 50 cases where there is a difference between the modes and it makes
    sense now to combine the handling and move the mode checking to those combined functions. That
    way we get a switch no longer a jump. Actually, some already share a function and check for the
    mode. On the other hand, this is how \TEX\ does it.

    When we have version 2.10 released I might move the mode tests to the runners so that we get a
    smaller case cq. jump table and we might also go for mode 1 permanently. A side effect will be
    that some commands codes will be collapsed (move and such). See older source for the two 
    intermediate variants that were tested for a few years. 

*/

static inline void tex_aux_big_switch(int mode, int cmd)
{
    /* todo: order */

    switch (cmd) {

        case arithmic_cmd: 
        case internal_integer_cmd: 
        case register_integer_cmd: 
        case internal_attribute_cmd: 
        case register_attribute_cmd: 
        case internal_posit_cmd: 
        case register_posit_cmd: 
        case internal_dimension_cmd: 
        case register_dimension_cmd: 
        case font_property_cmd : 
        case internal_glue_cmd: 
        case register_glue_cmd: 
        case internal_muglue_cmd: 
        case register_muglue_cmd: 
        case internal_toks_cmd: 
        case register_toks_cmd: 
        case define_char_code_cmd: 
        case def_cmd: 
        case define_family_cmd: 
        case define_font_cmd: 
        case hyphenation_cmd: 
        case let_cmd: 
        case prefix_cmd: 
        case register_cmd: 
        case auxiliary_cmd: 
        case set_box_cmd: 
        case box_property_cmd: 
        case set_font_cmd: 
        case interaction_cmd: 
        case math_parameter_cmd: 
        case page_property_cmd: 
        case specification_cmd: 
        case shorthand_def_cmd: 
        case association_cmd: 
        case lua_value_cmd: 
        case integer_cmd: 
        case posit_cmd: 
        case dimension_cmd: 
        case gluespec_cmd: 
        case mugluespec_cmd: 
        case index_cmd: 
        case combine_toks_cmd:
        case some_item_cmd:               tex_run_prefixed_command();       break;
        case fontspec_cmd:                tex_run_font_spec();              break;
        case specificationspec_cmd:       tex_run_specification_spec();     break;
        case parameter_cmd:               tex_aux_run_parameter();          break;
        case iterator_value_cmd:          tex_aux_run_illegal_case();       break;
        case after_something_cmd:         tex_aux_run_after_something();    break;
        case begin_group_cmd:             tex_aux_run_begin_group();        break;
        case penalty_cmd:                 tex_aux_run_penalty();            break;
        case case_shift_cmd:              tex_aux_run_case_shift();         break;
        case catcode_table_cmd:           tex_aux_run_catcode_table();      break;
        case end_cs_name_cmd:             tex_aux_run_cs_error();           break;
        case end_group_cmd:               tex_aux_run_end_group();          break;
        case end_local_cmd:               tex_aux_run_end_local();          break;
        case ignore_something_cmd:        tex_aux_run_ignore_something();   break;
        case insert_cmd:                  tex_run_insert();                 break;
        case kern_cmd:                    tex_aux_run_kern();               break;
        case leader_cmd:                  tex_aux_run_leader();             break;
        case legacy_cmd:                  tex_aux_run_legacy();             break;
        case local_box_cmd:               tex_aux_run_local_box();          break;
        case lua_protected_call_cmd:      
        case lua_semi_protected_call_cmd: tex_aux_run_lua_protected_call(); break;
        case lua_function_call_cmd:       tex_aux_run_lua_function_call();  break;
        case make_box_cmd:                tex_aux_run_make_box();           break;
        case mark_cmd:                    tex_run_mark();                   break;
        case message_cmd:                 tex_aux_run_message();            break;
        case node_cmd:                    tex_aux_run_node();               break;
        case relax_cmd:                   
        case ignore_cmd:                  tex_aux_run_relax();              break;
        case active_char_cmd:             tex_aux_run_active();             break;
        case remove_item_cmd:             tex_aux_run_remove_item();        break;
        case right_brace_cmd:             tex_aux_run_right_brace();        break;
        case vcenter_cmd:                 tex_run_vcenter();                break;
        case xray_cmd:                    tex_aux_run_show_whatever();      break;
        case alignment_cmd:               
        case alignment_tab_cmd:           tex_run_alignment_error();        break;
        case end_template_cmd:            tex_run_alignment_end_template(); break;
        case mvl_cmd:                     tex_aux_run_mvl();                break;      
        /* */
        case math_fraction_cmd:    mode == mmode ? tex_run_math_fraction()         : tex_aux_run_insert_dollar_sign(); break;
        case delimiter_number_cmd: mode == mmode ? tex_run_math_delimiter_number() : tex_aux_run_insert_dollar_sign(); break;
        case math_fence_cmd:       mode == mmode ? tex_run_math_fence()            : tex_aux_run_insert_dollar_sign(); break;
        case math_modifier_cmd:    mode == mmode ? tex_run_math_modifier()         : tex_aux_run_insert_dollar_sign(); break;
        case math_accent_cmd:      mode == mmode ? tex_run_math_accent()           : tex_aux_run_insert_dollar_sign(); break;
        case math_choice_cmd:      mode == mmode ? tex_run_math_choice()           : tex_aux_run_insert_dollar_sign(); break;
        case math_component_cmd:   mode == mmode ? tex_run_math_math_component()   : tex_aux_run_insert_dollar_sign(); break;
        case math_style_cmd:       mode == mmode ? tex_run_math_style()            : tex_aux_run_insert_dollar_sign(); break;
        case mkern_cmd:            mode == mmode ? tex_aux_run_mkern()             : tex_aux_run_insert_dollar_sign(); break;
        case mskip_cmd:            mode == mmode ? tex_aux_run_mglue()             : tex_aux_run_insert_dollar_sign(); break;
        case math_radical_cmd:     mode == mmode ? tex_run_math_radical()          : tex_aux_run_insert_dollar_sign(); break;
        case subscript_cmd:          
        case superscript_cmd:        
        case math_script_cmd:      mode == mmode ? tex_run_math_script()           : tex_aux_run_insert_dollar_sign(); break;
        case equation_number_cmd:  mode == mmode ? tex_run_math_equation_number()  : tex_aux_run_illegal_case();       break;
        case left_brace_cmd:       mode == mmode ? tex_run_math_left_brace()       : tex_aux_run_left_brace();         break;

        /* */

        case vadjust_cmd:          mode == vmode ? tex_aux_run_illegal_case()  : tex_run_vadjust();           break;
        case discretionary_cmd:    mode == vmode ? tex_aux_run_new_paragraph() : tex_run_discretionary();     break;
        case explicit_space_cmd:   mode == vmode ? tex_aux_run_new_paragraph() : tex_aux_run_space();         break;
        case hmove_cmd:            mode == vmode ? tex_aux_run_move()          : tex_aux_run_illegal_case();  break;
        case vmove_cmd:            mode == vmode ? tex_aux_run_illegal_case()  : tex_aux_run_move();          break;    
        case hskip_cmd:            mode == vmode ? tex_aux_run_new_paragraph() : tex_aux_run_glue();          break;       
        case un_hbox_cmd:          mode == vmode ? tex_aux_run_new_paragraph() : tex_run_unpackage();         break;   

        /* */

        case math_char_number_cmd:
            switch (mode) { 
                case vmode: tex_aux_run_math_non_math();     break;
                case hmode: tex_run_text_math_char_number(); break;
                case mmode: tex_run_math_math_char_number(); break;
            } 
            break;
        case italic_correction_cmd:
            switch (mode) { 
                case vmode: tex_aux_run_illegal_case();           break;
                case hmode: tex_aux_run_text_italic_correction(); break;
                case mmode: tex_run_math_italic_correction();     break;
            } 
            break;
        case mathspec_cmd:
            switch (mode) { 
                case vmode: tex_aux_run_math_non_math(); break;
                case hmode: tex_run_text_math_spec();    break;
                case mmode: tex_run_math_math_spec();    break;
            } 
            break;
        case char_given_cmd:   
        case letter_cmd:    
        case other_char_cmd:   
            switch (mode) { 
                case vmode: tex_aux_run_new_paragraph(); break;
                case hmode: tex_aux_run_text_letter();   break;
                case mmode: tex_run_math_letter();       break;
            } 
            break;

        case accent_cmd:             
            switch (mode) { 
                case vmode: tex_aux_run_new_paragraph(); break;    
                case hmode: tex_aux_run_text_accent();   break;
                case mmode: tex_run_math_accent();       break;
            } 
            break;
        case boundary_cmd:             
            switch (mode) { 
                case vmode: tex_aux_run_par_boundary();  break;    
                case hmode: tex_aux_run_text_boundary(); break;      
                case mmode: tex_aux_run_math_boundary(); break;
            } 
            break;
        case char_number_cmd:    
            switch (mode) { 
                case vmode: tex_aux_run_new_paragraph();    break;     
                case hmode: tex_aux_run_text_char_number(); break;    
                case mmode: tex_run_math_char_number();     break;
            } 
            break;
        case math_shift_cmd: 
        case math_shift_cs_cmd: 
            switch (mode) { 
                case vmode: tex_aux_run_new_paragraph(); break; 
                case hmode: tex_run_math_initialize();   break;   
                case mmode: tex_run_math_shift();        break;
            } 
            break;
        case end_paragraph_cmd:      
            switch (mode) { 
                case vmode: tex_aux_run_paragraph_end_vmode(); break;
                case hmode: tex_aux_run_paragraph_end_hmode(); break;
                case mmode: tex_aux_run_relax();               break;
            } 
            break;
        case spacer_cmd:             
            switch (mode) { 
                case vmode: tex_aux_run_relax();      break;
                case hmode: tex_aux_run_space();      break;    
                case mmode: tex_aux_run_math_space(); break;
            } 
            break;
        case begin_paragraph_cmd:    
            switch (mode) { 
                case vmode: tex_aux_run_begin_paragraph_vmode(); break;
                case hmode: tex_aux_run_begin_paragraph_hmode(); break;
                case mmode: tex_aux_run_begin_paragraph_mmode(); break;
            } 
            break;
        case end_job_cmd:  
            switch (mode) { 
                case vmode: tex_aux_run_end_job();            break;
                case hmode: tex_aux_run_head_for_vmode();     break;
                case mmode: tex_aux_run_insert_dollar_sign(); break;
            } 
            break;

        case vskip_cmd:              
            switch (mode) { 
                case vmode: tex_aux_run_glue();               break;   
                case hmode: tex_aux_run_head_for_vmode();     break;   
                case mmode: tex_aux_run_insert_dollar_sign(); break;
            } 
            break;
        case un_vbox_cmd:            
            switch (mode) { 
                case vmode: tex_run_unpackage();              break;   
                case hmode: tex_aux_run_head_for_vmode();     break;  
                case mmode: tex_aux_run_insert_dollar_sign(); break;
            } 
            break;

        case halign_cmd:            
            switch (mode) { 
                case vmode: tex_run_alignment_initialize(); break;  
                case hmode: tex_aux_run_head_for_vmode();   break;   
                case mmode: tex_aux_run_halign_mmode();     break;
            } 
            break;
        case valign_cmd:       
            switch (mode) { 
                case vmode: tex_aux_run_new_paragraph();      break;    
                case hmode: tex_run_alignment_initialize();   break;   
                case mmode: tex_aux_run_insert_dollar_sign(); break;
            } 
            break;

        case hrule_cmd:      
            switch (mode) { 
                case vmode: tex_aux_run_hrule();              break;     
                case hmode: tex_aux_run_head_for_vmode();     break;    
                case mmode: tex_aux_run_insert_dollar_sign(); break;
                } 
            break;
        case vrule_cmd:  
            switch (mode) { 
                case vmode: tex_aux_run_new_paragraph(); break;
                case hmode: tex_aux_run_vrule();         break;   
                case mmode: tex_aux_run_mrule();         break;
            } 
            break;

        /* */

        default:
            /*tex The next is unlikely to happen but compilers like the check. */
            tex_confusion("unknown cmd code");
            break;
    }

}

/*tex
    Some preset values no longer make sense, like family 1 for some math symbols but we keep them
    for compatibility reasons. All settings are moved to the relevant modules.
*/

void tex_initialize_variables(void)
{
    if (lmt_main_state.run_state == initializing_state) {
     /* mag_par = 1000; */
        tolerance_par = default_tolerance;
        hang_after_par = default_hangafter;
        max_dead_cycles_par = default_deadcycles;
        math_pre_display_gap_factor_par = default_pre_display_gap;
     /* pre_binary_penalty_par = infinite_penalty; */
     /* pre_relation_penalty_par = infinite_penalty; */
        math_font_control_par = assumed_math_control; 
        math_eqno_gap_step_par = default_eqno_gap_step;
        px_dimension_par = one_bp;
        eu_factor_par = eu_def_factor;
        show_node_details_par = 2; /*tex $>1$: |[subtype]| $>2$: |[attributes]| */
        ex_hyphen_char_par = '-';
        escape_char_par = '\\';
        end_line_char_par = '\r';
        space_char_par = ' ';
        output_box_par = default_output_box;
        adjust_spacing_step_par = -1;
        adjust_spacing_stretch_par = -1;
        adjust_spacing_shrink_par = -1;
        math_double_script_mode_par = -1, 
        math_glue_mode_par = default_math_glue_mode; 
        hyphenation_mode_par = default_hyphenation_mode;
        glyph_scale_par = scaling_factor;
        glyph_x_scale_par = scaling_factor;
        glyph_y_scale_par = scaling_factor;
        glyph_x_offset_par = 0;
        glyph_y_offset_par = 0;
        math_begin_class_par = math_begin_class;
        math_end_class_par = math_end_class;
        math_left_class_par = unset_noad_class;
        math_right_class_par = unset_noad_class;
        math_display_penalty_factor_par = scaling_factor;
        math_inline_penalty_factor_par = scaling_factor;
        pre_inline_penalty_par = max_integer;
        post_inline_penalty_par = max_integer;
        pre_short_inline_penalty_par = max_integer;
        post_short_inline_penalty_par = max_integer;
        variable_family_par = -1, 
        ignore_depth_criterion_par = ignore_depth;
        script_space_before_factor_par = scaling_factor;
        script_space_between_factor_par = scaling_factor;
        script_space_after_factor_par = scaling_factor;
        hbadness_mode_par = badness_mode_all;
        vbadness_mode_par = badness_mode_all;
        empty_paragraph_mode_par = effective_empty_option_all;
        aux_get_date_and_time(&time_par, &day_par, &month_par, &year_par, &lmt_engine_state.utc_time);
    }
}
