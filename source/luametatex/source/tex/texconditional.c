/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex

    In \LUAMETATEX\ The condition code has been upgraded. Bits and pieces have been optimized and
    on top of the extra checks in \LUATEX|\ we have a few more here. In order to get nicer looking
    nested conditions |\orelse| has been introduced. Some conditionals are not really needed but
    they give less noise when tracing macros. It's also possible to let \LUA\ code behave like
    a test.

*/

/*tex

    We consider now the way \TEX\ handles various kinds of |\if| commands. Conditions can be inside
    conditions, and this nesting has a stack that is independent of the |save_stack|.

    Four global variables represent the top of the condition stack: |cond_ptr| points to
    pushed-down entries, if any; |if_limit| specifies the largest code of a |fi_or_else| command
    that is syntactically legal; |cur_if| is the name of the current type of conditional; and
    |if_line| is the line number at which it began.

    If no conditions are currently in progress, the condition stack has the special state
    |cond_ptr = null|, |if_limit = normal|, |cur_if = 0|, |if_line = 0|. Otherwise |cond_ptr|
    points to a two-word node; the |type|, |subtype|, and |link| fields of the first word contain
    |if_limit|, |cur_if|, and |cond_ptr| at the next level, and the second word contains the
    corresponding |if_line|.

    In |cond_ptr| we keep track of the top of the condition stack while |if_limit| holds the upper
    bound on |fi_or_else| codes. The type of conditional being worked on is stored in cur_if and
    |if_line| keeps track of the line where that conditional began. When we skip conditional text,
    |skip_line| keeps track of the line number where skipping began, for use in error messages.

    All these variables are collected in:

*/

condition_state_info lmt_condition_state = {
    .cond_ptr      = null,
    .cur_if        = 0,
    .if_limit      = 0,
    .cur_unless    = 0,
    .if_unless     = 0,
    .if_step       = 0,
    .unused        = 0,
    .if_line       = 0,
    .if_nesting    = 0,
    .skip_line     = 0,
    .chk_integer   = 0,
    .chk_dimension = 0,
};

/*tex

    Here is a procedure that ignores text until coming to an |\or|, |\else|, or |\fi| at level zero
    of |\if| \unknown |\fi| nesting. After it has acted, |cur_chr| will indicate the token that was
    found, but |cur_tok| will not be set (because this makes the procedure run faster).

    With |l| we keep track of the level of |\if|\unknown|\fi| nesting and |scanner_status| let us
    return to the entry status. The |pass_text| function only returns when we have a |fi_or_else|.

*/

static void tex_aux_pass_text(void)
{
    int level = 0;
    int status = lmt_input_state.scanner_status;
    lmt_input_state.scanner_status = scanner_is_skipping;
    lmt_condition_state.skip_line = lmt_input_state.input_line;
    while (1) {
        tex_get_next();
        if (cur_cmd == if_test_cmd) {
            switch (cur_chr) {
                case if_code:
                   ++level;
                   break;
                case fi_code:
                    if (level == 0) {
                        lmt_input_state.scanner_status = status;
                        return;
                    } else {
                        --level;
                        break;
                    }
                case else_code:
                case or_code:
                    if (level == 0) {
                        lmt_input_state.scanner_status = status;
                        return;
                    } else {
                        break;
                    }
                case or_else_code:
                case or_unless_code:
                    tex_get_next_non_spacer();
                    /*tex So we skip the token after |\orelse| or |\orunless| without testing it! */
                    break;
                default:
                   ++level;
                   break;
            }
        }
    }
}

/*tex
    We return when we have a |fi_or_else| or when we have a valid |or_else| followed by an
    |if_test_cmd|.
*/

static int tex_aux_pass_text_x(int tracing_ifs, int tracing_commands)
{
    int level = 0;
    int status = lmt_input_state.scanner_status;
    lmt_input_state.scanner_status = scanner_is_skipping;
    lmt_condition_state.skip_line = lmt_input_state.input_line;
    while (1) {
        tex_get_next();
        if (cur_cmd == if_test_cmd) {
            switch (cur_chr) {
                case if_code:
                   ++level;
                   break;
                case fi_code:
                    if (level == 0) {
                        lmt_input_state.scanner_status = status;
                        return 0;
                    } else {
                        --level;
                        break;
                    }
                case else_code:
                case or_code:
                    if (level == 0) {
                        lmt_input_state.scanner_status = status;
                        return 0;
                    } else {
                        break;
                    }
                case or_else_code:
                case or_unless_code:
                    if (level == 0) {
                        int unless = cur_chr == or_unless_code;
                        if (tracing_commands > 1) {
                            tex_begin_diagnostic();
                            tex_print_str(unless ? "{orunless}" : "{orelse}");
                            tex_end_diagnostic();
                        } else if (tracing_ifs) {
                            tex_show_cmd_chr(cur_cmd, cur_chr);
                        }
                        tex_get_next_non_spacer();
                        if (lmt_condition_state.if_limit == if_code) {
                            if (cur_cmd == if_test_cmd && cur_chr >= first_real_if_test_code) {
                                /* okay */
                            } else {
                                tex_handle_error(
                                    normal_error_type,
                                    unless ? "No condition after \\orunless" : "No condition after \\orelse",
                                    "I'd expected a proper if test command."
                                );
                            }
                            lmt_input_state.scanner_status = status;
                            return unless;
                        }
                    } else {
                        --level;
                    }
                    break;
                default:
                   ++level;
                   break;
            }
        }
    }
}

/*tex

    When we begin to process a new |\if|, we set |if_limit = if_code|; then, if |\or| or |\else| or
    |\fi| occurs before the current |\if| condition has been evaluated, |\relax| will be inserted.
    For example, a sequence of commands like |\ifvoid 1 \else ... \fi| would otherwise require
    something after the |1|.

    When a conditional ends that was apparently started in a different input file, the |if_warning|
    procedure is invoked in order to update the |if_stack|. If moreover |\tracingnesting| is
    positive we want to give a warning message (with the same complications as above).

*/

static void tex_aux_if_warning(void)
{
    /*tex Do we need a warning? */
    bool warning = false;
    int index = lmt_input_state.in_stack_data.ptr;
    lmt_input_state.base_ptr = lmt_input_state.input_stack_data.ptr;
    /*tex Store current state. */
    lmt_input_state.input_stack[lmt_input_state.base_ptr] = lmt_input_state.cur_input;
    while (lmt_input_state.in_stack[index].if_ptr == lmt_condition_state.cond_ptr) {
        /*tex Set variable |w| to. */
        if (tracing_nesting_par > 0) {
            while ((lmt_input_state.input_stack[lmt_input_state.base_ptr].state == token_list_state) || (lmt_input_state.input_stack[lmt_input_state.base_ptr].index > index)) {
                --lmt_input_state.base_ptr;
            }
            if (lmt_input_state.input_stack[lmt_input_state.base_ptr].name > 17) {
                warning = true;
            }
        }
        lmt_input_state.in_stack[index].if_ptr = node_next(lmt_condition_state.cond_ptr);
        --index;
    }
    if (warning) {
        tex_begin_diagnostic();
        tex_print_format("[conditional: end of %C%L of a different file]", if_test_cmd, lmt_condition_state.cur_if, lmt_condition_state.if_line);
        tex_end_diagnostic();
        if (tracing_nesting_par > 1) {
            tex_show_context();
        }
        if (lmt_error_state.history == spotless) {
            lmt_error_state.history = warning_issued;
        }
    }
}

/*tex 
    We can consider a dedicated condition stack so that we can copy faster. Or we can just emulate
    an if node in |lmt_condition_state|. 
*/

static void tex_aux_push_condition_stack(int code, int unless)
{
    halfword p = tex_get_node(if_node_size);
    node_type(p) = if_node;
    node_subtype(p) = 0; /* unused */
    node_next(p) = lmt_condition_state.cond_ptr;
    if_limit_type(p) = (quarterword) lmt_condition_state.if_limit;
    if_limit_subtype(p) = (quarterword) lmt_condition_state.cur_if;
    if_limit_step(p) = (singleword) lmt_condition_state.cur_unless;
    if_limit_unless(p) = (singleword) lmt_condition_state.if_unless;
    if_limit_stepunless(p) = (singleword) lmt_condition_state.if_unless;
    if_limit_line(p) = lmt_condition_state.if_line;
    lmt_condition_state.cond_ptr = p;
    lmt_condition_state.cur_if = (quarterword) cur_chr;
    lmt_condition_state.cur_unless = (singleword) unless;
    lmt_condition_state.if_step = (singleword) code;
    lmt_condition_state.if_limit = if_code;
    lmt_condition_state.if_line = lmt_input_state.input_line;
    ++lmt_condition_state.if_nesting;
}

static void tex_aux_pop_condition_stack(void)
{
    halfword p;
    if (lmt_input_state.in_stack[lmt_input_state.in_stack_data.ptr].if_ptr == lmt_condition_state.cond_ptr) {
        /*tex
            Conditionals are possibly not properly nested with files. This test can become an
            option.
        */
        tex_aux_if_warning();
    }
    p = lmt_condition_state.cond_ptr;
    --lmt_condition_state.if_nesting;
    lmt_condition_state.if_line = if_limit_line(p);
    lmt_condition_state.cur_if = if_limit_subtype(p);
    lmt_condition_state.cur_unless = if_limit_unless(p);
    lmt_condition_state.if_step = if_limit_step(p);
    lmt_condition_state.if_unless = if_limit_stepunless(p);
    lmt_condition_state.if_limit = if_limit_type(p);
    lmt_condition_state.cond_ptr = node_next(p);
    tex_free_node(p, if_node_size);
}

/*
    void tex_quit_fi(void) 
    {
        tex_aux_pop_condition_stack();
    }
*/

/*tex
    Here's a procedure that changes the |if_limit| code corresponding to a given value of
    |cond_ptr|.
*/

static inline void tex_aux_change_if_limit(int l, halfword p)
{
    if (p == lmt_condition_state.cond_ptr) {
        lmt_condition_state.if_limit = (quarterword) l;
    } else {
        halfword q = lmt_condition_state.cond_ptr;
        while (q) {
            if (node_next(q) == p) {
                if_limit_type(q) = (quarterword) l;
                return;
            } else {
                q = node_next(q);
            }
        }
        tex_confusion("if");
    }
}

/*tex

    The conditional|\ifcsname| is equivalent to |\expandafter| |\expandafter| |\ifdefined|
    |\csname|, except that no new control sequence will be entered into the hash table (once all
    tokens preceding the mandatory |\endcsname| have been expanded). Because we have \UTF 8, we
    find plenty of small helpers that are used in conversion.

    A csname resolve can itself have nested csname resolving. We keep track of the nesting level
    and also remember the last match.

*/

/* moved to texexpand */

/*tex

    An active character will be treated as category 13 following |\if \noexpand| or following
    |\ifcat \noexpand|.

*/

static void tex_aux_get_x_token_or_active_char(void)
{
    tex_get_x_token();
 // if (cur_cmd == relax_cmd && cur_chr == no_expand_flag && tex_is_active_cs(cs_text(cur_cs))) {
    if (cur_cmd == relax_cmd && cur_chr == no_expand_relax_code && tex_is_active_cs(cs_text(cur_cs))) {
        cur_cmd = active_char_cmd;
        cur_chr = active_cs_value(cs_text(cur_tok - cs_token_flag));
    }
}

/*tex

    A condition is started when the |expand| procedure encounters an |if_test| command; in that
    case |expand| reduces to |conditional|, which is a recursive procedure.

*/

static void tex_aux_missing_equal_error(int code)
{
    tex_handle_error(back_error_type, "Missing = inserted for %C", if_test_cmd, code,
        "I was expecting to see '<', '=', or '>'. Didn't."
    );
}

/*tex

    This is an important function because a bit larger macro package does lots of testing. Compared
    to regular \TEX\ there is of course the penalty of larger data structures but there's not much
    we can do about that. Then there are more variants, which in turn can lead to a performance hit
    as there is more to test and more code involved, which might influence cache hits and such.
    However, I already optimized the \LUATEX\ code a bit and here there are some more tiny potential
    speedups. But \unknown\ they are hard to measure and especially their impact on a normal run:
    \TEX\ is already pretty fast and often these tests themselves are not biggest bottleneck, at
    least not in \CONTEXT. My guess is that the speedups compensate the extra if tests so in the end
    we're still okay. Expansion, pushing back tokens, accessing memory all over the place, excessive
    use of \LUA\ \unknown\ all that has probably way more impact on a run. But I keep an eye on the
    next one anyway.

*/

static void tex_aux_show_if_state(halfword code, halfword case_value)
{
    tex_begin_diagnostic();
    switch (code) {
        case if_chk_int_code       : tex_print_format("{chknum %i}",        case_value); break;
        case if_chk_integer_code   : tex_print_format("{chknumber %i}",     case_value); break;
        case if_val_int_code       : tex_print_format("{numval %i}",        case_value); break;
        case if_cmp_int_code       : tex_print_format("{cmpnum %i}",        case_value); break;
        case if_chk_dim_code       : tex_print_format("{chkdim %i}",        case_value); break;
        case if_chk_dimension_code : tex_print_format("{chkdimension %i}",  case_value); break;
        case if_val_dim_code       : tex_print_format("{dimval %i}",        case_value); break;
        case if_cmp_dim_code       : tex_print_format("{cmpdim %i}",        case_value); break;
        case if_case_code          : tex_print_format("{case %i}",          case_value); break;
        case if_math_parameter_code: tex_print_format("{mathparameter %i}", case_value); break;
        case if_math_style_code    : tex_print_format("{mathstyle %i}",     case_value); break;
        case if_arguments_code     : tex_print_format("{arguments %i}",     case_value); break;
        case if_parameter_code     : tex_print_format("{parameter %i}",     case_value); break;
        case if_parameters_code    : tex_print_format("{parameters %i}",    case_value); break;
        default                    : tex_print_format("{todo %i}",          case_value); break;
    }
    tex_end_diagnostic();
}

/*tex Why do we skip over relax? */

static inline halfword tex_aux_grab_toks(int expand, int expandlist, int *head) // todo: tail 
{
    halfword p = null;
    if (expand) {
        do {
            tex_get_x_token();
        } while (cur_cmd == spacer_cmd || cur_cmd == relax_cmd);
    } else {
        do {
            tex_get_token();
        } while (cur_cmd == spacer_cmd || cur_cmd == relax_cmd);
    }
    switch (cur_cmd) {
        case left_brace_cmd:
            p = expandlist ? tex_scan_toks_expand(1, NULL, 0, 0) : tex_scan_toks_normal(1, NULL);
            *head = p;
            break;
        case internal_toks_cmd:
        case register_toks_cmd:
            p = eq_value(cur_chr);
            break;
        case register_cmd:
            /* is this okay? probably not as cur_val can be way to large */
            if (cur_chr == token_val_level) {
                halfword n = tex_scan_toks_register_number();
                p = eq_value(register_toks_location(n));
                break;
            } else {
                goto DEFAULT;
            }
        case cs_name_cmd:
            if (cur_chr == last_named_cs_code) {
                if (lmt_scanner_state.last_cs_name != null_cs) {
                    p = eq_value(lmt_scanner_state.last_cs_name);
                }
                break;
            } else { 
                /* fall through */
            }
        case call_cmd:
        case protected_call_cmd:
        case semi_protected_call_cmd:
        case constant_call_cmd:
        case tolerant_call_cmd:
        case tolerant_protected_call_cmd:
        case tolerant_semi_protected_call_cmd:
            p = eq_value(cur_cs);
            break;
        default:
          DEFAULT:
            {
                halfword n;
                tex_back_input(cur_tok);
                n = tex_scan_toks_register_number();
                p = eq_value(register_toks_location(n));
                break;
            }
    }
    /* skip over the ref count */
    return p ? token_link(p) : null;
}

/*tex 
    
    We can at some point (when deemed critital in \CONTEXT) add these: 

    \starttyping 
    \counttok   token     tokenlist 
    \counttoks  tokenlist tokenlist 
    \countxtoks tokenlist tokenlist % expanded 
    \countchar  chartoken tokenlist
    \stoptyping 

    but for now we keep them local (static).
*/

static halfword tex_aux_count_tok(int once)
{
    halfword qq = null;
    halfword p, q;
    halfword result = 0;
    int save_scanner_status = lmt_input_state.scanner_status;
    lmt_input_state.scanner_status = scanner_is_normal;
    p = tex_get_token();
    q = tex_aux_grab_toks(0, 0, &qq);
    if (p == q) {
        result = 1;
    } else {
        while (q) {
            if (p == token_info(q)) {
                result += 1;
                if (once) { 
                    break;
                }
            } else {
                q = token_link(q);
            }
        }
    }
    if (qq) {
        tex_flush_token_list(qq);
    }
    lmt_input_state.scanner_status = save_scanner_status;
    return result;
}

static halfword tex_aux_count_toks(int once, int expand)
{
    halfword pp = null;
    halfword p;
    halfword result = 0;
    int save_scanner_status = lmt_input_state.scanner_status;
    lmt_input_state.scanner_status = scanner_is_normal;
    p = tex_aux_grab_toks(expand, expand, &pp);
    if (p) {
        halfword qq = null;
        halfword q = tex_aux_grab_toks(expand, expand, &qq);
        if (p == q) {
            result = 1;
        } else {
            int qh = q;
            int ph = p;
            while (p && q) {
                halfword pt = token_info(p);
                halfword qt = token_info(q);
              AGAIN:
                if (pt == qt) {
                    p = token_link(p);
                    q = token_link(q);
                } else if (token_cmd(pt) == ignore_cmd && token_cmd(qt) >= ignore_cmd && token_cmd(qt) <= other_char_cmd) {
                    p = token_link(p);
                    if (token_chr(pt) == token_chr(qt)) {
                        q = token_link(q);
                    } else {
                        pt = token_info(p);
                        goto AGAIN;
                    }
                } else {
                    p = ph;
                    q = token_link(qh);
                    qh = q;
                }
                if (! p) {
                    result += 1;
                    if (once) { 
                        break;
                    }
                }
            }
        }
        if (qq) {
            tex_flush_token_list(qq);
        }
    }
    if (pp) {
        tex_flush_token_list(pp);
    }
    lmt_input_state.scanner_status = save_scanner_status;
    return result;
}

static halfword tex_aux_count_char(int once)
{
    halfword tok;
    halfword qq = null;
    halfword q;
    halfword result = 0;
    int save_scanner_status = lmt_input_state.scanner_status;
    lmt_input_state.scanner_status = scanner_is_normal;
    tok = tex_get_token();
    q = tex_aux_grab_toks(0, 0, &qq);
    if (q) {
        int nesting = 0;
        while (q) {
            if (! nesting && token_info(q) == tok) {
                result += 1;
                if (once) { 
                    break;
                }
            } else if (token_cmd(token_info(q)) == left_brace_cmd) {
                nesting += 1;
            } else if (token_cmd(token_info(q)) == right_brace_cmd) {
                nesting -= 1;
            }
            q = token_link(q);
        }
    }
    if (qq) {
        tex_flush_token_list(qq);
    }
    lmt_input_state.scanner_status = save_scanner_status;
    return result;
}

// static inline halfword tex_aux_scan_comparison(int code)
// {
//     halfword r;
//     do {
//         tex_get_x_token();
//     } while (cur_cmd == spacer_cmd);
//     r = cur_tok - other_token;
//     if ((r < '<') || (r > '>')) {
//         tex_aux_missing_equal_error(code);
//         return '=';
//     } else {
//         return r;
//     }
// }

// static inline halfword tex_aux_scan_comparison(int code)
// {
//     do {
//         tex_get_x_token();
//     } while (cur_cmd == spacer_cmd);
//     if (cur_cmd != other_char_cmd || (cur_chr < '<') || (cur_chr > '>')) {
//         tex_aux_missing_equal_error(code);
//         return '=';
//     } else {
//         return cur_chr;
//     }
// }

typedef enum comparisons { 
    /* case 0 .. 7 */
    comparison_equal       = 0,
    comparison_less        = 1,
    comparison_greater     = 2,
    comparison_not_equal   = 3,
    comparison_not_less    = 4,
    comparison_not_greater = 5,
    comparison_element     = 6,
    comparison_not_element = 7,
} comparisons; 

typedef enum values { 
    /* case 1 .. 4 */
    value_less    = 1,
    value_equal   = 2,
    value_greater = 3,
    value_error   = 4,
} values;

typedef enum checks { 
    /* case 1 .. 2 */
    check_okay    = 1,
    check_error   = 2,
} checks;

typedef enum parameterstates { 
    parameter_zero  = 0,
    parameter_set   = 1,
    parameter_unset = 2,
} parameterstates;

static inline halfword tex_aux_scan_comparison(int code)
{
    bool negate = false;
    while (1) {
        tex_get_x_token();
        switch (cur_cmd) { 
            case letter_cmd: 
            case other_char_cmd: 
                switch (cur_chr) { 
                    /* traditional */
                    case '='   : return negate ? comparison_not_equal   : comparison_equal;
                    case '<'   : return negate ? comparison_not_less    : comparison_less;
                    case '>'   : return negate ? comparison_not_greater : comparison_greater;
                    /* also */
                    case '&'   : return negate ? comparison_not_element : comparison_element;
                    /* bonus */
                    case '!'   : negate = ! negate ; continue;
                    /* neat */
                    case 0x2208: return negate ? comparison_not_element : comparison_element; 
                    case 0x2209: return negate ? comparison_element     : comparison_not_element;
                    case 0x2260: return negate ? comparison_equal       : comparison_not_equal;
                    case 0x2264: return negate ? comparison_greater     : comparison_not_greater;
                    case 0x2265: return negate ? comparison_less        : comparison_not_less; 
                    case 0x2270: return negate ? comparison_greater     : comparison_not_greater;
                    case 0x2271: return negate ? comparison_less        : comparison_not_less; 
                }
            case spacer_cmd: 
                continue;
            default:
                tex_aux_missing_equal_error(code);
                return 0;
        }
    } 
}

static inline void tex_aux_check_strict(int *result)
{
    tex_get_x_token();
    switch (cur_cmd) { 
        case relax_cmd:
        case spacer_cmd: 
        case if_test_cmd:
            break;
        default: 
            *result = check_error;
            break;
    }
    tex_back_input(cur_tok);
}

void tex_conditional_if(halfword code, int unless)
{
    /*tex The result or case value. */
    int result = 0;
    /*tex The |cond_ptr| corresponding to this conditional: */
    halfword save_cond_ptr;
    /*tex Tracing options */
    int tracing_ifs = tracing_ifs_par > 0;
    int tracing_commands = tracing_commands_par;
    int tracing_both = tracing_ifs && (tracing_commands <= 1);
    if (tracing_both) {
        tex_show_cmd_chr(cur_cmd, cur_chr);
    }
    tex_aux_push_condition_stack(code, unless);
    save_cond_ptr = lmt_condition_state.cond_ptr;
    /*tex Either process |\ifcase| or set |b| to the value of a boolean condition. */
  HERE:
    /*tex We can get back here so we need to make sure result is always set! */
    lmt_condition_state.if_step = (singleword) code;
    lmt_condition_state.if_unless = (singleword) unless;
    switch (code) {
        case if_char_code:
        case if_cat_code:
            /*tex Test if two characters match. Seldom used, this one. */
            {
                halfword n, m;
                tex_aux_get_x_token_or_active_char();
                if ((cur_cmd > active_char_cmd) || (cur_chr > max_character_code)) {
                    /*tex It's not a character. */
                    m = relax_cmd;
                    n = relax_code;
                } else {
                    m = cur_cmd;
                    n = cur_chr;
                }
                tex_aux_get_x_token_or_active_char();
                if ((cur_cmd > active_char_cmd) || (cur_chr > max_character_code)) {
                    cur_cmd = relax_cmd;
                    cur_chr = relax_code;
                }
                result = code == if_char_code ? (n == cur_chr) : (m == cur_cmd);
            }
            goto RESULT;
        case if_int_code:
        case if_abs_int_code:
            {
                halfword n1 = tex_scan_integer(0, NULL, NULL);
                halfword cp = tex_aux_scan_comparison(code);
                halfword n2 = tex_scan_integer(0, NULL, NULL);
                if (code == if_abs_int_code) {
                    if (n1 < 0) {
                        n1 = -n1;
                    }
                    if (n2 < 0) {
                        n2 = -n2;
                    }
                }
                switch (cp) {
                    case comparison_equal      : result = (n1 == n2); break;
                    case comparison_less       : result = (n1 <  n2); break;
                    case comparison_greater    : result = (n1  > n2); break;
                    case comparison_not_equal  : result = (n1 != n2); break;
                    case comparison_not_less   : result = (n1 >= n2); break;
                    case comparison_not_greater: result = (n1 <= n2); break;
                    case comparison_element    : result = (n1 & n2) == n1; break;
                    case comparison_not_element: result = (n1 & n2) != n1; break;
                }
            }
            goto RESULT;
        case if_zero_int_code:
            result = tex_scan_integer(0, NULL, NULL) == 0;
            goto RESULT;
        case if_interval_int_code:
            {
                scaled n0 = tex_scan_integer(0, NULL, NULL);
                scaled n1 = tex_scan_integer(0, NULL, NULL);
                scaled n2 = tex_scan_integer(0, NULL, NULL);
                result = n1 - n2;
                result = result == 0 ? 1 : (result > 0 ? result <= n0 : -result <= n0);
            }
            goto RESULT;
        case if_posit_code:
        case if_abs_posit_code:
            {
                halfword n1 = tex_scan_posit(0);
                halfword cp = tex_aux_scan_comparison(code);
                halfword n2 = tex_scan_posit(0);
                if (code == if_abs_posit_code) {
                    tex_posit zero = tex_integer_to_posit(0);
                    if (tex_posit_lt(n1,zero.v)) {
                        n1 = tex_posit_neg(n1);
                    }
                    if (tex_posit_lt(n2,zero.v)) {
                        n2 = tex_posit_neg(n2);
                    }
                }
                switch (cp) {
                    case comparison_equal      : result = tex_posit_eq(n1,n2); break;
                    case comparison_less       : result = tex_posit_lt(n1,n2); break;
                    case comparison_greater    : result = tex_posit_gt(n1,n2); break;
                    case comparison_not_equal  : result = tex_posit_ne(n1,n2); break;
                    case comparison_not_less   : result = tex_posit_gt(n1,n2); break;
                    case comparison_not_greater: result = tex_posit_lt(n1,n2); break;
                    case comparison_element    : result = tex_posit_eq(tex_integer_to_posit(tex_posit_to_integer(n1) & tex_posit_to_integer(n2)).v,n1); break;
                    case comparison_not_element: result = tex_posit_ne(tex_integer_to_posit(tex_posit_to_integer(n1) & tex_posit_to_integer(n2)).v,n1); break;
                }
            }
            goto RESULT;
        case if_zero_posit_code:
            result = tex_posit_eq_zero(tex_scan_posit(0));
            goto RESULT;
        case if_interval_posit_code:
            {
                halfword n0 = tex_scan_posit(0);
                halfword n1 = tex_scan_posit(0);
                halfword n2 = tex_scan_posit(0);
                result = tex_posit_sub(n1, n2);
                result = tex_posit_eq_zero(result) ? 1 : (tex_posit_gt_zero(result) ? tex_posit_le(result, n0) : tex_posit_le(tex_posit_neg(result), n0));
            }
            goto RESULT;
        case if_dim_code:
        case if_abs_dim_code:
            {
                scaled n1 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                halfword cp = tex_aux_scan_comparison(code);
                scaled n2 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                if (code == if_abs_dim_code) {
                    if (n1 < 0) {
                        n1 = -n1;
                    }
                    if (n2 < 0) {
                        n2 = -n2;
                    }
                }
                switch (cp) {
                    case comparison_equal      : result = (n1 == n2); break;
                    case comparison_less       : result = (n1 <  n2); break;
                    case comparison_greater    : result = (n1  > n2); break;
                    case comparison_not_equal  : result = (n1 != n2); break;
                    case comparison_not_less   : result = (n1 >= n2); break;
                    case comparison_not_greater: result = (n1 <= n2); break;
                    case comparison_element    : result = (n1/65536 & n2/65536) == n1/65536; break; /* maybe we should round */
                    case comparison_not_element: result = (n1/65536 & n2/65536) != n1/65536; break; /* maybe we should round */
                }
            }
            goto RESULT;
        case if_zero_dim_code:
            result = tex_scan_dimension(0, 0, 0, 0, NULL, NULL) == 0;
            goto RESULT;
        case if_interval_dim_code:
            {
                scaled n0 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                scaled n1 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                scaled n2 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                result = n1 - n2;
                result = result == 0 ? 1 : (result > 0 ? result <= n0 : -result <= n0);
            }
            goto RESULT;
        case if_odd_code:
            result = odd(tex_scan_integer(0, NULL, NULL));
            goto RESULT;
        case if_vmode_code:
            result = is_v_mode(cur_list.mode);
            goto RESULT;
        case if_hmode_code:
            result = is_h_mode(cur_list.mode);
            goto RESULT;
        case if_mmode_code:
            result = is_m_mode(cur_list.mode);
            goto RESULT;
        case if_inner_code:
            result = cur_list.mode < nomode;
            goto RESULT;
        case if_void_code:
            {
                halfword n = tex_scan_box_register_number();
                result = box_register(n) == null;
            }
            goto RESULT;
        case if_hbox_code:
            {
                halfword n = tex_scan_box_register_number();
                halfword p = box_register(n);
                result = p && (node_type(p) == hlist_node);
            }
            goto RESULT;
        case if_vbox_code:
            {
                halfword n = tex_scan_box_register_number();
                halfword p = box_register(n);
                result = p && (node_type(p) == vlist_node);
            }
            goto RESULT;
        case if_tok_code:
        case if_cstok_code:
            {
                halfword pp = null;
                halfword qq = null;
                halfword p, q;
                int expand = code == if_tok_code;
                int save_scanner_status = lmt_input_state.scanner_status;
                lmt_input_state.scanner_status = scanner_is_normal;
                p = tex_aux_grab_toks(expand, 1, &pp);
                q = tex_aux_grab_toks(expand, 1, &qq);
                if (p == q) {
                    /* this is sneaky, a list always is different */
                    result = 1;
                } else {
                    while (p && q) {
                        if (token_info(p) != token_info(q)) {
                         // p = null;
                         // break;
                            result = 0;
                            goto IFTOKDONE;
                        } else {
                            p = token_link(p);
                            q = token_link(q);
                        }
                    }
                    result = (! p) && (! q);
                }
              IFTOKDONE:
                if (pp) {
                    tex_flush_token_list(pp);
                }
                if (qq) {
                    tex_flush_token_list(qq);
                }
                lmt_input_state.scanner_status = save_scanner_status;
            }
            goto RESULT;
        case if_last_named_cs_code:
        case if_x_code:
            {
                /*tex
                    Test if two tokens match. Note that |\ifx| will declare two macros different
                    if one is |\long| or |\outer| and the other isn't, even though the texts of
                    the macros are the same.

                    We need to reset |scanner_status|, since |\outer| control sequences are
                    allowed, but we might be scanning a macro definition or preamble.

                    This is no longer true as we dropped these properties but it does apply to
                    protected macros and such.
                 */
             // halfword p, q, n;
                halfword p, q;
                int save_scanner_status = lmt_input_state.scanner_status;
                lmt_input_state.scanner_status = scanner_is_normal;
                if (code == if_x_code) {
                    tex_get_next();
                 // n = cur_cs;
                    p = cur_cmd;
                    q = cur_chr;
                } else {
                 // n = lmt_scanner_state.last_cs_name;
                    p = eq_type(lmt_scanner_state.last_cs_name); 
                    q = eq_value(lmt_scanner_state.last_cs_name);
                }
                tex_get_next();
                if ((p == constant_call_cmd && cur_cmd == call_cmd) || (p == call_cmd && cur_cmd == constant_call_cmd)) {
                    /*tex This is a somewhat special case. */
                    goto SOMECALLCMD;
                } else if (cur_cmd != p) {
                    result = 0;
                } else if (cur_cmd < call_cmd) {
                    result = cur_chr == q;
                } else {
                    SOMECALLCMD:
                    /*tex
                        Test if two macro texts match. Note also that |\ifx| decides that macros
                        |\a| and |\b| are different in examples like this:

                        \starttyping
                        \def\a{\c}  \def\c{}
                        \def\b{\d}  \def\d{}
                        \stoptyping

                        We acctually have commands beyond valid call commands but they are never 
                        seen here. 
                    */
                    p = token_link(cur_chr);
                    /*tex Omit reference counts. */
                 // q = token_link(eq_value(n));
                    q = token_link(q);
                    if (p == q) {
                        result = 1;
                    /*
                    } else if (! q) {
                        result = 0;
                    */
                    } else {
                        while (p && q) {
                            if (token_info(p) != token_info(q)) {
                             // p = null;
                             // break;
                                result = 0;
                                goto IFXDONE;
                            } else {
                                p = token_link(p);
                                q = token_link(q);
                            }
                        }
                        result = (! p) && (! q);
                    }
                }
              IFXDONE:
                lmt_input_state.scanner_status = save_scanner_status;
            }
            goto RESULT;
        case if_true_code:
            result = 1;
            goto RESULT;
        case if_false_code:
            result = 0;
            goto RESULT;
        case if_chk_int_code:
            {
                lmt_error_state.intercept = 1; /* maybe ++ and -- so that we can nest */
                lmt_error_state.last_intercept = 0;
                lmt_condition_state.chk_integer = 0;
                tex_scan_integer_validate(); 
                result = lmt_error_state.last_intercept ? check_error : check_okay;
                lmt_error_state.intercept = 0;
                lmt_error_state.last_intercept = 0;
             /* goto CASE; */
                goto CASECHECK;
            }
        case if_chk_integer_code:
            {
                lmt_error_state.intercept = 1; /* maybe ++ and -- so that we can nest */
                lmt_error_state.last_intercept = 0;
                lmt_condition_state.chk_integer = tex_scan_integer(0, NULL, NULL); 
                result = lmt_error_state.last_intercept ? check_error : check_okay;
                if (result == check_okay) { 
                    tex_aux_check_strict(&result);
                }
                lmt_error_state.intercept = 0;
                lmt_error_state.last_intercept = 0;
             /* goto CASE; */
                goto CASECHECK;
            }
        case if_chk_intexpr_code: /* numeric result check */
            lmt_error_state.intercept = 1;
            lmt_error_state.last_intercept = 0;
            lmt_condition_state.chk_integer = tex_scan_expr(integer_val_level);
            result = lmt_error_state.last_intercept ? check_error : check_okay;
            if (result == check_okay) { 
                tex_aux_check_strict(&result);
            }
            lmt_error_state.intercept = 0;
            lmt_error_state.last_intercept = 0;
            goto CASECHECK;
        case if_val_int_code:
            {
                lmt_error_state.intercept = 1;
                lmt_error_state.last_intercept = 0;
                lmt_condition_state.chk_integer = tex_scan_integer(0, NULL, NULL);
                result = lmt_error_state.last_intercept ? value_error : (lmt_condition_state.chk_integer < 0) ? value_less : (lmt_condition_state.chk_integer > 0) ? value_greater : value_equal;
                lmt_error_state.intercept = 0;
                lmt_error_state.last_intercept = 0;
                goto CASE;
            }
        case if_cmp_int_code:
            {
                halfword n1 = tex_scan_integer(0, NULL, NULL);
                halfword n2 = tex_scan_integer(0, NULL, NULL);
                result = (n1 < n2) ? 0 : (n1 > n2) ? 2 : 1;
                goto CASE;
            }
        case if_chk_dim_code:
            {
                lmt_error_state.intercept = 1;
                lmt_error_state.last_intercept = 0;
                lmt_condition_state.chk_dimension = 0;
                tex_scan_dimension_validate(); 
                result = lmt_error_state.last_intercept ? check_error : check_okay;
                lmt_error_state.intercept = 0;
                lmt_error_state.last_intercept = 0;
             /* goto CASE; */
                goto CASECHECK;
            }
        case if_chk_dimension_code:
            {
                lmt_error_state.intercept = 1;
                lmt_error_state.last_intercept = 0;
                lmt_condition_state.chk_dimension = tex_scan_dimension(0, 0, 0, 0, NULL, NULL); 
                result = lmt_error_state.last_intercept ? check_error : check_okay;
                if (result == check_okay) { 
                    tex_aux_check_strict(&result);
                }
                lmt_error_state.intercept = 0;
                lmt_error_state.last_intercept = 0;
             /* goto CASE; */
                goto CASECHECK;
            }
        case if_chk_dimexpr_code: /* dimension result check */
            lmt_error_state.intercept = 1;
            lmt_error_state.last_intercept = 0;
            lmt_condition_state.chk_dimension = tex_scan_expr(dimension_val_level);
            result = lmt_error_state.last_intercept ? check_error : check_okay;
            if (result == check_okay) { 
                tex_aux_check_strict(&result);
            }
            lmt_error_state.intercept = 0;
            lmt_error_state.last_intercept = 0;
            goto CASECHECK;
        /* too messy as it expects a { } so best use \ifchkdimension 
         case if_chk_dimexpression_code:
         case if_chk_dimensionexpr_code: // alias
          // lmt_condition_state.chk_dimension = tex_scanned_expression(dimension_val_level);
        */
        case if_val_dim_code:
            {
                lmt_error_state.intercept = 1;
                lmt_error_state.last_intercept = 0;
                lmt_condition_state.chk_dimension = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                result = lmt_error_state.last_intercept ? value_error : (lmt_condition_state.chk_dimension < 0) ? value_less : (lmt_condition_state.chk_dimension > 0) ? value_greater : value_equal;
                lmt_error_state.intercept = 0;
                lmt_error_state.last_intercept = 0;
                goto CASE;
            }
        case if_cmp_dim_code:
            {
                scaled n1 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                scaled n2 = tex_scan_dimension(0, 0, 0, 0, NULL, NULL);
                result = (n1 < n2) ? 0 : (n1 > n2) ? 2 : 1;
                goto CASE;
            }
        case if_case_code:
            /*tex 
                Select the appropriate case and |return| or |goto common_ending|. We could 
                support |\unless| for a limited case but let's not mess around to much; after 
                all it is an original \TEX\ primitive. 
            */
            result = tex_scan_integer(0, NULL, NULL);
            goto CASE;
        case if_defined_code:
            /*tex
                The conditional |\ifdefined| tests if a control sequence is defined. We need to
                reset |scanner_status|, since |\outer| control sequences are allowed, but we
                might be scanning a macro definition or preamble.
            */
            {
                int save_scanner_status = lmt_input_state.scanner_status;
                lmt_input_state.scanner_status = scanner_is_normal;
                tex_get_next();
                result = cur_cmd != undefined_cs_cmd;
                lmt_input_state.scanner_status = save_scanner_status;
                goto RESULT;
            }
        case if_csname_code:
            result = tex_is_valid_csname();
            goto RESULT;
        case if_in_csname_code:
            /*tex This one will go away. */
            result = lmt_expand_state.cs_name_level;
            goto RESULT;
        case if_font_char_code:
            /*tex The conditional |\iffontchar| tests the existence of a character in a font. */
            {
                halfword fnt = tex_scan_font_identifier(NULL);
                halfword chr = tex_scan_char_number(0);
                result = tex_char_exists(fnt, chr);
            }
            goto RESULT;
        case if_condition_code:
            /*tex This can't happen! */
            goto RESULT;
        case if_flags_code:
            {
                halfword cs; 
                singleword flag;
                tex_get_r_token();
                cs = cur_cs; 
                flag = eq_flag(cs);
                /* todo: each prefix */
                tex_get_token();
                if (cur_cmd == prefix_cmd) {
                    /* Not all prefixes are reflected cq. stored with a contrtol sequence. */
                    switch (cur_chr) {
                        /*tex We check flags: */
                        case frozen_code        : result = is_frozen   (flag); break;
                        case permanent_code     : result = is_permanent(flag); break;
                        case immutable_code     : result = is_immutable(flag); break;
                     /* case primitive_code     : result = is_primitive(flag); break; */
                        case mutable_code       : result = is_mutable  (flag); break;
                        case noaligned_code     : result = is_noaligned(flag); break;
                        case instance_code      : result = is_instance (flag); break;
                        case untraced_code      : result = is_untraced (flag); break;
                        /*tex We check cmd: */
                        case global_code        : result = eq_level(cs) == level_one; break;
                        case tolerant_code      : result = is_tolerant_cmd(eq_type(cs)); break;
                        case protected_code     : result = is_protected_cmd(eq_type(cs)); break;
                     /* case overloaded_code    : */
                     /* case aliased_code       : */
                     /* case immediate_code     : */
                     /* case deferred_code      : */
                     /*      conditional_code     */
                     /*      value_code           */
                        case semiprotected_code : result = is_semi_protected_cmd(eq_type(cs)); break;
                     /* case enforced_code      : */ 
                     /* case always_code        : */
                     /* case inherited_code     : */  
                        case constant_code      : result = is_constant_cmd(eq_type(cs)); break;
                     /* case retained_code      : */
                     /* case constrained_code   : */                                            
                    }
                } else {
                    int fl; 
                    tex_back_input(cur_tok);
                    fl = tex_scan_integer(1, NULL, NULL); 
                    result = (flag & fl) == fl;
                    if (! result) {
                        if (is_protected(fl)) {
                            result = is_protected_cmd(eq_type(cs));
                        } else if (is_semiprotected(fl)) {
                            result = is_semi_protected_cmd(eq_type(cs));
                        } else if (is_tolerant(fl)) {
                            result = is_tolerant_cmd(eq_type(cs));
                        } else if (is_global(fl)) {
                            result = eq_level(cs) == level_one;
                        }
                    }
                }
                goto RESULT;
            }
        case if_empty_code:
            {
                tex_get_token();
              EMPTY_CHECK_AGAIN:
                switch (cur_cmd) {
                    case call_cmd:
                    case constant_call_cmd:
                        result = ! token_link(cur_chr);
                        break;
                    case internal_toks_reference_cmd:
                    case register_toks_reference_cmd:
                        result = ! token_link(cur_chr);
                        break;
                    case register_cmd:
                        /*tex See |tex_aux_grab_toks|. */
                        if (cur_chr == token_val_level) {
                            halfword n = tex_scan_toks_register_number();
                            halfword p = eq_value(register_toks_location(n));
                            result = ! p || ! token_link(p);
                        } else {
                            result = 0;
                        }
                        break;
                    case internal_toks_cmd:
                    case register_toks_cmd:
                        { 
                            halfword p = eq_value(cur_chr);   
                            result = ! p || ! token_link(p);
                        }
                        break;
                    case left_brace_cmd:
                        {
                            halfword h = tex_scan_toks_expand(1, NULL, 0, 0);
                            result = token_link(h) == null; 
                            tex_flush_token_list(h);
                        }
                        break;
                    case cs_name_cmd:
                        if (cur_chr == last_named_cs_code && lmt_scanner_state.last_cs_name != null_cs) {
                            cur_cmd = eq_type(lmt_scanner_state.last_cs_name);
                            cur_chr = eq_value(lmt_scanner_state.last_cs_name);
                            goto EMPTY_CHECK_AGAIN;
                        } 
                        /* fall through */
                    default:
                        result = 0;
                }
                goto RESULT;
            }
        case if_relax_code:
            {
                tex_get_token();
                result = cur_cmd == relax_cmd;
                goto RESULT;
            }
        case if_boolean_code:
            result = tex_scan_integer(0, NULL, NULL) ? 1 : 0;
            goto RESULT;
        case if_numexpression_code: /* boolean check */
            result = tex_scanned_expression(integer_val_level) ? 1 : 0;
            goto RESULT;
        case if_dimexpression_code: /* boolean check */
            result = tex_scanned_expression(dimension_val_level) ? 1 : 0;
            goto RESULT;
        case if_math_parameter_code:
            /*tex
                A value of |1| means that the parameter is set to a non-zero value, while |2| means
                that it is unset.
            */
            {
                do {
                    tex_get_x_token();
                } while (cur_cmd == spacer_cmd);
                if (cur_cmd == math_parameter_cmd) {
                    int code = cur_chr;
                    int style = tex_scan_math_style_identifier(0, 0);
                    result = tex_get_math_parameter(style, code, NULL);
                    if (result == max_dimension) {
                        result = parameter_unset; 
                    } else if (result) {
                        result = parameter_set;
                    } else { 
                        result = parameter_zero; 
                    }
                } else {
                    tex_normal_error("mathparameter", "a valid parameter expected");
                    result = parameter_zero;
                }
                goto CASE;
            }
        case if_math_style_code:
            result = tex_current_math_style();
            goto CASE;
        case if_arguments_code:
            result = lmt_expand_state.arguments;
            goto CASE;
        case if_parameters_code:
            /*tex
                The result has the last non-null count. We could have the test in the for but let's
                keep it readable.
            */
            result = tex_get_parameter_count();
            goto CASE;
        case if_parameter_code:
            {
                /*tex
                    We need to pick up the next token but avoid replacement by the parameter which
                    happens in the getters: 0 = no parameter, 1 = okay, 2 = empty. This permits
                    usage like |\ifparameter#2\or yes\else no\fi| as with the other checkers.
                */
                if (lmt_input_state.cur_input.loc) {
                    halfword t = token_info(lmt_input_state.cur_input.loc);
                    if (t < cs_token_flag && token_cmd(t) == parameter_reference_cmd) {
                        lmt_input_state.cur_input.loc = token_link(lmt_input_state.cur_input.loc);
                        result = lmt_input_state.parameter_stack[lmt_input_state.cur_input.parameter_start + token_chr(t) - 1] != null ? 1 : 2;
                    } else { 
                        /*tex 
                            We have a replacement text so we check and backtrack. This is somewhat
                            tricky because a parameter can be a condition but we assume sane usage. 
                        */
                        tex_get_token();
                        switch (cur_cmd) { 
                            case if_test_cmd: 
                                result = 2;
                                break;
                            case index_cmd:
                                if (cur_chr >= 0 && cur_chr < lmt_input_state.parameter_stack_data.ptr) {
                                    result = lmt_input_state.parameter_stack[cur_chr] != null ? 1 : 2;
                                }
                                break;
                            default:
                                result = 1;
                                break;
                        }
                    }          
                    /*tex Because we only have two values we can actually support |\unless|. */
                 // if (unless) { 
                 //     result = result == 1 ? 2 : 1;
                 // }
                 // goto CASEWITHUNLESS;
                    goto CASECHECK;
                } else { 
                    goto CASE;
                }
            }
        case if_has_tok_code:
            result = tex_aux_count_tok(1);
            goto RESULT;
        case if_has_toks_code:
        case if_has_xtoks_code:
            result = tex_aux_count_toks(1, code == if_has_xtoks_code);
            goto RESULT;
        case if_has_char_code:
            result = tex_aux_count_char(1);
            goto RESULT;
        case if_insert_code:
            /* beware: it tests */
            result = ! tex_insert_is_void(tex_scan_integer(0, NULL, NULL));
            goto RESULT;
        case if_in_alignment_code:
            result = tex_in_alignment();
            goto RESULT;
        case if_cramped_code:
            result = tex_is_cramped_style(tex_scan_math_style_identifier(0, 0));
            goto RESULT;
        case if_list_code:
            {
                halfword n = tex_scan_box_register_number();
                result = box_register(n) != null && box_list(box_register(n)) != null;
            }
            goto RESULT;
     // case if_bitwise_and_code:
     //     {
     //         halfword n1 = scan_integer(0, NULL);
     //         halfword n2 = scan_integer(0, NULL);
     //         result = n1 & n2 ? 1 : 0;
     //         goto RESULT;
     //     }
        default:
            {
                int category;
                strnumber u = tex_save_cur_string();
                int save_scanner_status = lmt_input_state.scanner_status;
                lmt_input_state.scanner_status = scanner_is_normal;
                lmt_token_state.luacstrings = 0;
                category = lmt_function_call_by_category(code - last_if_test_code, 0, &result);
                tex_restore_cur_string(u);
                lmt_input_state.scanner_status = save_scanner_status;
                if (lmt_token_state.luacstrings > 0) {
                    tex_lua_string_start();
                }
                switch (category) {
                    case lua_value_integer_code:
                    case lua_value_cardinal_code:
                    case lua_value_dimension_code:
                        /* unless: we could swap 1 and 3 */
                        goto CASE;
                    case lua_value_boolean_code:
                        goto RESULT;
                    case lua_value_conditional_code:
                        /* can we stay in the condition */
                        tex_back_input(token_val(if_test_cmd, if_condition_code));
                        tex_aux_pop_condition_stack();
                        return;
                    default:
                        result = 0;
                        goto RESULT;
                }
            }
    }
  CASECHECK:
    if (unless) { 
        result = result == check_okay ? check_error : check_okay;
        goto CASEWITHUNLESS;
    } else { 
        goto CASE;
    }
  CASE:
    /*tex
       It is too messy to support |\unless| for case variants although for |\ifparameter| we do 
       support it (there are only a few outcomes there).
    */
    if (unless) {
        /*tex This could be a helper as we do this a few more times. */
        halfword online = tracing_online_par;
        tracing_online_par = 1;
        tex_begin_diagnostic();
        tex_print_format( "ignoring \\unless in %C test", if_test_cmd, code);
        tex_end_diagnostic();
        tracing_online_par = online;
    }
  CASEWITHUNLESS:
    if (tracing_commands > 1) {
        tex_aux_show_if_state(code, result);
    }
    while (result) {
        unless = tex_aux_pass_text_x(tracing_ifs, tracing_commands);
        if (tracing_both) {
            tex_show_cmd_chr(cur_cmd, cur_chr);
        }
        if (lmt_condition_state.cond_ptr == save_cond_ptr) {
            if (cur_chr >= first_real_if_test_code) {
                /*tex
                    We have an |or_else_cmd| here, but keep in mind that |\expandafter \ifx| and
                    |\unless \ifx| and |\ifcondition| don't work in such cases! We stay in this
                    function call.
                */
                if (cur_chr == if_condition_code) {
                 // goto COMMON_ENDING;
                    tex_aux_pop_condition_stack();
                    return;
                } else {
                    code = cur_chr;
                    goto HERE;
                }
            } else if (cur_chr == or_code) {
                --result;
            } else {
                goto COMMON_ENDING;
            }
        } else if (cur_chr == fi_code) {
            tex_aux_pop_condition_stack();
        }
    }
    tex_aux_change_if_limit(or_code, save_cond_ptr);
    /*tex Wait for |\or|, |\else|, or |\fi|. */
    return;
  RESULT:
    if (unless) {
        result = ! result;
    }
    if (tracing_commands > 1) {
        /*tex Display the value of |b|. */
        tex_begin_diagnostic();
        tex_print_str(result ? "{true}" : "{false}");
        tex_end_diagnostic();
    }
    if (result) {
        tex_aux_change_if_limit(else_code, save_cond_ptr);
        /*tex Wait for |\else| or |\fi|. */
        return;
    } else {
        /*tex
            Skip to |\else| or |\fi|, then |goto common_ending|. In a construction like |\if \iftrue
            abc\else d\fi|, the first |\else| that we come to after learning that the |\if| is false
            is not the |\else| we're looking for. Hence the following curious logic is needed.
        */
        while (1) {
            unless = tex_aux_pass_text_x(tracing_ifs, tracing_commands);
            if (tracing_both) {
                tex_show_cmd_chr(cur_cmd, cur_chr);
            }
            if (lmt_condition_state.cond_ptr == save_cond_ptr) {
                /* still fragile for |\unless| and |\expandafter| etc. */
                if (cur_chr >= first_real_if_test_code) {
                    if (cur_chr == if_condition_code) {
                     // goto COMMON_ENDING;
                        tex_aux_pop_condition_stack();
                        return;
                    } else {
                        code = cur_chr;
                        goto HERE;
                    }
                } else if (cur_chr != or_code) {
                    goto COMMON_ENDING;
                } else {
                    tex_handle_error(
                        normal_error_type,
                        "Extra \\or",
                        "I'm ignoring this; it doesn't match any \\if."
                    );
                }
            } else if (cur_chr == fi_code) {
                tex_aux_pop_condition_stack();
            }
        }
    }
  COMMON_ENDING:
    if (cur_chr == fi_code) {
        tex_aux_pop_condition_stack();
    } else {
        /*tex Wait for |\fi|. */
//lmt_condition_state.if_step = code;
        lmt_condition_state.if_limit = fi_code;
    }
}

/*tex
    Terminate the current conditional and skip to |\fi| The processing of conditionals is complete
    except for the following code, which is actually part of |expand|. It comes into play when
    |\or|, |\else|, or |\fi| is scanned.
*/

void tex_conditional_fi_or_else(void)
{
    int tracing_ifs = tracing_ifs_par > 0;
    if (tracing_ifs && tracing_commands_par <= 1) {
        tex_show_cmd_chr(if_test_cmd, cur_chr);
    }
    if (cur_chr == or_else_code || cur_chr == or_unless_code) {
        tex_get_next_non_spacer();
    } else if (cur_chr > lmt_condition_state.if_limit) {
        if (lmt_condition_state.if_limit == if_code) {
            /*tex The condition is not yet evaluated. */
            tex_insert_relax_and_cur_cs();
        } else {
            tex_handle_error(normal_error_type,
                "Extra %C",
                if_test_cmd, cur_chr,
                "I'm ignoring this; it doesn't match any \\if."
            );
        }
        /*tex We don't pop the stack! */
        return;
    }
    /*tex Skip to |\fi|. */
    while (! (cur_cmd == if_test_cmd && cur_chr == fi_code)) {
        tex_aux_pass_text();
        if (tracing_ifs) {
            tex_show_cmd_chr(cur_cmd, cur_chr);
        }
    }
    tex_aux_pop_condition_stack();
}

/*tex

    Negate a boolean conditional and |goto reswitch|. The result of a boolean condition is reversed
    when the conditional is preceded by |\unless|. We silently ignore |\unless| for those tests that
    act like an |\ifcase|. In \ETEX\ there was an error message.

*/

void tex_conditional_unless(void)
{
    tex_get_token();
    if (cur_cmd == if_test_cmd) {
        if (tracing_commands_par > 1) {
            tex_show_cmd_chr(cur_cmd, cur_chr);
        }
        if (cur_chr != if_condition_code) {;
            tex_conditional_if(cur_chr, 1);
        }
    } else {
        tex_handle_error(back_error_type,
            "You can't use '\\unless' before '%C'",
            cur_cmd, cur_chr,
            "Continue, and I'll forget that it ever happened."
        );
    }
}

void tex_show_ifs(void)
{
    if (lmt_condition_state.cond_ptr) {
        /*tex First we determine the |\if ... \fi| nesting. */
        int n = 0;
        {
            /*tex We start at the tail of a token list to show. */
            halfword p = lmt_condition_state.cond_ptr;
            do {
                ++n;
                p = node_next(p);
            } while (p);
        }
        /*tex Now reporting can start. */
        {
            halfword cond_ptr = lmt_condition_state.cond_ptr;
            int cur_if = lmt_condition_state.cur_if;
            int cur_unless = lmt_condition_state.cur_unless;
            int if_step = lmt_condition_state.if_step;
            int if_unless = lmt_condition_state.if_unless;
            int if_line = lmt_condition_state.if_line;
            int if_limit = lmt_condition_state.if_limit;
            do {
                if (cur_unless) {
                    if (if_line) {
                        tex_print_format("[conditional: level %i, current %C %C, limit %C, %sstep %C, line %i]",
                            n,
                            expand_after_cmd, expand_unless_code,
                            if_test_cmd, cur_if,
                            if_test_cmd, if_limit,
                            if_unless ? "unless " : "",
                            if_test_cmd, if_step,
                            if_line
                       );
                    } else {
                        tex_print_format("[conditional: level %i, current %C %C, limit %C, %sstep %C]",
                            n,
                            expand_after_cmd, expand_unless_code,
                            if_test_cmd, cur_if,
                            if_test_cmd, if_limit,
                            if_unless ? "unless " : "",
                            if_test_cmd, if_step
                        );
                    }
                } else {
                    if (if_line) {
                        tex_print_format("[conditional: level %i, current %C, limit %C, %sstep %C, line %i]",
                            n,
                            if_test_cmd, cur_if,
                            if_test_cmd, if_limit,
                            if_unless ? "unless " : "",
                            if_test_cmd, if_step,
                            if_line
                        );
                    } else {
                        tex_print_format("[conditional: level %i, current %C, limit %C, %sstep %C]",
                            n,
                            if_test_cmd, cur_if,
                            if_test_cmd, if_limit,
                            if_unless ? "unless " : "",
                            if_test_cmd, if_step
                        );
                    }
                }
                --n;
                cur_if = if_limit_subtype(cond_ptr);
                cur_unless = if_limit_unless(cond_ptr);;
                if_step = if_limit_step(cond_ptr);;
                if_unless = if_limit_stepunless(cond_ptr);;
                if_line = if_limit_line(cond_ptr);;
                if_limit = if_limit_type(cond_ptr);;
                cond_ptr = node_next(cond_ptr);
                if (cond_ptr) {
                    tex_print_levels();
                }
            } while (cond_ptr);
        }
    } else {
        tex_print_str("[conditional: none active]");
    }
}

void tex_conditional_catch_up(void)
{
    condition_state_info saved_condition_state = lmt_condition_state;
    while (lmt_input_state.in_stack[lmt_input_state.in_stack_data.ptr].if_ptr != lmt_condition_state.cond_ptr) {
        /* todo, more info */
        tex_print_nlp();
        tex_print_format("Warning: end of file when %C", if_test_cmd, lmt_condition_state.cur_if);
        if (lmt_condition_state.if_limit == fi_code) {
            tex_print_str_esc("else");
        }
        if (lmt_condition_state.if_line) {
            tex_print_format(" entered on line %i", lmt_condition_state.if_line);
        }
        tex_print_str(" is incomplete");
        lmt_condition_state.cur_if = if_limit_subtype(lmt_condition_state.cond_ptr);
        lmt_condition_state.cur_unless = if_limit_unless(lmt_condition_state.cond_ptr);
        lmt_condition_state.if_step = if_limit_step(lmt_condition_state.cond_ptr);
        lmt_condition_state.if_unless = if_limit_stepunless(lmt_condition_state.cond_ptr);
        lmt_condition_state.if_limit = if_limit_type(lmt_condition_state.cond_ptr);
        lmt_condition_state.if_line = if_limit_line(lmt_condition_state.cond_ptr);
        lmt_condition_state.cond_ptr = node_next(lmt_condition_state.cond_ptr);
    }
    lmt_condition_state = saved_condition_state;
}

/*tex 

    There is no gain over |\expandafter| because backing up is what takes time. We just keep this 
    as reference.
*/ 

/*
void tex_conditional_after_fi(void)
{
    halfword t = tex_get_token();
    int tracing_ifs = tracing_ifs_par > 0;
    int tracing_commands = tracing_commands_par > 0;
    while (1) {
        tex_aux_pass_text_x(tracing_ifs, tracing_commands);
        if (cur_chr == fi_code) {
            tex_aux_pop_condition_stack();
            break;
        } else {
            // some error
        }
    }
    tex_back_input(t);
}
*/
