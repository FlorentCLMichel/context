/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex TODO: update field specifications for added node entries */

/*tex

    This module started out using DEBUG to trigger checking invalid node usage, something that is
    needed because users can mess up nodes in \LUA. At some point that code was always enabled so
    it is now always on but still can be recognized as additional code. And as the performance hit
    is close to zero so disabling makes no sense, not even to make it configurable. There is a
    little more memory used but that is neglectable compared to other memory usage. Only on
    massive freeing we can gain.

*/

node_memory_state_info lmt_node_memory_state = {
    .nodes       = NULL,
    .nodesizes   = NULL,
    .free_chain  = { null },
    .nodes_data  = {
        .minimum   = min_node_size,
        .maximum   = max_node_size,
        .size      = siz_node_size,
        .step      = stp_node_size,
        .allocated = 0,
        .itemsize  = sizeof(memoryword) + sizeof(char),
        .top       = 0, // beware, node pointers are just offsets below top
        .ptr       = 0, // total size in use
        .initial   = 0,
        .offset    = 0,
        .extra     = 0,
    },
    .reserved                   = 0,
    .padding                    = 0,
    .node_properties_id         = 0,
    .lua_properties_level       = 0,
    .attribute_cache            = 0,
    .max_used_attribute         = 1,
    .node_properties_table_size = 0,
};

/*tex Defined below. */

static void     tex_aux_check_node      (halfword node);
static halfword tex_aux_allocated_node  (int size);

/*tex

    The following definitions are used for keys at the \LUA\ end and provide an efficient way to
    share hashed strings. For a long time we had this:

    static value_info lmt_node_fields_accent  [10];

    node_info lmt_node_data[] = {
        { .id = hlist_node, .size = box_node_size, .subtypes = NULL, .fields = lmt_node_fields_list, .name = NULL, .lua = 0, .visible = 1 },
        ....
    } ;

    etc but eventually we went a bit more dynamic because after all some helpers showeed up. This
    brings many node properties together. Not all nodes are visible for users. Most of the
    properties can be provided as lists.

    not all math noad fields are mentioned here yet ... some are still experimental

*/

void lmt_nodelib_initialize(void) {

    /*tes The subtypes of nodes. */

    value_info
        *subtypes_dir, *subtypes_par, *subtypes_glue, *subtypes_boundary, *subtypes_penalty, *subtypes_kern,
        *subtypes_rule, *subtypes_glyph , *subtypes_disc, *subtypes_list, *subtypes_adjust, *subtypes_mark,
        *subtypes_math, *subtypes_noad, *subtypes_radical, *subtypes_choice, *subtypes_accent, *subtypes_fence,
        *subtypes_fraction, *subtypes_split, *subtypes_attribute;

    value_info
        *lmt_node_fields_accent, *lmt_node_fields_adjust, *lmt_node_fields_attribute,
        *lmt_node_fields_boundary, *lmt_node_fields_choice, *lmt_node_fields_delimiter, *lmt_node_fields_dir,
        *lmt_node_fields_disc, *lmt_node_fields_fence, *lmt_node_fields_fraction, *lmt_node_fields_glue,
        *lmt_node_fields_glue_spec, *lmt_node_fields_glyph, *lmt_node_fields_insert, *lmt_node_fields_split,
        *lmt_node_fields_kern, *lmt_node_fields_list, *lmt_node_fields_par, *lmt_node_fields_mark, *lmt_node_fields_math,
        *lmt_node_fields_math_kernel, *lmt_node_fields_noad, *lmt_node_fields_penalty,
        *lmt_node_fields_radical, *lmt_node_fields_rule, *lmt_node_fields_style, *lmt_node_fields_parameter,
        *lmt_node_fields_math_sub, *lmt_node_fields_unset, *lmt_node_fields_whatsit,
        *lmt_node_fields_align_record;

    subtypes_dir = lmt_aux_allocate_value_info(cancel_dir_subtype);

    set_value_entry_key(subtypes_dir, normal_dir_subtype, normal)
    set_value_entry_key(subtypes_dir, cancel_dir_subtype, cancel)

    subtypes_split = lmt_aux_allocate_value_info(insert_split_subtype);

    set_value_entry_key(subtypes_split, normal_split_subtype, normal)
    set_value_entry_key(subtypes_split, insert_split_subtype, insert)

    subtypes_par = lmt_aux_allocate_value_info(math_par_subtype);

    set_value_entry_key(subtypes_par, vmode_par_par_subtype,   vmodepar)
    set_value_entry_key(subtypes_par, local_box_par_subtype,   localbox)
    set_value_entry_key(subtypes_par, hmode_par_par_subtype,   hmodepar)
    set_value_entry_key(subtypes_par, parameter_par_subtype,   parameter)
    set_value_entry_key(subtypes_par, local_break_par_subtype, localbreak)
    set_value_entry_key(subtypes_par, math_par_subtype,        math)

    subtypes_glue = lmt_aux_allocate_value_info(u_leaders);

    set_value_entry_key(subtypes_glue, user_skip_glue,                userskip)
    set_value_entry_key(subtypes_glue, line_skip_glue,                lineskip)
    set_value_entry_key(subtypes_glue, baseline_skip_glue,            baselineskip)
    set_value_entry_key(subtypes_glue, par_skip_glue,                 parskip)
    set_value_entry_key(subtypes_glue, above_display_skip_glue,       abovedisplayskip)
    set_value_entry_key(subtypes_glue, below_display_skip_glue,       belowdisplayskip)
    set_value_entry_key(subtypes_glue, above_display_short_skip_glue, abovedisplayshortskip)
    set_value_entry_key(subtypes_glue, below_display_short_skip_glue, belowdisplayshortskip)
    set_value_entry_key(subtypes_glue, left_skip_glue,                leftskip)
    set_value_entry_key(subtypes_glue, right_skip_glue,               rightskip)
    set_value_entry_key(subtypes_glue, top_skip_glue,                 topskip)
    set_value_entry_key(subtypes_glue, bottom_skip_glue,              bottomskip)
    set_value_entry_key(subtypes_glue, split_top_skip_glue,           splittopskip)
    set_value_entry_key(subtypes_glue, tab_skip_glue,                 tabskip)
    set_value_entry_key(subtypes_glue, space_skip_glue,               spaceskip)
    set_value_entry_key(subtypes_glue, xspace_skip_glue,              xspaceskip)
    set_value_entry_key(subtypes_glue, zero_space_skip_glue,          zerospaceskip)
    set_value_entry_key(subtypes_glue, par_fill_left_skip_glue,       parfillleftskip)
    set_value_entry_key(subtypes_glue, par_fill_right_skip_glue,      parfillskip)
    set_value_entry_key(subtypes_glue, par_init_left_skip_glue,       parinitleftskip)
    set_value_entry_key(subtypes_glue, par_init_right_skip_glue,      parinitrightskip)
    set_value_entry_key(subtypes_glue, indent_skip_glue,              indentskip)
    set_value_entry_key(subtypes_glue, left_hang_skip_glue,           lefthangskip)
    set_value_entry_key(subtypes_glue, right_hang_skip_glue,          righthangskip)
    set_value_entry_key(subtypes_glue, correction_skip_glue,          correctionskip)
    set_value_entry_key(subtypes_glue, inter_math_skip_glue,          intermathskip)
    set_value_entry_key(subtypes_glue, ignored_glue,                  ignored)
    set_value_entry_key(subtypes_glue, page_glue,                     page)
    set_value_entry_key(subtypes_glue, math_skip_glue,                mathskip)
    set_value_entry_key(subtypes_glue, thin_mu_skip_glue,             thinmuskip)
    set_value_entry_key(subtypes_glue, med_mu_skip_glue,              medmuskip)
    set_value_entry_key(subtypes_glue, thick_mu_skip_glue,            thickmuskip)
    set_value_entry_key(subtypes_glue, conditional_math_glue,         conditionalmathskip)
    set_value_entry_key(subtypes_glue, rulebased_math_glue,           rulebasedmathskip)
    set_value_entry_key(subtypes_glue, mu_glue,                       muglue)
    set_value_entry_key(subtypes_glue, a_leaders,                     leaders)
    set_value_entry_key(subtypes_glue, c_leaders,                     cleaders)
    set_value_entry_key(subtypes_glue, x_leaders,                     xleaders)
    set_value_entry_key(subtypes_glue, g_leaders,                     gleaders)
    set_value_entry_key(subtypes_glue, u_leaders,                     uleaders)

    subtypes_boundary = lmt_aux_allocate_value_info(adjust_boundary);

    set_value_entry_key(subtypes_boundary, cancel_boundary,     cancel)
    set_value_entry_key(subtypes_boundary, user_boundary,       user)
    set_value_entry_key(subtypes_boundary, attribute_boundary,  attribute)
    set_value_entry_key(subtypes_boundary, protrusion_boundary, protrusion)
    set_value_entry_key(subtypes_boundary, word_boundary,       word)
    set_value_entry_key(subtypes_boundary, page_boundary,       page)
    set_value_entry_key(subtypes_boundary, math_boundary,       math)
    set_value_entry_key(subtypes_boundary, optional_boundary,   optional)
    set_value_entry_key(subtypes_boundary, lua_boundary,        lua)
    set_value_entry_key(subtypes_boundary, par_boundary,        par)
    set_value_entry_key(subtypes_boundary, balance_boundary,    balance)
    set_value_entry_key(subtypes_boundary, adjust_boundary,     adjust)

    subtypes_penalty = lmt_aux_allocate_value_info(equation_number_penalty_subtype);

    set_value_entry_key(subtypes_penalty, user_penalty_subtype,            userpenalty)
    set_value_entry_key(subtypes_penalty, linebreak_penalty_subtype,       linebreakpenalty)
    set_value_entry_key(subtypes_penalty, line_penalty_subtype,            linepenalty)
    set_value_entry_key(subtypes_penalty, word_penalty_subtype,            wordpenalty)
    set_value_entry_key(subtypes_penalty, final_penalty_subtype,           finalpenalty)
    set_value_entry_key(subtypes_penalty, orphan_penalty_subtype,          orphanpenalty)
    set_value_entry_key(subtypes_penalty, toddler_penalty_subtype,         toddlerpenalty)
    set_value_entry_key(subtypes_penalty, single_line_penalty_subtype,     singlelinepenalty)
    set_value_entry_key(subtypes_penalty, math_pre_penalty_subtype,        mathprepenalty)
    set_value_entry_key(subtypes_penalty, math_post_penalty_subtype,       mathpostpenalty)
    set_value_entry_key(subtypes_penalty, before_display_penalty_subtype,  beforedisplaypenalty)
    set_value_entry_key(subtypes_penalty, after_display_penalty_subtype,   afterdisplaypenalty)
    set_value_entry_key(subtypes_penalty, equation_number_penalty_subtype, equationnumberpenalty)

    subtypes_kern = lmt_aux_allocate_value_info(vertical_math_kern_subtype);

    set_value_entry_key(subtypes_kern, font_kern_subtype,            fontkern)
    set_value_entry_key(subtypes_kern, explicit_kern_subtype,        userkern)
    set_value_entry_key(subtypes_kern, accent_kern_subtype,          accentkern)
    set_value_entry_key(subtypes_kern, italic_kern_subtype,          italiccorrection)
    set_value_entry_key(subtypes_kern, left_correction_kern_subtype, leftcorrectionkern)
    set_value_entry_key(subtypes_kern, right_correction_kern_subtype,rightcorrectionkern)
    set_value_entry_key(subtypes_kern, space_font_kern_subtype,      spacefontkern)
    set_value_entry_key(subtypes_kern, left_margin_kern_subtype,     leftmarginkern)
    set_value_entry_key(subtypes_kern, right_margin_kern_subtype,    rightmarginkern)
    set_value_entry_key(subtypes_kern, explicit_math_kern_subtype,   mathkern)
    set_value_entry_key(subtypes_kern, math_shape_kern_subtype,      mathshapekern)
    set_value_entry_key(subtypes_kern, left_math_slack_kern_subtype, leftmathslackkern);
    set_value_entry_key(subtypes_kern, right_math_slack_kern_subtype,rightmathslackkern);
    set_value_entry_key(subtypes_kern, horizontal_math_kern_subtype, horizontalmathkern)
    set_value_entry_key(subtypes_kern, vertical_math_kern_subtype,   verticalmathkern)

    subtypes_rule = lmt_aux_allocate_value_info(spacing_rule_subtype);

    set_value_entry_key(subtypes_rule, normal_rule_subtype,        normal)
    set_value_entry_key(subtypes_rule, empty_rule_subtype,         empty)
    set_value_entry_key(subtypes_rule, strut_rule_subtype,         strut)
    set_value_entry_key(subtypes_rule, virtual_rule_subtype,       virtual)
    set_value_entry_key(subtypes_rule, outline_rule_subtype,       outline)
    set_value_entry_key(subtypes_rule, user_rule_subtype,          user)
    set_value_entry_key(subtypes_rule, math_over_rule_subtype,     over)
    set_value_entry_key(subtypes_rule, math_under_rule_subtype,    under)
    set_value_entry_key(subtypes_rule, math_fraction_rule_subtype, fraction)
    set_value_entry_key(subtypes_rule, math_radical_rule_subtype,  radical)
    set_value_entry_key(subtypes_rule, box_rule_subtype,           box)
    set_value_entry_key(subtypes_rule, image_rule_subtype,         image)
    set_value_entry_key(subtypes_rule, spacing_rule_subtype,       spacing)

    subtypes_glyph = lmt_aux_allocate_value_info(glyph_math_accent_subtype);

    set_value_entry_key(subtypes_glyph, glyph_unset_subtype,            unset)
    set_value_entry_key(subtypes_glyph, glyph_character_subtype,        character)
    set_value_entry_key(subtypes_glyph, glyph_ligature_subtype,         ligature)
    set_value_entry_key(subtypes_glyph, glyph_math_delimiter_subtype,   delimiter);
    set_value_entry_key(subtypes_glyph, glyph_math_extensible_subtype,  extensible);
    set_value_entry_key(subtypes_glyph, glyph_math_ordinary_subtype,    ordinary);
    set_value_entry_key(subtypes_glyph, glyph_math_operator_subtype,    operator);
    set_value_entry_key(subtypes_glyph, glyph_math_binary_subtype,      binary);
    set_value_entry_key(subtypes_glyph, glyph_math_relation_subtype,    relation);
    set_value_entry_key(subtypes_glyph, glyph_math_open_subtype,        open);
    set_value_entry_key(subtypes_glyph, glyph_math_close_subtype,       close);
    set_value_entry_key(subtypes_glyph, glyph_math_punctuation_subtype, punctuation);
    set_value_entry_key(subtypes_glyph, glyph_math_variable_subtype,    variable);
    set_value_entry_key(subtypes_glyph, glyph_math_active_subtype,      active);
    set_value_entry_key(subtypes_glyph, glyph_math_inner_subtype,       inner);
    set_value_entry_key(subtypes_glyph, glyph_math_over_subtype,        over);
    set_value_entry_key(subtypes_glyph, glyph_math_under_subtype,       under);
    set_value_entry_key(subtypes_glyph, glyph_math_fraction_subtype,    fraction);
    set_value_entry_key(subtypes_glyph, glyph_math_radical_subtype,     radical);
    set_value_entry_key(subtypes_glyph, glyph_math_middle_subtype,      middle);
    set_value_entry_key(subtypes_glyph, glyph_math_prime_subtype,       prime);
    set_value_entry_key(subtypes_glyph, glyph_math_accent_subtype,      accent);

    subtypes_disc = lmt_aux_allocate_value_info(syllable_discretionary_code);

    set_value_entry_key(subtypes_disc, normal_discretionary_code,      discretionary)
    set_value_entry_key(subtypes_disc, explicit_discretionary_code,    explicit)
    set_value_entry_key(subtypes_disc, automatic_discretionary_code,   automatic)
    set_value_entry_key(subtypes_disc, mathematics_discretionary_code, math)
    set_value_entry_key(subtypes_disc, syllable_discretionary_code,    regular)

    subtypes_fence = lmt_aux_allocate_value_info(no_fence_side);

    set_value_entry_key(subtypes_fence, unset_fence_side,    unset)
    set_value_entry_key(subtypes_fence, left_fence_side,     left)
    set_value_entry_key(subtypes_fence, middle_fence_side,   middle)
    set_value_entry_key(subtypes_fence, right_fence_side,    right)
    set_value_entry_key(subtypes_fence, left_operator_side,  operator)
    set_value_entry_key(subtypes_fence, no_fence_side,       no)

    subtypes_list = lmt_aux_allocate_value_info(spacing_list);

    set_value_entry_key(subtypes_list, unknown_list,              unknown)
    set_value_entry_key(subtypes_list, line_list,                 line)
    set_value_entry_key(subtypes_list, hbox_list,                 box)
    set_value_entry_key(subtypes_list, indent_list,               indent)
    set_value_entry_key(subtypes_list, container_list,            container)
    set_value_entry_key(subtypes_list, align_row_list,            alignment)
    set_value_entry_key(subtypes_list, align_cell_list,           cell)
    set_value_entry_key(subtypes_list, equation_list,             equation)
    set_value_entry_key(subtypes_list, equation_number_list,      equationnumber)
    set_value_entry_key(subtypes_list, math_list_list,            math)
    set_value_entry_key(subtypes_list, math_pack_list,            mathpack)
    set_value_entry_key(subtypes_list, math_char_list,            mathchar)
    set_value_entry_key(subtypes_list, math_h_extensible_list,    hextensible)
    set_value_entry_key(subtypes_list, math_v_extensible_list,    vextensible)
    set_value_entry_key(subtypes_list, math_h_delimiter_list,     hdelimiter)
    set_value_entry_key(subtypes_list, math_v_delimiter_list,     vdelimiter)
    set_value_entry_key(subtypes_list, math_over_delimiter_list,  overdelimiter)
    set_value_entry_key(subtypes_list, math_under_delimiter_list, underdelimiter)
    set_value_entry_key(subtypes_list, math_numerator_list,       numerator)
    set_value_entry_key(subtypes_list, math_denominator_list,     denominator)
    set_value_entry_key(subtypes_list, math_modifier_list,        modifier)
    set_value_entry_key(subtypes_list, math_fraction_list,        fraction)
    set_value_entry_key(subtypes_list, math_nucleus_list,         nucleus)
    set_value_entry_key(subtypes_list, math_sup_list,             sup)
    set_value_entry_key(subtypes_list, math_sub_list,             sub)
    set_value_entry_key(subtypes_list, math_prime_list,           prime)
    set_value_entry_key(subtypes_list, math_pre_post_sup_list,    prepostsup)
    set_value_entry_key(subtypes_list, math_pre_post_sub_list,    prepostsub)
    set_value_entry_key(subtypes_list, math_degree_list,          degree)
    set_value_entry_key(subtypes_list, math_scripts_list,         scripts)
    set_value_entry_key(subtypes_list, math_over_list,            over)
    set_value_entry_key(subtypes_list, math_under_list,           under)
    set_value_entry_key(subtypes_list, math_accent_list,          accent)
    set_value_entry_key(subtypes_list, math_radical_list,         radical)
    set_value_entry_key(subtypes_list, math_fence_list,           fence)
    set_value_entry_key(subtypes_list, math_rule_list,            rule)
    set_value_entry_key(subtypes_list, math_ghost_list,           ghost)
    set_value_entry_key(subtypes_list, math_text_list,            mathtext)
    set_value_entry_key(subtypes_list, insert_result_list,        insert)
    set_value_entry_key(subtypes_list, local_list,                local)
    set_value_entry_key(subtypes_list, local_left_list,           left)
    set_value_entry_key(subtypes_list, local_right_list,          right)
    set_value_entry_key(subtypes_list, local_middle_list,         middle)
    set_value_entry_key(subtypes_list, balance_slot_list,         balanceslot)
    set_value_entry_key(subtypes_list, balance_list,              balance)
    set_value_entry_key(subtypes_list, spacing_list,              spacing)

    subtypes_math = lmt_aux_allocate_value_info(end_broken_math);

    set_value_entry_key(subtypes_math, begin_inline_math, beginmath)
    set_value_entry_key(subtypes_math, end_inline_math,   endmath)
    set_value_entry_key(subtypes_math, begin_broken_math, beginbrokenmath)
    set_value_entry_key(subtypes_math, end_broken_math,   endbrokenmath)

    subtypes_adjust = lmt_aux_allocate_value_info(local_adjust_code);

    set_value_entry_key(subtypes_adjust, pre_adjust_code,   pre)
    set_value_entry_key(subtypes_adjust, post_adjust_code,  post)
    set_value_entry_key(subtypes_adjust, local_adjust_code, local)

    subtypes_mark = lmt_aux_allocate_value_info(reset_mark_value_code);

    set_value_entry_key(subtypes_mark, set_mark_value_code,   set)
    set_value_entry_key(subtypes_mark, reset_mark_value_code, reset)

    subtypes_noad = lmt_aux_allocate_value_info(vcenter_noad_subtype); // last_noad_subtype

    set_value_entry_key(subtypes_noad, ordinary_noad_subtype,    ordinary)
    set_value_entry_key(subtypes_noad, operator_noad_subtype,    operator)
    set_value_entry_key(subtypes_noad, binary_noad_subtype,      binary)
    set_value_entry_key(subtypes_noad, relation_noad_subtype,    relation)
    set_value_entry_key(subtypes_noad, open_noad_subtype,        open)
    set_value_entry_key(subtypes_noad, close_noad_subtype,       close)
    set_value_entry_key(subtypes_noad, punctuation_noad_subtype, punctuation)
    set_value_entry_key(subtypes_noad, variable_noad_subtype,    variable)
    set_value_entry_key(subtypes_noad, active_noad_subtype,      active)
    set_value_entry_key(subtypes_noad, inner_noad_subtype,       inner)
    set_value_entry_key(subtypes_noad, under_noad_subtype,       under)
    set_value_entry_key(subtypes_noad, over_noad_subtype,        over)
    set_value_entry_key(subtypes_noad, fraction_noad_subtype,    fraction)
    set_value_entry_key(subtypes_noad, radical_noad_subtype,     radical)
    set_value_entry_key(subtypes_noad, middle_noad_subtype,      middle)
    set_value_entry_key(subtypes_noad, prime_noad_subtype,       prime)
    set_value_entry_key(subtypes_noad, accent_noad_subtype,      accent)
    set_value_entry_key(subtypes_noad, fenced_noad_subtype,      fenced)
    set_value_entry_key(subtypes_noad, ghost_noad_subtype,       ghost)
    set_value_entry_key(subtypes_noad, vcenter_noad_subtype,     vcenter)

    subtypes_choice = lmt_aux_allocate_value_info(discretionary_choice_subtype);

    set_value_entry_key(subtypes_choice, normal_choice_subtype,        normal)
    set_value_entry_key(subtypes_choice, discretionary_choice_subtype, discretionary)

    subtypes_radical = lmt_aux_allocate_value_info(h_extensible_radical_subtype);

    set_value_entry_key(subtypes_radical, normal_radical_subtype,          normal)
    set_value_entry_key(subtypes_radical, radical_radical_subtype,         radical)
    set_value_entry_key(subtypes_radical, root_radical_subtype,            root)
    set_value_entry_key(subtypes_radical, rooted_radical_subtype,          rooted)
    set_value_entry_key(subtypes_radical, under_delimiter_radical_subtype, underdelimiter)
    set_value_entry_key(subtypes_radical, over_delimiter_radical_subtype,  overdelimiter)
    set_value_entry_key(subtypes_radical, delimiter_under_radical_subtype, delimiterunder)
    set_value_entry_key(subtypes_radical, delimiter_over_radical_subtype,  delimiterover)
    set_value_entry_key(subtypes_radical, delimited_radical_subtype,       delimited)
    set_value_entry_key(subtypes_radical, h_extensible_radical_subtype,    hextensible)

    subtypes_fraction = lmt_aux_allocate_value_info(stretched_fraction_subtype);

    set_value_entry_key(subtypes_fraction, over_fraction_subtype,      over)
    set_value_entry_key(subtypes_fraction, atop_fraction_subtype,      atop)
    set_value_entry_key(subtypes_fraction, above_fraction_subtype,     above)
    set_value_entry_key(subtypes_fraction, skewed_fraction_subtype,    skewed)
    set_value_entry_key(subtypes_fraction, stretched_fraction_subtype, stretched)

    subtypes_accent = lmt_aux_allocate_value_info(fixedboth_accent_subtype);

    set_value_entry_key(subtypes_accent, bothflexible_accent_subtype, bothflexible)
    set_value_entry_key(subtypes_accent, fixedtop_accent_subtype,     fixedtop)
    set_value_entry_key(subtypes_accent, fixedbottom_accent_subtype,  fixedbottom)
    set_value_entry_key(subtypes_accent, fixedboth_accent_subtype,    fixedboth)

    subtypes_attribute = lmt_aux_allocate_value_info(attribute_value_subtype);

    set_value_entry_key(subtypes_attribute, attribute_list_subtype,  list)
    set_value_entry_key(subtypes_attribute, attribute_value_subtype, value)

    /*tex The fields of nodes. I need to update these! */

    lmt_node_fields_accent = lmt_aux_allocate_value_info(7);

    set_value_entry_val(lmt_node_fields_accent, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_accent, 1, node_list_field, bottomaccent);
    set_value_entry_val(lmt_node_fields_accent, 2, node_list_field, topaccent);
    set_value_entry_val(lmt_node_fields_accent, 3, node_list_field, overlayaccent) /* middle */;
    set_value_entry_val(lmt_node_fields_accent, 4, node_list_field, fraction);
    set_value_entry_val(lmt_node_fields_accent, 5, node_list_field, topovershoot);
    set_value_entry_val(lmt_node_fields_accent, 6, node_list_field, bottomovershoot);

    lmt_node_fields_adjust = lmt_aux_allocate_value_info(8);

    set_value_entry_val(lmt_node_fields_adjust, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_adjust, 1, node_list_field, list);
    set_value_entry_val(lmt_node_fields_adjust, 1, integer_field,   options);
    set_value_entry_val(lmt_node_fields_adjust, 1, integer_field,   index);
    set_value_entry_val(lmt_node_fields_adjust, 1, node_list_field, except);
    set_value_entry_val(lmt_node_fields_adjust, 1, dimension_field, depthbefore);
    set_value_entry_val(lmt_node_fields_adjust, 1, dimension_field, depthafter);

    lmt_node_fields_attribute = lmt_aux_allocate_value_info(4);

    set_value_entry_val(lmt_node_fields_attribute, 0, integer_field, count);
    set_value_entry_val(lmt_node_fields_attribute, 1, integer_field, data);
    set_value_entry_val(lmt_node_fields_attribute, 2, integer_field, index);
    set_value_entry_val(lmt_node_fields_attribute, 3, integer_field, value);

    /* Nothing */

    lmt_node_fields_boundary = lmt_aux_allocate_value_info(2);

    set_value_entry_val(lmt_node_fields_boundary, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_boundary, 1, integer_field,   data);

    lmt_node_fields_choice = lmt_aux_allocate_value_info(9);

    set_value_entry_val(lmt_node_fields_choice, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_choice, 1, node_list_field, display);
    set_value_entry_val(lmt_node_fields_choice, 2, node_list_field, text);
    set_value_entry_val(lmt_node_fields_choice, 3, node_list_field, script);
    set_value_entry_val(lmt_node_fields_choice, 4, node_list_field, scriptscript);
    set_value_entry_val(lmt_node_fields_choice, 5, integer_field,   class);
    set_value_entry_val(lmt_node_fields_choice, 6, node_list_field, pre);
    set_value_entry_val(lmt_node_fields_choice, 7, node_list_field, post);
    set_value_entry_val(lmt_node_fields_choice, 8, node_list_field, replace);

    lmt_node_fields_delimiter = lmt_aux_allocate_value_info(8);

    set_value_entry_val(lmt_node_fields_delimiter, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_delimiter, 1, integer_field,   smallfamily);
    set_value_entry_val(lmt_node_fields_delimiter, 2, integer_field,   smallchar);
    set_value_entry_val(lmt_node_fields_delimiter, 3, integer_field,   largefamily);
    set_value_entry_val(lmt_node_fields_delimiter, 4, integer_field,   largechar);
    set_value_entry_val(lmt_node_fields_delimiter, 4, integer_field,   properties);
    set_value_entry_val(lmt_node_fields_delimiter, 4, integer_field,   group);
    set_value_entry_val(lmt_node_fields_delimiter, 4, integer_field,   index);

    lmt_node_fields_dir = lmt_aux_allocate_value_info(3);

    set_value_entry_val(lmt_node_fields_dir, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_dir, 1, integer_field,   dir);
    set_value_entry_val(lmt_node_fields_dir, 2, integer_field,   level);

    lmt_node_fields_disc = lmt_aux_allocate_value_info(7);

    set_value_entry_val(lmt_node_fields_disc, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_disc, 1, node_list_field, pre);
    set_value_entry_val(lmt_node_fields_disc, 2, node_list_field, post);
    set_value_entry_val(lmt_node_fields_disc, 3, node_list_field, replace);
    set_value_entry_val(lmt_node_fields_disc, 4, integer_field,   penalty);
    set_value_entry_val(lmt_node_fields_disc, 5, integer_field,   options);
    set_value_entry_val(lmt_node_fields_disc, 6, integer_field,   class);

    lmt_node_fields_fence = lmt_aux_allocate_value_info(8);

    set_value_entry_val(lmt_node_fields_fence,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_fence,  1, node_list_field, delimiter);
    set_value_entry_val(lmt_node_fields_fence,  2, integer_field,   nestingfactor);
    set_value_entry_val(lmt_node_fields_fence,  3, integer_field,   top);
    set_value_entry_val(lmt_node_fields_fence,  4, integer_field,   bottom);
    set_value_entry_val(lmt_node_fields_fence,  5, dimension_field, topovershoot);
    set_value_entry_val(lmt_node_fields_fence,  6, dimension_field, bottomovershoot);
    set_value_entry_val(lmt_node_fields_fence,  7, integer_field,   variant);

    lmt_node_fields_fraction = lmt_aux_allocate_value_info(9);

    set_value_entry_val(lmt_node_fields_fraction, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_fraction, 1, node_list_field, numerator);
    set_value_entry_val(lmt_node_fields_fraction, 2, node_list_field, denominator);
    set_value_entry_val(lmt_node_fields_fraction, 3, node_list_field, left);
    set_value_entry_val(lmt_node_fields_fraction, 4, node_list_field, right);
    set_value_entry_val(lmt_node_fields_fraction, 5, node_list_field, middle);
    set_value_entry_val(lmt_node_fields_fraction, 6, dimension_field, rule);
    set_value_entry_val(lmt_node_fields_fraction, 7, integer_field,   hfactor);
    set_value_entry_val(lmt_node_fields_fraction, 8, integer_field,   vfactor);

    lmt_node_fields_glue = lmt_aux_allocate_value_info(11);

    set_value_entry_val(lmt_node_fields_glue,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_glue,  1, node_list_field, leader);
    set_value_entry_val(lmt_node_fields_glue,  2, dimension_field, width);
    set_value_entry_val(lmt_node_fields_glue,  3, dimension_field, stretch);
    set_value_entry_val(lmt_node_fields_glue,  4, dimension_field, shrink);
    set_value_entry_val(lmt_node_fields_glue,  5, integer_field,   stretchorder);
    set_value_entry_val(lmt_node_fields_glue,  6, integer_field,   shrinkorder);
    set_value_entry_val(lmt_node_fields_glue,  7, integer_field,   font);
    set_value_entry_val(lmt_node_fields_glue,  8, integer_field,   options);
    set_value_entry_val(lmt_node_fields_glue,  9, integer_field,   data);
    set_value_entry_val(lmt_node_fields_glue, 10, integer_field,   callback);

    lmt_node_fields_glue_spec = lmt_aux_allocate_value_info(5);

    set_value_entry_val(lmt_node_fields_glue_spec, 0, dimension_field, width);
    set_value_entry_val(lmt_node_fields_glue_spec, 1, dimension_field, stretch);
    set_value_entry_val(lmt_node_fields_glue_spec, 2, dimension_field, shrink);
    set_value_entry_val(lmt_node_fields_glue_spec, 3, integer_field,   stretchorder);
    set_value_entry_val(lmt_node_fields_glue_spec, 4, integer_field,   shrinkorder);
 /* set_value_entry_val(lmt_node_fields_glue_spec, 5, integer_field,   options); */

    lmt_node_fields_glyph = lmt_aux_allocate_value_info(34);

    set_value_entry_val(lmt_node_fields_glyph,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_glyph,  1, integer_field,   font);
    set_value_entry_val(lmt_node_fields_glyph,  2, integer_field,   char);
    set_value_entry_val(lmt_node_fields_glyph,  3, dimension_field, xoffset);
    set_value_entry_val(lmt_node_fields_glyph,  4, dimension_field, yoffset);
    set_value_entry_val(lmt_node_fields_glyph,  5, integer_field,   data);
    set_value_entry_val(lmt_node_fields_glyph,  6, dimension_field, width);
    set_value_entry_val(lmt_node_fields_glyph,  7, dimension_field, height);
    set_value_entry_val(lmt_node_fields_glyph,  8, dimension_field, depth);
    set_value_entry_val(lmt_node_fields_glyph,  9, dimension_field, total);
    set_value_entry_val(lmt_node_fields_glyph, 10, dimension_field, scale);
    set_value_entry_val(lmt_node_fields_glyph, 11, dimension_field, xscale);
    set_value_entry_val(lmt_node_fields_glyph, 12, dimension_field, yscale);
    set_value_entry_val(lmt_node_fields_glyph, 13, integer_field,   expansion);
    set_value_entry_val(lmt_node_fields_glyph, 14, integer_field,   state);
    set_value_entry_val(lmt_node_fields_glyph, 15, integer_field,   script);
    set_value_entry_val(lmt_node_fields_glyph, 16, integer_field,   language);
    set_value_entry_val(lmt_node_fields_glyph, 17, integer_field,   lhmin);
    set_value_entry_val(lmt_node_fields_glyph, 18, integer_field,   rhmin);
    set_value_entry_val(lmt_node_fields_glyph, 19, dimension_field, left);
    set_value_entry_val(lmt_node_fields_glyph, 20, dimension_field, right);
    set_value_entry_val(lmt_node_fields_glyph, 21, integer_field,   script);
    set_value_entry_val(lmt_node_fields_glyph, 22, integer_field,   hyphenate);
    set_value_entry_val(lmt_node_fields_glyph, 23, integer_field,   options);
    set_value_entry_val(lmt_node_fields_glyph, 24, integer_field,   discpart);
    set_value_entry_val(lmt_node_fields_glyph, 25, integer_field,   protected);
    set_value_entry_val(lmt_node_fields_glyph, 26, integer_field,   properties);
    set_value_entry_val(lmt_node_fields_glyph, 27, integer_field,   group);
    set_value_entry_val(lmt_node_fields_glyph, 28, integer_field,   index);
    set_value_entry_val(lmt_node_fields_glyph, 29, integer_field,   state);
    set_value_entry_val(lmt_node_fields_glyph, 30, integer_field,   control);
    set_value_entry_val(lmt_node_fields_glyph, 31, dimension_field, raise);
    set_value_entry_val(lmt_node_fields_glyph, 32, integer_field,   weight);
    set_value_entry_val(lmt_node_fields_glyph, 33, integer_field,   slant);

    lmt_node_fields_insert = lmt_aux_allocate_value_info(6);

    set_value_entry_val(lmt_node_fields_insert, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_insert, 1, integer_field,   cost);
    set_value_entry_val(lmt_node_fields_insert, 2, dimension_field, depth);
    set_value_entry_val(lmt_node_fields_insert, 3, dimension_field, height);
    set_value_entry_val(lmt_node_fields_insert, 4, integer_field,   index);
    set_value_entry_val(lmt_node_fields_insert, 5, node_list_field, list);

    lmt_node_fields_split = lmt_aux_allocate_value_info(6);

    set_value_entry_val(lmt_node_fields_split, 0, attribute_field, height);
    set_value_entry_val(lmt_node_fields_split, 1, integer_field,   index);
    set_value_entry_val(lmt_node_fields_split, 2, node_field,      lastinsert);
    set_value_entry_val(lmt_node_fields_split, 3, node_field,      bestinsert);
    set_value_entry_val(lmt_node_fields_split, 4, integer_field,   stretchorder);
    set_value_entry_val(lmt_node_fields_split, 5, integer_field,   shrinkorder);

    lmt_node_fields_kern = lmt_aux_allocate_value_info(3);

    set_value_entry_val(lmt_node_fields_kern, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_kern, 1, dimension_field, kern);
    set_value_entry_val(lmt_node_fields_kern, 2, integer_field,   expansion);

    lmt_node_fields_list = lmt_aux_allocate_value_info(30);

    set_value_entry_val(lmt_node_fields_list,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_list,  1, node_list_field, list);
    set_value_entry_val(lmt_node_fields_list,  2, dimension_field, width);
    set_value_entry_val(lmt_node_fields_list,  3, dimension_field, depth);
    set_value_entry_val(lmt_node_fields_list,  4, dimension_field, height);
    set_value_entry_val(lmt_node_fields_list,  5, integer_field,   direction);
    set_value_entry_val(lmt_node_fields_list,  6, dimension_field, shift);
    set_value_entry_val(lmt_node_fields_list,  7, integer_field,   glueorder);
    set_value_entry_val(lmt_node_fields_list,  8, integer_field,   gluesign);
    set_value_entry_val(lmt_node_fields_list,  9, integer_field,   glueset);
    set_value_entry_val(lmt_node_fields_list, 10, integer_field,   geometry);
    set_value_entry_val(lmt_node_fields_list, 11, integer_field,   orientation);
    set_value_entry_val(lmt_node_fields_list, 12, integer_field,   anchor);
    set_value_entry_val(lmt_node_fields_list, 13, integer_field,   source);
    set_value_entry_val(lmt_node_fields_list, 14, integer_field,   target);
    set_value_entry_val(lmt_node_fields_list, 15, dimension_field, xoffset);
    set_value_entry_val(lmt_node_fields_list, 16, dimension_field, yoffset);
    set_value_entry_val(lmt_node_fields_list, 17, dimension_field, woffset);
    set_value_entry_val(lmt_node_fields_list, 18, dimension_field, hoffset);
    set_value_entry_val(lmt_node_fields_list, 19, dimension_field, doffset);
    set_value_entry_val(lmt_node_fields_list, 20, node_list_field, pre);     /* migrated */
    set_value_entry_val(lmt_node_fields_list, 21, node_list_field, post);    /* migrated */
    set_value_entry_val(lmt_node_fields_list, 22, integer_field,   state);
    set_value_entry_val(lmt_node_fields_list, 23, integer_field,   index);
    set_value_entry_val(lmt_node_fields_list, 24, node_list_field, preadjust);
    set_value_entry_val(lmt_node_fields_list, 25, node_list_field, postadjust);
    set_value_entry_val(lmt_node_fields_list, 26, integer_field,   axis);
    set_value_entry_val(lmt_node_fields_list, 27, node_list_field, except);
    set_value_entry_val(lmt_node_fields_list, 28, integer_field,   exdepth);
    set_value_entry_val(lmt_node_fields_list, 29, integer_field,   total);

    lmt_node_fields_par = lmt_aux_allocate_value_info(65);

    set_value_entry_val(lmt_node_fields_par,  0, attribute_field,  attr);
    set_value_entry_val(lmt_node_fields_par,  1, integer_field,    dir);
    set_value_entry_val(lmt_node_fields_par,  2, node_field,       leftbox);
    set_value_entry_val(lmt_node_fields_par,  3, dimension_field,  leftboxwidth);
    set_value_entry_val(lmt_node_fields_par,  4, node_field,       rightbox);
    set_value_entry_val(lmt_node_fields_par,  5, dimension_field,  rightboxwidth);
    set_value_entry_val(lmt_node_fields_par,  6, node_field,       middlebox);
    set_value_entry_val(lmt_node_fields_par,  7, node_field,       parpasses);
    set_value_entry_val(lmt_node_fields_par,  8, integer_field,    linebreakchecks);
    set_value_entry_val(lmt_node_fields_par,  9, integer_field,    state);
    set_value_entry_val(lmt_node_fields_par, 10, integer_field,    prevgraf);
    set_value_entry_val(lmt_node_fields_par, 11, dimension_field,  hsize);
    set_value_entry_val(lmt_node_fields_par, 12, integer_field,    leftskip);
    set_value_entry_val(lmt_node_fields_par, 13, integer_field,    rightskip);
    set_value_entry_val(lmt_node_fields_par, 14, dimension_field,  hangindent);
    set_value_entry_val(lmt_node_fields_par, 15, integer_field,    hangafter);
    set_value_entry_val(lmt_node_fields_par, 16, dimension_field,  parindent);
    set_value_entry_val(lmt_node_fields_par, 17, glue_field,       parfillleftskip);
    set_value_entry_val(lmt_node_fields_par, 18, glue_field,       parfillrightskip);
    set_value_entry_val(lmt_node_fields_par, 19, integer_field,    adjustspacing);
    set_value_entry_val(lmt_node_fields_par, 20, integer_field,    protrudechars);
    set_value_entry_val(lmt_node_fields_par, 21, integer_field,    pretolerance);
    set_value_entry_val(lmt_node_fields_par, 22, integer_field,    tolerance);
    set_value_entry_val(lmt_node_fields_par, 23, dimension_field,  emergencystretch);
    set_value_entry_val(lmt_node_fields_par, 24, integer_field,    looseness);
    set_value_entry_val(lmt_node_fields_par, 25, integer_field,    lastlinefit);
    set_value_entry_val(lmt_node_fields_par, 26, integer_field,    linepenalty);
    set_value_entry_val(lmt_node_fields_par, 27, integer_field,    interlinepenalty);
    set_value_entry_val(lmt_node_fields_par, 28, integer_field,    clubpenalty);
    set_value_entry_val(lmt_node_fields_par, 29, integer_field,    widowpenalty);
    set_value_entry_val(lmt_node_fields_par, 30, integer_field,    displaywidowpenalty);
    set_value_entry_val(lmt_node_fields_par, 31, node_field,       toddlerpenalties);
    set_value_entry_val(lmt_node_fields_par, 32, integer_field,    brokenpenalty);
    set_value_entry_val(lmt_node_fields_par, 33, integer_field,    adjdemerits);
    set_value_entry_val(lmt_node_fields_par, 34, integer_field,    doublehyphendemerits);
    set_value_entry_val(lmt_node_fields_par, 35, integer_field,    finalhyphendemerits);
    set_value_entry_val(lmt_node_fields_par, 36, node_field,       parshape);
    set_value_entry_val(lmt_node_fields_par, 37, node_field,       interlinepenalties);
    set_value_entry_val(lmt_node_fields_par, 38, node_field,       clubpenalties);
    set_value_entry_val(lmt_node_fields_par, 39, node_field,       widowpenalties);
    set_value_entry_val(lmt_node_fields_par, 40, node_field,       displaywidowpenalties);
    set_value_entry_val(lmt_node_fields_par, 41, node_field,       brokenpenalties);
    set_value_entry_val(lmt_node_fields_par, 42, node_field,       orphanpenalties);
    set_value_entry_val(lmt_node_fields_par, 43, integer_field,    singlelinepenalty);
    set_value_entry_val(lmt_node_fields_par, 44, glue_field,       baselineskip);
    set_value_entry_val(lmt_node_fields_par, 45, glue_field,       lineskip);
    set_value_entry_val(lmt_node_fields_par, 46, dimension_field,  lineskiplimit);
    set_value_entry_val(lmt_node_fields_par, 47, integer_field,    adjustspacingstep);
    set_value_entry_val(lmt_node_fields_par, 48, integer_field,    adjustspacingshrink);
    set_value_entry_val(lmt_node_fields_par, 49, integer_field,    adjustspacingstretch);
    set_value_entry_val(lmt_node_fields_par, 50, token_list_field, endpartokens);
    set_value_entry_val(lmt_node_fields_par, 51, integer_field,    hyphenationmode);
    set_value_entry_val(lmt_node_fields_par, 52, integer_field,    shapingpenaltiesmode);
    set_value_entry_val(lmt_node_fields_par, 53, integer_field,    shapingpenalty);
    set_value_entry_val(lmt_node_fields_par, 54, glue_field,       parinitleftskip);
    set_value_entry_val(lmt_node_fields_par, 55, glue_field,       parinitrightskip);
    set_value_entry_val(lmt_node_fields_par, 56, glue_field,       emergencyleftskip);
    set_value_entry_val(lmt_node_fields_par, 57, glue_field,       emergencyrightskip);
    set_value_entry_val(lmt_node_fields_par, 58, dimension_field,  emergencyextrastretch);
    set_value_entry_val(lmt_node_fields_par, 59, node_field,       fitnessclasses);
    set_value_entry_val(lmt_node_fields_par, 60, node_field,       adjacentdemerits);
    set_value_entry_val(lmt_node_fields_par, 61, integer_field,    hyphenpenalty);
    set_value_entry_val(lmt_node_fields_par, 62, integer_field,    exhyphenpenalty);
    set_value_entry_val(lmt_node_fields_par, 63, integer_field,    lefttwindemerits);
    set_value_entry_val(lmt_node_fields_par, 64, integer_field,    righttwindemerits);

    lmt_node_fields_mark = lmt_aux_allocate_value_info(3);

    set_value_entry_val(lmt_node_fields_mark, 0, attribute_field,  attr);
    set_value_entry_val(lmt_node_fields_mark, 1, integer_field,    class);
    set_value_entry_val(lmt_node_fields_mark, 2, token_list_field, mark);

    lmt_node_fields_math = lmt_aux_allocate_value_info(11);

    set_value_entry_val(lmt_node_fields_math,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_math,  1, integer_field,   surround);
    set_value_entry_val(lmt_node_fields_math,  2, dimension_field, width);
    set_value_entry_val(lmt_node_fields_math,  3, dimension_field, stretch);
    set_value_entry_val(lmt_node_fields_math,  4, dimension_field, shrink);
    set_value_entry_val(lmt_node_fields_math,  5, integer_field,   stretchorder);
    set_value_entry_val(lmt_node_fields_math,  6, integer_field,   shrinkorder);
    set_value_entry_val(lmt_node_fields_math,  7, integer_field,   penalty);
    set_value_entry_val(lmt_node_fields_math,  8, integer_field,   options);
    set_value_entry_val(lmt_node_fields_math,  9, integer_field,   tolerance);
    set_value_entry_val(lmt_node_fields_math, 10, integer_field,   pretolerance);

    lmt_node_fields_math_kernel = lmt_aux_allocate_value_info(7);

    set_value_entry_val(lmt_node_fields_math_kernel, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_math_kernel, 1, integer_field,   fam);
    set_value_entry_val(lmt_node_fields_math_kernel, 2, integer_field,   char);
    set_value_entry_val(lmt_node_fields_math_kernel, 3, integer_field,   options);
    set_value_entry_val(lmt_node_fields_math_kernel, 4, integer_field,   properties);
    set_value_entry_val(lmt_node_fields_math_kernel, 5, integer_field,   group);
    set_value_entry_val(lmt_node_fields_math_kernel, 6, integer_field,   index);

    lmt_node_fields_noad = lmt_aux_allocate_value_info(29);

    set_value_entry_val(lmt_node_fields_noad,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_noad,  1, node_list_field, hlist)
    set_value_entry_val(lmt_node_fields_noad,  2, node_list_field, nucleus)
    set_value_entry_val(lmt_node_fields_noad,  3, node_list_field, sup)
    set_value_entry_val(lmt_node_fields_noad,  4, node_list_field, sub)
    set_value_entry_val(lmt_node_fields_noad,  5, node_list_field, suppre)
    set_value_entry_val(lmt_node_fields_noad,  6, node_list_field, subpre)
    set_value_entry_val(lmt_node_fields_noad,  7, integer_field,   italic)
    set_value_entry_val(lmt_node_fields_noad,  8, integer_field,   width)
    set_value_entry_val(lmt_node_fields_noad,  9, integer_field,   height)
    set_value_entry_val(lmt_node_fields_noad, 10, integer_field,   depth)
    set_value_entry_val(lmt_node_fields_noad, 11, integer_field,   options)
    set_value_entry_val(lmt_node_fields_noad, 12, integer_field,   style)
    set_value_entry_val(lmt_node_fields_noad, 13, integer_field,   fam)
    set_value_entry_val(lmt_node_fields_noad, 14, integer_field,   scriptstate)
    set_value_entry_val(lmt_node_fields_noad, 15, integer_field,   analyzed)
    set_value_entry_val(lmt_node_fields_noad, 16, integer_field,   mainclass)
    set_value_entry_val(lmt_node_fields_noad, 17, integer_field,   leftclass)
    set_value_entry_val(lmt_node_fields_noad, 18, integer_field,   rightclass)
    set_value_entry_val(lmt_node_fields_noad, 19, integer_field,   scriptorder)
    set_value_entry_val(lmt_node_fields_noad, 20, integer_field,   source)
    set_value_entry_val(lmt_node_fields_noad, 21, node_list_field, prime)
    set_value_entry_val(lmt_node_fields_noad, 22, integer_field,   leftslack)
    set_value_entry_val(lmt_node_fields_noad, 23, integer_field,   rightslack)
    set_value_entry_val(lmt_node_fields_noad, 24, integer_field,   subshift)
    set_value_entry_val(lmt_node_fields_noad, 25, integer_field,   supshift)
    set_value_entry_val(lmt_node_fields_noad, 26, integer_field,   primeshift)
    set_value_entry_val(lmt_node_fields_noad, 27, integer_field,   scriptkern)
    set_value_entry_val(lmt_node_fields_noad, 28, attribute_field, extraattr)

    lmt_node_fields_penalty = lmt_aux_allocate_value_info(4);

    set_value_entry_val(lmt_node_fields_penalty, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_penalty, 1, integer_field,   penalty);
    set_value_entry_val(lmt_node_fields_penalty, 2, integer_field,   nepalty);
    set_value_entry_val(lmt_node_fields_penalty, 3, integer_field,   options);

    lmt_node_fields_radical = lmt_aux_allocate_value_info(9);

    set_value_entry_val(lmt_node_fields_radical, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_radical, 1, node_list_field, degree);
    set_value_entry_val(lmt_node_fields_radical, 2, node_list_field, left);
    set_value_entry_val(lmt_node_fields_radical, 3, node_list_field, right);
    set_value_entry_val(lmt_node_fields_radical, 4, integer_field,   size);
    set_value_entry_val(lmt_node_fields_radical, 5, dimension_field, height);
    set_value_entry_val(lmt_node_fields_radical, 6, dimension_field, depth);
    set_value_entry_val(lmt_node_fields_radical, 7, node_list_field, top);
    set_value_entry_val(lmt_node_fields_radical, 8, node_list_field, bottom);

    lmt_node_fields_rule = lmt_aux_allocate_value_info(16);

    set_value_entry_val(lmt_node_fields_rule,  0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_rule,  1, dimension_field, width);
    set_value_entry_val(lmt_node_fields_rule,  2, dimension_field, depth);
    set_value_entry_val(lmt_node_fields_rule,  3, dimension_field, height);
    set_value_entry_val(lmt_node_fields_rule,  3, dimension_field, total);
    set_value_entry_val(lmt_node_fields_rule,  4, dimension_field, xoffset);
    set_value_entry_val(lmt_node_fields_rule,  5, dimension_field, yoffset);
    set_value_entry_val(lmt_node_fields_rule,  6, dimension_field, left);
    set_value_entry_val(lmt_node_fields_rule,  7, dimension_field, right);
    set_value_entry_val(lmt_node_fields_rule,  8, integer_field,   data);
    set_value_entry_val(lmt_node_fields_rule,  9, integer_field,   font);
    set_value_entry_val(lmt_node_fields_rule, 10, integer_field,   fam);
    set_value_entry_val(lmt_node_fields_rule, 11, integer_field,   char);
    set_value_entry_val(lmt_node_fields_rule, 12, integer_field,   options);
    set_value_entry_val(lmt_node_fields_rule, 13, integer_field,   on);
    set_value_entry_val(lmt_node_fields_rule, 14, integer_field,   off);
    set_value_entry_val(lmt_node_fields_rule, 15, integer_field,   thickness);

    lmt_node_fields_style = lmt_aux_allocate_value_info(3);

    set_value_entry_val(lmt_node_fields_style, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_style, 1, integer_field,   style);
    set_value_entry_val(lmt_node_fields_style, 2, integer_field,   scale);

    lmt_node_fields_parameter = lmt_aux_allocate_value_info(3);

    set_value_entry_val(lmt_node_fields_parameter, 0, integer_field,   style);
    set_value_entry_val(lmt_node_fields_parameter, 1, integer_field,   name);
    set_value_entry_val(lmt_node_fields_parameter, 2, integer_field,   value);

    lmt_node_fields_math_sub = lmt_aux_allocate_value_info(2);

    set_value_entry_val(lmt_node_fields_math_sub, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_math_sub, 1, node_list_field, list);

    lmt_node_fields_unset = lmt_aux_allocate_value_info(5);

    set_value_entry_val(lmt_node_fields_unset, 0, attribute_field, attr);
    set_value_entry_val(lmt_node_fields_unset, 1, dimension_field, shrink);
    set_value_entry_val(lmt_node_fields_unset, 2, dimension_field, stretch);
    set_value_entry_val(lmt_node_fields_unset, 3, integer_field,   count);
    set_value_entry_val(lmt_node_fields_unset, 4, integer_field,   span);

    lmt_node_fields_align_record = lmt_aux_allocate_value_info(3);

    set_value_entry_val(lmt_node_fields_align_record, 0, node_field,      list);
    set_value_entry_val(lmt_node_fields_align_record, 1, dimension_field, width);
    set_value_entry_val(lmt_node_fields_align_record, 2, dimension_field, size);

    lmt_node_fields_whatsit = lmt_aux_allocate_value_info(1);

    set_value_entry_val(lmt_node_fields_whatsit, 0, attribute_field, attr);

    lmt_interface.node_data = lmt_memory_malloc((passive_node + 2) * sizeof(node_info));

    /*tex
        We start with the nodes that users can encounter. The order is mostly the one that \TEX\
        uses but we have move some around because we have some more and sometimes a bit different
        kind of nodes. You should use abstractions anyway, so numbers mean nothing. In original
        \TEX\ there are sometimes tests like |if (foo < kern_node)| but these have been replaces
        by switches and (un)equality tests so that the order is not really important.

        Subtypes in nodes and codes in commands sometimes are sort of in sync but don't rely on
        that!
    */

    lmt_interface.node_data[hlist_node]          = (node_info) { .id = hlist_node,          .size = box_node_size,            .first = 0, .last = last_list_subtype,          .subtypes = subtypes_list,     .fields = lmt_node_fields_list,           .name = lua_key(hlist),          .lua = lua_key_index(hlist),           .visible = 1 };
    lmt_interface.node_data[vlist_node]          = (node_info) { .id = vlist_node,          .size = box_node_size,            .first = 0, .last = last_list_subtype,          .subtypes = subtypes_list,     .fields = lmt_node_fields_list,           .name = lua_key(vlist),          .lua = lua_key_index(vlist),           .visible = 1 };
    lmt_interface.node_data[rule_node]           = (node_info) { .id = rule_node,           .size = rule_node_size,           .first = 0, .last = last_rule_subtype,          .subtypes = subtypes_rule,     .fields = lmt_node_fields_rule,           .name = lua_key(rule),           .lua = lua_key_index(rule),            .visible = 1 };
    lmt_interface.node_data[insert_node]         = (node_info) { .id = insert_node,         .size = insert_node_size,         .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_insert,         .name = lua_key(insert),         .lua = lua_key_index(insert),          .visible = 1 };
    lmt_interface.node_data[mark_node]           = (node_info) { .id = mark_node,           .size = mark_node_size,           .first = 0, .last = last_mark_subtype,          .subtypes = subtypes_mark,     .fields = lmt_node_fields_mark,           .name = lua_key(mark),           .lua = lua_key_index(mark),            .visible = 1 };
    lmt_interface.node_data[adjust_node]         = (node_info) { .id = adjust_node,         .size = adjust_node_size,         .first = 0, .last = last_adjust_subtype,        .subtypes = subtypes_adjust,   .fields = lmt_node_fields_adjust,         .name = lua_key(adjust),         .lua = lua_key_index(adjust),          .visible = 1 };
    lmt_interface.node_data[boundary_node]       = (node_info) { .id = boundary_node,       .size = boundary_node_size,       .first = 0, .last = last_boundary_subtype,      .subtypes = subtypes_boundary, .fields = lmt_node_fields_boundary,       .name = lua_key(boundary),       .lua = lua_key_index(boundary),        .visible = 1 };
    lmt_interface.node_data[disc_node]           = (node_info) { .id = disc_node,           .size = disc_node_size,           .first = 0, .last = last_discretionary_subtype, .subtypes = subtypes_disc,     .fields = lmt_node_fields_disc,           .name = lua_key(disc),           .lua = lua_key_index(disc),            .visible = 1 };
    lmt_interface.node_data[whatsit_node]        = (node_info) { .id = whatsit_node,        .size = whatsit_node_size,        .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_whatsit,        .name = lua_key(whatsit),        .lua = lua_key_index(whatsit),         .visible = 1 };
    lmt_interface.node_data[par_node]            = (node_info) { .id = par_node,            .size = par_node_size,            .first = 0, .last = last_par_subtype,           .subtypes = subtypes_par,      .fields = lmt_node_fields_par,            .name = lua_key(par),            .lua = lua_key_index(par),             .visible = 1 };
    lmt_interface.node_data[dir_node]            = (node_info) { .id = dir_node,            .size = dir_node_size,            .first = 0, .last = last_dir_subtype,           .subtypes = subtypes_dir,      .fields = lmt_node_fields_dir,            .name = lua_key(dir),            .lua = lua_key_index(dir),             .visible = 1 };
    lmt_interface.node_data[math_node]           = (node_info) { .id = math_node,           .size = math_node_size,           .first = 0, .last = last_math_subtype,          .subtypes = subtypes_math,     .fields = lmt_node_fields_math,           .name = lua_key(math),           .lua = lua_key_index(math),            .visible = 1 };
    lmt_interface.node_data[glue_node]           = (node_info) { .id = glue_node,           .size = glue_node_size,           .first = 0, .last = last_glue_subtype,          .subtypes = subtypes_glue,     .fields = lmt_node_fields_glue,           .name = lua_key(glue),           .lua = lua_key_index(glue),            .visible = 1 };
    lmt_interface.node_data[kern_node]           = (node_info) { .id = kern_node,           .size = kern_node_size,           .first = 0, .last = last_kern_subtype,          .subtypes = subtypes_kern,     .fields = lmt_node_fields_kern,           .name = lua_key(kern),           .lua = lua_key_index(kern),            .visible = 1 };
    lmt_interface.node_data[penalty_node]        = (node_info) { .id = penalty_node,        .size = penalty_node_size,        .first = 0, .last = last_penalty_subtype,       .subtypes = subtypes_penalty,  .fields = lmt_node_fields_penalty,        .name = lua_key(penalty),        .lua = lua_key_index(penalty),         .visible = 1 };
    lmt_interface.node_data[style_node]          = (node_info) { .id = style_node,          .size = style_node_size,          .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_style,          .name = lua_key(style),          .lua = lua_key_index(style),           .visible = 1 };
    lmt_interface.node_data[choice_node]         = (node_info) { .id = choice_node,         .size = choice_node_size,         .first = 0, .last = last_choice_subtype,        .subtypes = subtypes_choice,   .fields = lmt_node_fields_choice,         .name = lua_key(choice),         .lua = lua_key_index(choice),          .visible = 1 };
    lmt_interface.node_data[parameter_node]      = (node_info) { .id = parameter_node,      .size = parameter_node_size,      .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_parameter,      .name = lua_key(parameter),      .lua = lua_key_index(parameter),       .visible = 1 };
    lmt_interface.node_data[simple_noad]         = (node_info) { .id = simple_noad,         .size = noad_size,                .first = 0, .last = last_noad_subtype,          .subtypes = subtypes_noad,     .fields = lmt_node_fields_noad,           .name = lua_key(noad),           .lua = lua_key_index(noad),            .visible = 1 };
    lmt_interface.node_data[radical_noad]        = (node_info) { .id = radical_noad,        .size = radical_noad_size,        .first = 0, .last = last_radical_subtype,       .subtypes = subtypes_radical,  .fields = lmt_node_fields_radical,        .name = lua_key(radical),        .lua = lua_key_index(radical),         .visible = 1 };
    lmt_interface.node_data[fraction_noad]       = (node_info) { .id = fraction_noad,       .size = fraction_noad_size,       .first = 0, .last = last_fraction_subtype,      .subtypes = subtypes_fraction, .fields = lmt_node_fields_fraction,       .name = lua_key(fraction),       .lua = lua_key_index(fraction),        .visible = 1 };
    lmt_interface.node_data[accent_noad]         = (node_info) { .id = accent_noad,         .size = accent_noad_size,         .first = 0, .last = last_accent_subtype,        .subtypes = subtypes_accent,   .fields = lmt_node_fields_accent,         .name = lua_key(accent),         .lua = lua_key_index(accent),          .visible = 1 };
    lmt_interface.node_data[fence_noad]          = (node_info) { .id = fence_noad,          .size = fence_noad_size,          .first = 0, .last = last_fence_subtype,         .subtypes = subtypes_fence,    .fields = lmt_node_fields_fence,          .name = lua_key(fence),          .lua = lua_key_index(fence),           .visible = 1 };
    lmt_interface.node_data[math_char_node]      = (node_info) { .id = math_char_node,      .size = math_kernel_node_size,    .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_math_kernel,    .name = lua_key(mathchar),       .lua = lua_key_index(mathchar),        .visible = 1 };
    lmt_interface.node_data[math_text_char_node] = (node_info) { .id = math_text_char_node, .size = math_kernel_node_size,    .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_math_kernel,    .name = lua_key(mathtextchar),   .lua = lua_key_index(mathtextchar),    .visible = 1 };
    lmt_interface.node_data[sub_box_node]        = (node_info) { .id = sub_box_node,        .size = math_kernel_node_size,    .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_math_sub,        .name = lua_key(subbox),         .lua = lua_key_index(subbox),          .visible = 1 };
    lmt_interface.node_data[sub_mlist_node]      = (node_info) { .id = sub_mlist_node,      .size = math_kernel_node_size,    .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_math_sub,      .name = lua_key(submlist),       .lua = lua_key_index(submlist),        .visible = 1 };
    lmt_interface.node_data[delimiter_node]      = (node_info) { .id = delimiter_node,      .size = math_delimiter_node_size, .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_delimiter,      .name = lua_key(delimiter),      .lua = lua_key_index(delimiter),       .visible = 1 };
    lmt_interface.node_data[glyph_node]          = (node_info) { .id = glyph_node,          .size = glyph_node_size,          .first = 0, .last = last_glyph_subtype,         .subtypes = subtypes_glyph,    .fields = lmt_node_fields_glyph,          .name = lua_key(glyph),          .lua = lua_key_index(glyph),           .visible = 1 };

    /*tex
        Who knows when someone needs is, so for now we keep it exposed.
    */

    lmt_interface.node_data[unset_node]          = (node_info) { .id = unset_node,          .size = box_node_size,            .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_unset,          .name = lua_key(unset),          .lua = lua_key_index(unset),           .visible = 1 };
    lmt_interface.node_data[specification_node]  = (node_info) { .id = specification_node,  .size = specification_node_size,  .first = 0, .last = 0,                          .subtypes = NULL,              .fields = NULL,                           .name = lua_key(specification),  .lua = lua_key_index(specification),   .visible = 0 };
    lmt_interface.node_data[align_record_node]   = (node_info) { .id = align_record_node,   .size = box_node_size,            .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_align_record,   .name = lua_key(alignrecord),    .lua = lua_key_index(alignrecord),     .visible = 1 };

    /*tex
        These nodes never show up in nodelists and are managed special. Messing with such nodes
        directly is not a good idea.
    */

    lmt_interface.node_data[attribute_node]      = (node_info) { .id = attribute_node,      .size = attribute_node_size,      .first = 0, .last = last_attribute_subtype,     .subtypes = subtypes_attribute,.fields = lmt_node_fields_attribute,      .name = lua_key(attribute),      .lua = lua_key_index(attribute),       .visible = 1 };

    /*
        We still expose the glue spec as they are the containers for skip registers but there is no
        real need to use them at the user end.
    */

    lmt_interface.node_data[glue_spec_node]      = (node_info) { .id = glue_spec_node,      .size = glue_spec_size,           .first = 0, .last = 0,                          .subtypes = NULL,              .fields = lmt_node_fields_glue_spec,      .name = lua_key(gluespec),       .lua = lua_key_index(gluespec),        .visible = 1 };

    /*tex
        This one sometimes shows up, especially when we temporarily need an alternative head pointer,
        simply because we want to retain some head in case the original head is replaced.
    */

    lmt_interface.node_data[temp_node]           = (node_info) { .id = temp_node,           .size = temp_node_size,           .first = 0, .last = 0,                          .subtypes = NULL,              .fields = NULL,                           .name = lua_key(temp),           .lua = lua_key_index(temp),            .visible = 1 };

    /*tex
        The split nodes are used for insertions.
    */

    lmt_interface.node_data[split_node]          = (node_info) { .id = split_node,          .size = split_node_size,          .first = 0, .last = last_split_subtype,         .subtypes = subtypes_split,    .fields = lmt_node_fields_split,          .name = lua_key(split),          .lua = lua_key_index(split),           .visible = 1 };

    /*tex
        The following nodes are not meant for users. They are used internally for different purposes
        and you should not encounter them in node lists. As with many nodes, they often are
        allocated using fast methods so they never show up in the new, copy and flush handlers.
    */

    lmt_interface.node_data[expression_node]     = (node_info) { .id = expression_node,     .size = expression_node_size,     .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(expression),     .lua = lua_key_index(expression),      .visible = 0 };
    lmt_interface.node_data[lmtx_expression_node]= (node_info) { .id = lmtx_expression_node,.size = lmtx_expression_node_size,.first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(lmtxexpression), .lua = lua_key_index(lmtxexpression),  .visible = 0 };
    lmt_interface.node_data[rpn_expression_node] = (node_info) { .id = rpn_expression_node, .size = rpn_expression_node_size, .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(rpnexpression),  .lua = lua_key_index(rpnexpression),   .visible = 0 };
    lmt_interface.node_data[loop_state_node]     = (node_info) { .id = loop_state_node,     .size = loop_state_node_size,     .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(loopstate),      .lua = lua_key_index(loopstate),       .visible = 0 };
    lmt_interface.node_data[math_spec_node]      = (node_info) { .id = math_spec_node,      .size = math_spec_node_size,      .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(mathspec),       .lua = lua_key_index(mathspec),        .visible = 0 };
    lmt_interface.node_data[font_spec_node]      = (node_info) { .id = font_spec_node,      .size = font_spec_node_size,      .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(fontspec),       .lua = lua_key_index(fontspec),        .visible = 0 };
    lmt_interface.node_data[nesting_node]        = (node_info) { .id = nesting_node,        .size = nesting_node_size,        .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(nestedlist),     .lua = lua_key_index(nestedlist),      .visible = 0 };
    lmt_interface.node_data[span_node]           = (node_info) { .id = span_node,           .size = span_node_size,           .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(span),           .lua = lua_key_index(span),            .visible = 0 };
    lmt_interface.node_data[align_stack_node]    = (node_info) { .id = align_stack_node,    .size = align_stack_node_size,    .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(alignstack),     .lua = lua_key_index(alignstack),      .visible = 0 };
 // lmt_interface.node_data[noad_state_node]     = (node_info) { .id = noad_state_node,     .size = noad_state_node_size,     .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(noadstate),      .lua = lua_key_index(noadstate),       .visible = 0 };
    lmt_interface.node_data[if_node]             = (node_info) { .id = if_node,             .size = if_node_size,             .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(ifstack),        .lua = lua_key_index(ifstack),         .visible = 0 };
    lmt_interface.node_data[unhyphenated_node]   = (node_info) { .id = unhyphenated_node,   .size = active_node_size,         .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(unhyphenated),   .lua = lua_key_index(unhyphenated),    .visible = 0 };
    lmt_interface.node_data[hyphenated_node]     = (node_info) { .id = hyphenated_node,     .size = active_node_size,         .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(hyphenated),     .lua = lua_key_index(hyphenated),      .visible = 0 };
    lmt_interface.node_data[delta_node]          = (node_info) { .id = delta_node,          .size = delta_node_size,          .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(delta),          .lua = lua_key_index(delta),           .visible = 0 };
    lmt_interface.node_data[passive_node]        = (node_info) { .id = passive_node,        .size = passive_node_size,        .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = lua_key(passive),        .lua = lua_key_index(passive),         .visible = 0 };
    lmt_interface.node_data[passive_node + 1]    = (node_info) { .id = -1,                  .size = -1,                       .first = 0, .last = 0,                         .subtypes = NULL,              .fields = NULL,                           .name = NULL,                    .lua = 0,                              .visible = 0 };

    lmt_interface.par_data = lmt_memory_malloc(par_n_of_codes * sizeof(par_info));

    lmt_interface.par_data[par_none_code                   ] = (par_info) { .cmd = 0,                      .chr = 0,                            .category = par_none_category                };
    lmt_interface.par_data[par_hsize_code                  ] = (par_info) { .cmd = internal_dimension_cmd, .chr = hsize_code,                   .category = par_hsize_category               };
    lmt_interface.par_data[par_left_skip_code              ] = (par_info) { .cmd = internal_glue_cmd,      .chr = left_skip_code,               .category = par_skip_category                };
    lmt_interface.par_data[par_right_skip_code             ] = (par_info) { .cmd = internal_glue_cmd,      .chr = right_skip_code,              .category = par_skip_category                };
    lmt_interface.par_data[par_hang_indent_code            ] = (par_info) { .cmd = internal_dimension_cmd, .chr = hang_indent_code,             .category = par_hang_category                };
    lmt_interface.par_data[par_hang_after_code             ] = (par_info) { .cmd = internal_integer_cmd,   .chr = hang_after_code,              .category = par_hang_category                };
    lmt_interface.par_data[par_par_indent_code             ] = (par_info) { .cmd = internal_dimension_cmd, .chr = par_indent_code,              .category = par_indent_category              };
    lmt_interface.par_data[par_par_fill_left_skip_code     ] = (par_info) { .cmd = internal_glue_cmd,      .chr = par_fill_left_skip_code,      .category = par_par_fill_category            };
    lmt_interface.par_data[par_par_fill_right_skip_code    ] = (par_info) { .cmd = internal_glue_cmd,      .chr = par_fill_right_skip_code,     .category = par_par_fill_category            };
    lmt_interface.par_data[par_par_init_left_skip_code     ] = (par_info) { .cmd = internal_glue_cmd,      .chr = par_init_left_skip_code,      .category = par_par_fill_category            };
    lmt_interface.par_data[par_par_init_right_skip_code    ] = (par_info) { .cmd = internal_glue_cmd,      .chr = par_init_right_skip_code,     .category = par_par_fill_category            };
    lmt_interface.par_data[par_emergency_left_skip_code    ] = (par_info) { .cmd = internal_glue_cmd,      .chr = emergency_left_skip_code,     .category = par_emergency_category           };
    lmt_interface.par_data[par_emergency_right_skip_code   ] = (par_info) { .cmd = internal_glue_cmd,      .chr = emergency_right_skip_code,    .category = par_emergency_category           };
    lmt_interface.par_data[par_adjust_spacing_code         ] = (par_info) { .cmd = internal_integer_cmd,   .chr = adjust_spacing_code,          .category = par_adjust_category              };
    lmt_interface.par_data[par_protrude_chars_code         ] = (par_info) { .cmd = internal_integer_cmd,   .chr = protrude_chars_code,          .category = par_protrude_category            };
    lmt_interface.par_data[par_pre_tolerance_code          ] = (par_info) { .cmd = internal_integer_cmd,   .chr = pre_tolerance_code,           .category = par_tolerance_category           };
    lmt_interface.par_data[par_tolerance_code              ] = (par_info) { .cmd = internal_integer_cmd,   .chr = tolerance_code,               .category = par_tolerance_category           };
    lmt_interface.par_data[par_emergency_stretch_code      ] = (par_info) { .cmd = internal_dimension_cmd, .chr = emergency_stretch_code,       .category = par_stretch_category             };
    lmt_interface.par_data[par_looseness_code              ] = (par_info) { .cmd = internal_integer_cmd,   .chr = looseness_code,               .category = par_looseness_category           };
    lmt_interface.par_data[par_last_line_fit_code          ] = (par_info) { .cmd = internal_integer_cmd,   .chr = last_line_fit_code,           .category = par_last_line_category           };
    lmt_interface.par_data[par_line_penalty_code           ] = (par_info) { .cmd = internal_integer_cmd,   .chr = line_penalty_code,            .category = par_line_penalty_category        };
    lmt_interface.par_data[par_inter_line_penalty_code     ] = (par_info) { .cmd = internal_integer_cmd,   .chr = inter_line_penalty_code,      .category = par_line_penalty_category        };
    lmt_interface.par_data[par_club_penalty_code           ] = (par_info) { .cmd = internal_integer_cmd,   .chr = club_penalty_code,            .category = par_club_penalty_category        };
    lmt_interface.par_data[par_widow_penalty_code          ] = (par_info) { .cmd = internal_integer_cmd,   .chr = widow_penalty_code,           .category = par_widow_penalty_category       };
    lmt_interface.par_data[par_display_widow_penalty_code  ] = (par_info) { .cmd = internal_integer_cmd,   .chr = display_widow_penalty_code,   .category = par_display_penalty_category     };
    lmt_interface.par_data[par_left_twin_demerits_code     ] = (par_info) { .cmd = internal_integer_cmd,   .chr = left_twin_demerits_code,      .category = par_twin_demerits_category       };
    lmt_interface.par_data[par_right_twin_demerits_code    ] = (par_info) { .cmd = internal_integer_cmd,   .chr = right_twin_demerits_code,     .category = par_twin_demerits_category       };
    lmt_interface.par_data[par_broken_penalty_code         ] = (par_info) { .cmd = internal_integer_cmd,   .chr = broken_penalty_code,          .category = par_broken_penalty_category      };
    lmt_interface.par_data[par_adj_demerits_code           ] = (par_info) { .cmd = internal_integer_cmd,   .chr = adj_demerits_code,            .category = par_demerits_category            };
    lmt_interface.par_data[par_double_hyphen_demerits_code ] = (par_info) { .cmd = internal_integer_cmd,   .chr = double_hyphen_demerits_code,  .category = par_demerits_category            };
    lmt_interface.par_data[par_final_hyphen_demerits_code  ] = (par_info) { .cmd = internal_integer_cmd,   .chr = final_hyphen_demerits_code,   .category = par_demerits_category            };
    lmt_interface.par_data[par_par_shape_code              ] = (par_info) { .cmd = specification_cmd,      .chr = par_shape_code,               .category = par_shape_category               };
    lmt_interface.par_data[par_inter_line_penalties_code   ] = (par_info) { .cmd = specification_cmd,      .chr = inter_line_penalties_code,    .category = par_line_penalty_category        };
    lmt_interface.par_data[par_club_penalties_code         ] = (par_info) { .cmd = specification_cmd,      .chr = club_penalties_code,          .category = par_club_penalty_category        };
    lmt_interface.par_data[par_widow_penalties_code        ] = (par_info) { .cmd = specification_cmd,      .chr = widow_penalties_code,         .category = par_widow_penalty_category       };
    lmt_interface.par_data[par_display_widow_penalties_code] = (par_info) { .cmd = specification_cmd,      .chr = display_widow_penalties_code, .category = par_display_penalty_category     };
    lmt_interface.par_data[par_broken_penalties_code       ] = (par_info) { .cmd = specification_cmd,      .chr = broken_penalties_code,        .category = par_broken_penalty_category      };
    lmt_interface.par_data[par_orphan_penalties_code       ] = (par_info) { .cmd = specification_cmd,      .chr = orphan_penalties_code,        .category = par_orphan_penalty_category      };
    lmt_interface.par_data[par_toddler_penalties_code      ] = (par_info) { .cmd = specification_cmd,      .chr = toddler_penalties_code,       .category = par_toddler_penalty_category     };
    lmt_interface.par_data[par_fitness_classes_code        ] = (par_info) { .cmd = specification_cmd,      .chr = fitness_classes_code,         .category = par_fitness_classes_category     };
    lmt_interface.par_data[par_adjacent_demerits_code      ] = (par_info) { .cmd = specification_cmd,      .chr = adjacent_demerits_code,       .category = par_demerits_category            };
    lmt_interface.par_data[par_orphan_line_factors_code    ] = (par_info) { .cmd = internal_integer_cmd,   .chr = orphan_line_factors_code,     .category = par_orphan_penalty_category      };
    lmt_interface.par_data[par_baseline_skip_code          ] = (par_info) { .cmd = internal_glue_cmd,      .chr = baseline_skip_code,           .category = par_line_category                };
    lmt_interface.par_data[par_line_skip_code              ] = (par_info) { .cmd = internal_glue_cmd,      .chr = line_skip_code,               .category = par_line_category                };
    lmt_interface.par_data[par_line_skip_limit_code        ] = (par_info) { .cmd = internal_dimension_cmd, .chr = line_skip_limit_code,         .category = par_line_category                };
    lmt_interface.par_data[par_adjust_spacing_step_code    ] = (par_info) { .cmd = internal_integer_cmd,   .chr = adjust_spacing_step_code,     .category = par_adjust_category              };
    lmt_interface.par_data[par_adjust_spacing_shrink_code  ] = (par_info) { .cmd = internal_integer_cmd,   .chr = adjust_spacing_shrink_code,   .category = par_adjust_category              };
    lmt_interface.par_data[par_adjust_spacing_stretch_code ] = (par_info) { .cmd = internal_integer_cmd,   .chr = adjust_spacing_stretch_code,  .category = par_adjust_category              };
    lmt_interface.par_data[par_hyphenation_mode_code       ] = (par_info) { .cmd = internal_integer_cmd,   .chr = hyphenation_mode_code,        .category = par_hyphenation_category,         };
    lmt_interface.par_data[par_shaping_penalties_mode_code ] = (par_info) { .cmd = internal_integer_cmd,   .chr = shaping_penalties_mode_code,  .category = par_shaping_penalty_category     };
    lmt_interface.par_data[par_shaping_penalty_code        ] = (par_info) { .cmd = internal_integer_cmd,   .chr = shaping_penalty_code,         .category = par_shaping_penalty_category     };
    lmt_interface.par_data[par_emergency_extra_stretch_code] = (par_info) { .cmd = internal_dimension_cmd, .chr = emergency_extra_stretch_code, .category = par_emergency_category           };
    lmt_interface.par_data[par_par_passes_code             ] = (par_info) { .cmd = specification_cmd,      .chr = par_passes_code,              .category = par_par_passes_category          };
    lmt_interface.par_data[par_line_break_checks_code      ] = (par_info) { .cmd = internal_integer_cmd,   .chr = line_break_checks_code,       .category = par_line_break_checks_category   };
    lmt_interface.par_data[par_single_line_penalty_code    ] = (par_info) { .cmd = internal_integer_cmd,   .chr = single_line_penalty_code,     .category = par_single_line_penalty_category };
    lmt_interface.par_data[par_hyphen_penalty_code         ] = (par_info) { .cmd = internal_integer_cmd,   .chr = hyphen_penalty_code,          .category = par_hyphen_penalty_category      };
    lmt_interface.par_data[par_ex_hyphen_penalty_code      ] = (par_info) { .cmd = internal_integer_cmd,   .chr = ex_hyphen_penalty_code,       .category = par_ex_hyphen_penalty_category   };

}

/*tex

    When we copy a node list, there are several possibilities: we do the same as a new node, we
    copy the entry to the table in properties (a reference), we do a deep copy of a table in the
    properties, we create a new table and give it the original one as a metatable. After some
    experiments (that also included timing) with these scenarios I decided that a deep copy made no
    sense, nor did nilling. In the end both the shallow copy and the metatable variant were both
    okay, although the metatables at that time performed a bit less. The most important aspect to
    keep in mind is that references to other nodes in properties no longer can be valid for that
    copy. We could use two tables (one unique and one shared) or metatables but that only
    complicates matters. Maybe some day I will remove the method that not used in \CONTEXT, just
    because switching methods during a run will for sure break things.

    When defining a new node, we could already allocate a table but it is rather easy to do that at
    the lua end e.g. using a metatable __index method. That way it is under macro package control.

    When deleting a node, we could keep the slot (e.g. setting it to false) but it could make
    memory consumption raise unneeded when we have temporary large node lists and after that only
    small lists.

    So, in the end this is what we ended up with. For the record, I also experimented with the
    following:

    \startitemize

        \startitem
            Copy attributes to the properties so that we have fast access at the \LUA\ end: in the
            end the overhead is not compensated by speed and convenience, in fact, attributes are
            not that slow when it comes to accessing them.
        \stopitem

        \startitem
            A bitset in the node but again the gain compared to attributes is neglectable and it
            also demands a pretty string agreement over what bit represents what, and this is
            unlikely to succeed in the tex community (I could use it for font handling, which is
            cross package, but decided that it doesn't pay off.
        \stopitem

    \stopitemize

    In case one wonders why properties make sense then, well, it is not so much speed that we gain,
    but more convenience: storing all kind of (temporary) data in attributes is no fun and this
    mechanism makes sure that properties are cleaned up when a node is freed. Also, the advantage
    of a more or less global properties table is that we stay at the \LUA\ end. An alternative is
    to store a reference in the node itself but that is complicated by the fact that the register
    has some limitations (no numeric keys) and we also don't want to mess with it too much.

    We keep track of nesting so that we don't overflow the stack, and, what is more important,
    don't keep resolving the registry index.

    We could add an index field to each node and use that one. But then we'd have to default to
    false. It actually would look nicer in tracing: indices instead of pseudo memory slots. It
    would not boost performance. A table like this is never really collected.

*/

static inline void lmt_properties_push(lua_State * L)
{
    lmt_node_memory_state.lua_properties_level++;
    if (lmt_node_memory_state.lua_properties_level == 1) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_node_memory_state.node_properties_id);
    }
}

static inline void lmt_properties_pop(lua_State * L)
{
    if (lmt_node_memory_state.lua_properties_level == 1) {
        lua_pop(L, 1);
    }
    lmt_node_memory_state.lua_properties_level--;
}

/*tex Resetting boils down to nilling. */

static inline void lmt_properties_reset(lua_State * L, halfword target)
{
    if (lmt_node_memory_state.lua_properties_level == 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_node_memory_state.node_properties_id);
        lua_pushnil(L);
        lua_rawseti(L, -2, target);
        lua_pop(L, 1);
    } else {
        lua_pushnil(L);
        lua_rawseti(L, -2, target);
    }
}

static inline void lmt_properties_copy(lua_State *L, halfword target, halfword source)
{
    if (lmt_node_memory_state.lua_properties_level == 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_node_memory_state.node_properties_id);
    }
    /* properties */
    if (lua_rawgeti(L, -1, source) == LUA_TTABLE) {
        /* properties source */
        lua_createtable(L, 0, 1);
        /* properties source {} */
        lua_insert(L, -2);
        /* properties {} source */
        lua_push_key(__index);
        /* properties {} source "__index" */
        lua_insert(L, -2);
        /* properties {} "__index" source  */
        lua_rawset(L, -3);
        /* properties {__index=source} */
        lua_createtable(L, 0, 1);
        /* properties {__index=source} {} */
        lua_insert(L, -2);
        /* properties {} {__index=source} */
        lua_setmetatable(L, -2);
        /* properties {}->{__index=source} */
        lua_rawseti(L, -2, target);
        /* properties[target]={}->{__index=source} */
    } else {
        /* properties nil */
        lua_pop(L, 1);
    }
    /* properties */
    if (lmt_node_memory_state.lua_properties_level == 0) {
        lua_pop(L, 1);
    }
}

/*tex The public one: */

void tex_reset_node_properties(halfword b)
{
    if (b) {
        lmt_properties_reset(lmt_lua_state.lua_instance, b);
    }
}

/*tex Here end the property handlers. */

static void tex_aux_node_range_test(halfword a, halfword b)
{
    if (b < 0 || b >= lmt_node_memory_state.nodes_data.allocated) {
        tex_formatted_error("nodes", "node range test failed in %s node", lmt_interface.node_data[node_type(a)].name);
    }
}

/*tex This needs a cleanup ... there is no need to store the pointer location itself. */

static inline void tex_aux_preset_disc_node(halfword n)
{
    disc_pre_break(n) = disc_pre_break_node(n);
    disc_post_break(n) = disc_post_break_node(n);
    disc_no_break(n) = disc_no_break_node(n);
    node_type(disc_pre_break(n)) = nesting_node;
    node_type(disc_post_break(n)) = nesting_node;
    node_type(disc_no_break(n)) = nesting_node;
    node_subtype(disc_pre_break(n)) = pre_break_code;
    node_subtype(disc_post_break(n)) = post_break_code;
    node_subtype(disc_no_break(n)) = no_break_code;
}

static inline void tex_aux_preset_node(halfword n, quarterword type)
{
    switch (type) {
        case glyph_node:
            break;
        case hlist_node:
        case vlist_node:
            box_direction(n) = direction_unknown;
            break;
        case disc_node:
            tex_aux_preset_disc_node(n);
            break;
        case rule_node:
            rule_width(n) = null_flag;
            rule_depth(n) = null_flag;
            rule_height(n) = null_flag;
            rule_data(n) = 0;
            break;
        case unset_node:
            box_width(n) = null_flag;
            break;
        case specification_node:
            specification_pointer(n) = NULL;
         // specification_size(n) = 0;
         // tex_null_specification_list(n); /* no */
            break;
        case simple_noad:
        case radical_noad:
        case fraction_noad:
        case accent_noad:
        case fence_noad:
            noad_family(n) = unused_math_family;
            noad_style(n) = unused_math_style;
            tex_reset_noad_classes(n); /* unsets them */
            break;
    }
}

halfword tex_new_node(quarterword type, quarterword subtype)
{
    halfword size = get_node_size(type);
    halfword node = tex_get_node(size);
    /*tex

        Both type() and subtype() will be set below, and node_next() is set to null by |get_node()|,
        so we can clear one word less than |s|.

    */
    memset((void *) (lmt_node_memory_state.nodes + node + 1), 0, (sizeof(memoryword) * ((size_t) size - 1)));

    if (tex_nodetype_is_complex(type)) {
        tex_aux_preset_node(node, type);
        if (input_file_state.mode > 0) {
            /*tex See table above. */
            switch (type) {
                case glyph_node:
                    if (input_file_state.mode > 1) {
                        glyph_input_file(node) = input_file_value();
                        glyph_input_line(node) = input_line_value();
                    }
                    break;
                case hlist_node:
                case vlist_node:
                case unset_node:
                    box_input_file(node) = input_file_value();
                    box_input_line(node) = input_line_value();
                    break;
            }
        }
        if (tex_nodetype_has_attributes(type)) {
            attach_current_attribute_list(node);
        }
    }
    /* last */
    node_type(node) = type;
    node_subtype(node) = subtype;
    return node;
}

halfword tex_new_temp_node(void)
{
    halfword n = tex_get_node(temp_node_size);
    node_type(n) = temp_node;
    node_subtype(n) = 0;
    memset((void *) (lmt_node_memory_state.nodes + n + 1), 0, (sizeof(memoryword) * (temp_node_size - 1)));
    return n;
}

static halfword tex_aux_new_glyph_node_with_attributes(halfword parent)
{
    halfword n = tex_get_node(glyph_node_size);
    memset((void *) (lmt_node_memory_state.nodes + n + 1), 0, (sizeof(memoryword) * (glyph_node_size - 1)));
    if (input_file_state.mode > 1) {
        glyph_input_file(n) = input_file_value();
        glyph_input_line(n) = input_line_value();
    }
    node_type(n) = glyph_node;
    node_subtype(n) = glyph_unset_subtype;
    if (parent) {
        tex_attach_attribute_list_copy(n, parent);
    } else {
        attach_current_attribute_list(n);
    }
    return n;
}

/*tex
    This makes a duplicate of the node list that starts at |p| and returns a pointer to the new
    list.
*/

halfword tex_copy_node_list(halfword p, halfword end)
{
    /*tex head of the list */
    halfword h = null;
    /*tex previous position in new list */
    halfword q = null;
    /*tex saves stack and time */
    lua_State *L = lmt_lua_state.lua_instance;
    lmt_properties_push(L);
    while (p != end) {
        halfword s = tex_copy_node(p);
        if (h) {
            tex_couple_nodes(q, s);
        } else {
            h = s;
        }
        q = s;
        p = node_next(p);
    }
    /*tex saves stack and time */
    lmt_properties_pop(L);
    return h;
}

/*tex Make a dupe of a single node. */

halfword tex_copy_node_only(halfword p)
{
    quarterword t = node_type(p);
    int s = get_node_size(t);
    halfword r = tex_get_node(s);
    memcpy((void *) (lmt_node_memory_state.nodes + r), (void *) (lmt_node_memory_state.nodes + p), sizeof(memoryword));
    memset((void *) (lmt_node_memory_state.nodes + r + 1), 0, sizeof(memoryword) * ((unsigned) s - 1));
    tex_aux_preset_node(r, t);
    return r;
}

/*tex
    We really need to use macros here as we need the temporary variable because varmem can be
    reallocated! We cross our fingers that the compiler doesn't optimize that one away. (The test
    suite had a few cases where reallocation during a copy happens.) We can make |copy_stub|
    local here.
 */

# define copy_sub_list(target,source) do { \
     if (source) { \
         halfword copy_stub = tex_copy_node_list(source, null); \
         target = copy_stub; \
     } else { \
         target = null; \
     } \
 } while (0)

# define copy_sub_node(target,source) do { \
    if (source) { \
        halfword copy_stub = tex_copy_node(source); \
        target = copy_stub; \
    } else { \
        target = null; \
    } \
} while (0)

/*tex
    Checking for |null| does not happen here but in the calling routine because zero also means zero
    glue (so that we don't need to have many glue nodes hanging around). In traditional \TEX\ these
    were static shared (ref counted) glue nodes.
*/

halfword tex_copy_node(halfword original)
{
    /*tex
        We really need a stub for copying because mem might move in the meantime due to resizing!
    */
    if (original < 0 || original >= lmt_node_memory_state.nodes_data.allocated) {
        return tex_formatted_error("nodes", "attempt to copy an impossible node %d", (int) original);
    } else if (original > lmt_node_memory_state.reserved && lmt_node_memory_state.nodesizes[original] == 0) {
        return tex_formatted_error("nodes", "attempt to copy a free %s node %d", get_node_name(node_type(original)), (int) original);
    } else {
        /*tex type of node */
        halfword type = node_type(original);
        int size = get_node_size(type);
        /*tex current node being fabricated for new list */
        halfword copy = tex_get_node(size);
        /*tex this saves work */
        memcpy((void *) (lmt_node_memory_state.nodes + copy), (void *) (lmt_node_memory_state.nodes + original), (sizeof(memoryword) * (unsigned) size));
        if (tex_nodetype_is_complex(type)) { /* was size ... brrrr */
            /*tex Beware: specifications share with prev, next and attr. */
            if (tex_nodetype_has_attributes(type)) {
                add_attribute_reference(node_attr(original));
                node_prev(copy) = null;
                lmt_properties_copy(lmt_lua_state.lua_instance, copy, original);
            }
            node_next(copy) = null;
            switch (type) {
                case glue_node:
                    copy_sub_list(glue_leader_ptr(copy), glue_leader_ptr(original));
                    break;
                case hlist_node:
                    copy_sub_list(box_pre_adjusted(copy), box_pre_adjusted(original));
                    copy_sub_list(box_post_adjusted(copy), box_post_adjusted(original));
                    // fall through
                case vlist_node:
                    copy_sub_list(box_pre_migrated(copy), box_pre_migrated(original));
                    copy_sub_list(box_post_migrated(copy), box_post_migrated(original));
                    // fall through
                case unset_node:
                    copy_sub_list(box_list(copy), box_list(original));
                    copy_sub_list(box_except(copy), box_except(original));
                    break;
                case disc_node:
                    disc_pre_break(copy) = disc_pre_break_node(copy);
                    if (disc_pre_break_head(original)) {
                        tex_set_disc_field(copy, pre_break_code, tex_copy_node_list(disc_pre_break_head(original), null));
                    } else {
                        tex_set_disc_field(copy, pre_break_code, null);
                    }
                    disc_post_break(copy) = disc_post_break_node(copy);
                    if (disc_post_break_head(original)) {
                        tex_set_disc_field(copy, post_break_code, tex_copy_node_list(disc_post_break_head(original), null));
                    } else {
                        tex_set_disc_field(copy, post_break_code, null);
                    }
                    disc_no_break(copy) = disc_no_break_node(copy);
                    if (disc_no_break_head(original)) {
                        tex_set_disc_field(copy, no_break_code, tex_copy_node_list(disc_no_break_head(original), null));
                    } else {
                        tex_set_disc_field(copy, no_break_code, null);
                    }
                    break;
                case insert_node:
                    if (insert_split_top(original)) {
                        insert_split_top(copy) = tex_copy_node(insert_split_top(original));
                    }
                    copy_sub_list(insert_list(copy), insert_list(original));
                    break;
                case mark_node:
                    tex_add_token_reference(mark_ptr(original));
                    break;
                case adjust_node:
                    copy_sub_list(adjust_list(copy), adjust_list(original));
                    break;
                case choice_node:
                    copy_sub_list(choice_display_mlist(copy), choice_display_mlist(original));
                    copy_sub_list(choice_text_mlist(copy), choice_text_mlist(original));
                    copy_sub_list(choice_script_mlist(copy), choice_script_mlist(original));
                    copy_sub_list(choice_script_script_mlist(copy), choice_script_script_mlist(original));
                    break;
                case simple_noad:
                case radical_noad:
                case fraction_noad:
                case accent_noad:
                    copy_sub_list(noad_nucleus(copy), noad_nucleus(original));
                    copy_sub_list(noad_subscr(copy), noad_subscr(original));
                    copy_sub_list(noad_supscr(copy), noad_supscr(original));
                    copy_sub_list(noad_subprescr(copy), noad_subprescr(original));
                    copy_sub_list(noad_supprescr(copy), noad_supprescr(original));
                    copy_sub_list(noad_prime(copy), noad_prime(original));
                 // copy_sub_list(noad_state(copy), noad_state(original));
                    switch (type) {
                        case radical_noad:
                            copy_sub_node(radical_left_delimiter(copy), radical_left_delimiter(original));
                            copy_sub_node(radical_right_delimiter(copy), radical_right_delimiter(original));
                            copy_sub_node(radical_top_delimiter(copy), radical_top_delimiter(original));
                            copy_sub_node(radical_bottom_delimiter(copy), radical_bottom_delimiter(original));
                            copy_sub_list(radical_degree(copy), radical_degree(original));
                            break;
                        case fraction_noad:
                         // copy_sub_list(fraction_numerator(copy), fraction_numerator(p));
                         // copy_sub_list(fraction_denominator(copy), fraction_denominator(p);
                            copy_sub_node(fraction_left_delimiter(copy), fraction_left_delimiter(original));
                            copy_sub_node(fraction_right_delimiter(copy), fraction_right_delimiter(original));
                            copy_sub_node(fraction_middle_delimiter(copy), fraction_middle_delimiter(original));
                            break;
                        case accent_noad:
                            copy_sub_list(accent_top_character(copy), accent_top_character(original));
                            copy_sub_list(accent_bottom_character(copy), accent_bottom_character(original));
                            copy_sub_list(accent_middle_character(copy), accent_middle_character(original));
                            break;
                    }
                    if (noad_extra_attr(copy)) {
                        add_attribute_reference(noad_extra_attr(copy));
                    }
                    break;
                case fence_noad:
                    /* in principle also scripts */
                    copy_sub_node(fence_delimiter(copy), fence_delimiter(original));
                    copy_sub_node(fence_delimiter_top(copy), fence_delimiter_top(original));
                    copy_sub_node(fence_delimiter_bottom(copy), fence_delimiter_bottom(original));
                    if (noad_extra_attr(copy)) {
                        add_attribute_reference(noad_extra_attr(copy));
                    }
                    break;
                case sub_box_node:
                case sub_mlist_node:
                    copy_sub_list(kernel_math_list(copy), kernel_math_list(original));
                    break;
                case par_node:
                    /* can also be copy_sub_node */
                    copy_sub_list(par_box_left(copy), par_box_left(original));
                    copy_sub_list(par_box_right(copy), par_box_right(original));
                    copy_sub_list(par_box_middle(copy), par_box_middle(original));
                    /* wipe copied fields */
                    par_left_skip(copy) = null;
                    par_right_skip(copy) = null;
                    par_par_fill_left_skip(copy) = null;
                    par_par_fill_right_skip(copy) = null;
                    par_par_init_left_skip(copy) = null;
                    par_par_init_right_skip(copy) = null;
                    par_emergency_left_skip(copy) = null;
                    par_emergency_right_skip(copy) = null;
                    par_baseline_skip(copy) = null;
                    par_line_skip(copy) = null;
                    par_par_shape(copy) = null;
                    par_inter_line_penalties(copy) = null;
                    par_club_penalties(copy) = null;
                    par_widow_penalties(copy) = null;
                    par_display_widow_penalties(copy) = null;
                    par_broken_penalties(copy) = null;
                    par_orphan_penalties(copy) = null;
                    par_toddler_penalties(copy) = null;
                    par_fitness_classes(copy) = null;
                    par_adjacent_demerits(copy) = null;
                    par_orphan_line_factors(copy) = null;
                    par_par_passes(copy) = null;
                    /* really copy fields */
                    tex_set_par_par(copy, par_left_skip_code, tex_get_par_par(original, par_left_skip_code), 1);
                    tex_set_par_par(copy, par_right_skip_code, tex_get_par_par(original, par_right_skip_code), 1);
                    tex_set_par_par(copy, par_par_fill_left_skip_code, tex_get_par_par(original, par_par_fill_left_skip_code), 1);
                    tex_set_par_par(copy, par_par_fill_right_skip_code, tex_get_par_par(original, par_par_fill_right_skip_code), 1);
                    tex_set_par_par(copy, par_par_init_left_skip_code, tex_get_par_par(original, par_par_init_left_skip_code), 1);
                    tex_set_par_par(copy, par_par_init_right_skip_code, tex_get_par_par(original, par_par_init_right_skip_code), 1);
                    tex_set_par_par(copy, par_baseline_skip_code, tex_get_par_par(original, par_baseline_skip_code), 1);
                    tex_set_par_par(copy, par_line_skip_code, tex_get_par_par(original, par_line_skip_code), 1);
                    tex_set_par_par(copy, par_par_shape_code, tex_get_par_par(original, par_par_shape_code), 1);
                    tex_set_par_par(copy, par_inter_line_penalties_code, tex_get_par_par(original, par_inter_line_penalties_code), 1);
                    tex_set_par_par(copy, par_club_penalties_code, tex_get_par_par(original, par_club_penalties_code), 1);
                    tex_set_par_par(copy, par_widow_penalties_code, tex_get_par_par(original, par_widow_penalties_code), 1);
                    tex_set_par_par(copy, par_display_widow_penalties_code, tex_get_par_par(original, par_display_widow_penalties_code), 1);
                    tex_set_par_par(copy, par_orphan_penalties_code, tex_get_par_par(original, par_orphan_penalties_code), 1);
                    tex_set_par_par(copy, par_toddler_penalties_code, tex_get_par_par(original, par_toddler_penalties_code), 1);
                    tex_set_par_par(copy, par_fitness_classes_code, tex_get_par_par(original, par_fitness_classes_code), 1);
                    tex_set_par_par(copy, par_adjacent_demerits_code, tex_get_par_par(original, par_adjacent_demerits_code), 1);
                    tex_set_par_par(copy, par_orphan_line_factors_code, tex_get_par_par(original, par_orphan_line_factors_code), 1);
                    tex_set_par_par(copy, par_par_passes_code, tex_get_par_par(original, par_par_passes_code), 1);
                    /* tokens, we could mess with a ref count instead */
                    par_end_par_tokens(copy) = par_end_par_tokens(original);
                    tex_add_token_reference(par_end_par_tokens(original));
                    break;
                case specification_node:
                    /* next field is zeroed */
                    specification_count(copy) = specification_count(original);
                    specification_options(copy) = specification_options(original);
                    specification_size(copy) = specification_size(original);
                    tex_copy_specification_list(copy, original);
                    break;
                default:
                    break;
            }
        }
        return copy;
    }
}

static inline void tex_aux_free_sub_node_list(halfword source)
{
    if (source) {
        tex_flush_node_list(source);
    }
}

static inline void tex_aux_free_sub_node(halfword source)
{
    if (source) {
        tex_flush_node(source);
    }
}

/* We don't need the checking for attributes if we make these lists frozen. */

void tex_flush_node(halfword p)
{
    if (! p) {
        /*tex legal, but no-op. */
        return;
    } else if (p <= lmt_node_memory_state.reserved || p >= lmt_node_memory_state.nodes_data.allocated) {
        tex_formatted_error("nodes", "attempt to free an impossible node %d of type %d", (int) p, node_type(p));
    } else if (lmt_node_memory_state.nodesizes[p] == 0) {
        for (int i = (lmt_node_memory_state.reserved + 1); i < lmt_node_memory_state.nodes_data.allocated; i++) {
            if (lmt_node_memory_state.nodesizes[i] > 0) {
                tex_aux_check_node(i);
            }
        }
        tex_formatted_error("nodes", "attempt to double-free %s node %d, ignored", get_node_name(node_type(p)), (int) p);
    } else {
        int t = node_type(p);
        if (tex_nodetype_is_complex(t)) {
            switch (t) {
                case glue_node:
                    tex_aux_free_sub_node_list(glue_leader_ptr(p));
                    break;
                case hlist_node:
                    tex_aux_free_sub_node_list(box_pre_adjusted(p));
                    tex_aux_free_sub_node_list(box_post_adjusted(p));
                    // fall through
                case vlist_node:
                    tex_aux_free_sub_node_list(box_pre_migrated(p));
                    tex_aux_free_sub_node_list(box_post_migrated(p));
                    // fall through
                case unset_node:
                    tex_aux_free_sub_node_list(box_list(p));
                    tex_aux_free_sub_node_list(box_except(p));
                    break;
                case disc_node:
                    /*tex Watch the start at temp node hack! */
                    tex_aux_free_sub_node_list(disc_pre_break_head(p));
                    tex_aux_free_sub_node_list(disc_post_break_head(p));
                    tex_aux_free_sub_node_list(disc_no_break_head(p));
                    break;
                case par_node:
                    tex_aux_free_sub_node_list(par_box_left(p));
                    tex_aux_free_sub_node_list(par_box_right(p));
                    tex_aux_free_sub_node_list(par_box_middle(p));
                    /* we could check for the flag */
                    tex_flush_node(par_left_skip(p));
                    tex_flush_node(par_right_skip(p));
                    tex_flush_node(par_par_fill_left_skip(p));
                    tex_flush_node(par_par_fill_right_skip(p));
                    tex_flush_node(par_par_init_left_skip(p));
                    tex_flush_node(par_par_init_right_skip(p));
                    tex_flush_node(par_emergency_left_skip(p));
                    tex_flush_node(par_emergency_right_skip(p));
                    tex_flush_node(par_baseline_skip(p));
                    tex_flush_node(par_line_skip(p));
                    tex_flush_node(par_par_shape(p));
                    tex_flush_node(par_club_penalties(p));
                    tex_flush_node(par_inter_line_penalties(p));
                    tex_flush_node(par_widow_penalties(p));
                    tex_flush_node(par_display_widow_penalties(p));
                    tex_flush_node(par_broken_penalties(p));
                    tex_flush_node(par_orphan_penalties(p));
                    tex_flush_node(par_toddler_penalties(p));
                    tex_flush_node(par_fitness_classes(p));
                    tex_flush_node(par_adjacent_demerits(p));
                    tex_flush_node(par_orphan_line_factors(p));
                    tex_flush_node(par_par_passes(p));
                    /* tokens */
                    tex_flush_token_list(par_end_par_tokens(p));
                    break;
                case insert_node:
                    if (insert_split_top(p)) {
                        tex_flush_node(insert_split_top(p));
                    }
                    tex_flush_node_list(insert_list(p));
                    break;
                case mark_node:
                    tex_delete_token_reference(mark_ptr(p));
                    break;
                case adjust_node:
                    tex_flush_node_list(adjust_list(p));
                    break;
                /*tex
                    Beware: math nodes get freed in |mlist_to_hlist| selectively because some fields
                    are reused or migrate. So there we also need to flush attribute fields!
                */
                case choice_node:
                    tex_aux_free_sub_node_list(choice_display_mlist(p));
                    tex_aux_free_sub_node_list(choice_text_mlist(p));
                    tex_aux_free_sub_node_list(choice_script_mlist(p));
                    tex_aux_free_sub_node_list(choice_script_script_mlist(p));
                    break;
                case simple_noad:
                case fraction_noad:
                case radical_noad:
                case accent_noad:
                    tex_aux_free_sub_node_list(noad_nucleus(p));
                    tex_aux_free_sub_node_list(noad_subscr(p));
                    tex_aux_free_sub_node_list(noad_supscr(p));
                    tex_aux_free_sub_node_list(noad_subprescr(p));
                    tex_aux_free_sub_node_list(noad_supprescr(p));
                    tex_aux_free_sub_node_list(noad_prime(p));
                 // tex_aux_free_sub_node_list(noad_state(p));
                    switch (t) {
                        case fraction_noad:
                         // tex_aux_free_sub_node_list(fraction_numerator(p));
                         // tex_aux_free_sub_node_list(fraction_denominator(p));
                            tex_aux_free_sub_node(fraction_left_delimiter(p));
                            tex_aux_free_sub_node(fraction_right_delimiter(p));
                            tex_aux_free_sub_node(fraction_middle_delimiter(p));
                            break;
                        case radical_noad:
                            tex_aux_free_sub_node(radical_left_delimiter(p));
                            tex_aux_free_sub_node(radical_right_delimiter(p));
                            tex_aux_free_sub_node(radical_top_delimiter(p));
                            tex_aux_free_sub_node(radical_bottom_delimiter(p));
                            tex_aux_free_sub_node_list(radical_degree(p));
                            break;
                        case accent_noad:
                            tex_aux_free_sub_node_list(accent_top_character(p));
                            tex_aux_free_sub_node_list(accent_bottom_character(p));
                            tex_aux_free_sub_node_list(accent_middle_character(p));
                            break;
                    }
                    if (noad_extra_attr(p)) {
                        delete_attribute_reference(noad_extra_attr(p));
                    }
                    break;
                case fence_noad:
                    tex_aux_free_sub_node_list(fence_delimiter(p));
                    tex_aux_free_sub_node_list(fence_delimiter_top(p));
                    tex_aux_free_sub_node_list(fence_delimiter_bottom(p));
                    if (noad_extra_attr(p)) {
                        delete_attribute_reference(noad_extra_attr(p));
                    }
                    break;
                case sub_box_node:
                case sub_mlist_node:
                    tex_aux_free_sub_node_list(kernel_math_list(p));
                    break;
                /*tex That was the last math node. */
                case specification_node:
                    tex_dispose_specification_list(p);
                    break;
                default:
                    break;
            }
            if (tex_nodetype_has_attributes(t)) {
             // if (node_attr(p)) {
             //     printf("%i : %i.%i : %i %i %s\n",
             //         p,
             //         node_type(p),
             //         node_subtype(p),
             //         node_attr(p),
             //         attribute_count(node_attr(p)),
             //         attribute_count(node_attr(p)) ? "" : "!"
             //     );
             // }
                delete_attribute_reference(node_attr(p));
                node_attr(p) = null; /* when we debug */
                lmt_properties_reset(lmt_lua_state.lua_instance, p);
            }
        }
        tex_free_node(p, get_node_size(t));
    }
}

/*tex Erase the list of nodes starting at |pp|. */

void tex_flush_node_list(halfword l)
{
    if (! l) {
        /*tex Legal, but no-op. */
        return;
    } else if (l <= lmt_node_memory_state.reserved || l >= lmt_node_memory_state.nodes_data.allocated) {
        tex_formatted_error("nodes", "attempt to free an impossible node list %d of type %d", (int) l, node_type(l));
    } else if (lmt_node_memory_state.nodesizes[l] == 0) {
        for (int i = (lmt_node_memory_state.reserved + 1); i < lmt_node_memory_state.nodes_data.allocated; i++) {
            if (lmt_node_memory_state.nodesizes[i] > 0) {
                tex_aux_check_node(i);
            }
        }
        tex_formatted_error("nodes", "attempt to double-free %s node %d, ignored", get_node_name(node_type(l)), (int) l);
    } else {
        /*tex Saves stack and time. */
        lua_State *L = lmt_lua_state.lua_instance;
        lmt_properties_push(L);
        while (l) {
            halfword nxt = node_next(l);
            tex_flush_node(l);
            l = nxt;
        }
        /*tex Saves stack and time. */
        lmt_properties_pop(L);
    }
}

static void tex_aux_check_node(halfword p)
{
    halfword type = node_type(p);
    switch (type) {
        case glue_node:
            tex_aux_node_range_test(p, glue_leader_ptr(p));
            break;
        case hlist_node:
            tex_aux_node_range_test(p, box_pre_adjusted(p));
            tex_aux_node_range_test(p, box_post_adjusted(p));
            // fall through
        case vlist_node:
            tex_aux_node_range_test(p, box_pre_migrated(p));
            tex_aux_node_range_test(p, box_post_migrated(p));
            // fall through
        case unset_node:
            tex_aux_node_range_test(p, box_except(p));
            // fall through
        case align_record_node:
            tex_aux_node_range_test(p, box_list(p));
            break;
        case insert_node:
            tex_aux_node_range_test(p, insert_split_top(p));
            tex_aux_node_range_test(p, insert_list(p));
            break;
        case disc_node:
            tex_aux_node_range_test(p, disc_pre_break_head(p));
            tex_aux_node_range_test(p, disc_post_break_head(p));
            tex_aux_node_range_test(p, disc_no_break_head(p));
            break;
        case adjust_node:
            tex_aux_node_range_test(p, adjust_list(p));
            break;
        case choice_node:
            tex_aux_node_range_test(p, choice_display_mlist(p));
            tex_aux_node_range_test(p, choice_text_mlist(p));
            tex_aux_node_range_test(p, choice_script_mlist(p));
            tex_aux_node_range_test(p, choice_script_script_mlist(p));
            break;
        case simple_noad:
        case radical_noad:
        case fraction_noad:
        case accent_noad:
            tex_aux_node_range_test(p, noad_nucleus(p));
            tex_aux_node_range_test(p, noad_subscr(p));
            tex_aux_node_range_test(p, noad_supscr(p));
            tex_aux_node_range_test(p, noad_subprescr(p));
            tex_aux_node_range_test(p, noad_supprescr(p));
            tex_aux_node_range_test(p, noad_prime(p));
         // tex_aux_node_range_test(p, noad_state(p));
            switch (type) {
                case radical_noad:
                    tex_aux_node_range_test(p, radical_degree(p));
                    tex_aux_node_range_test(p, radical_left_delimiter(p));
                    tex_aux_node_range_test(p, radical_right_delimiter(p));
                    tex_aux_node_range_test(p, radical_top_delimiter(p));
                    tex_aux_node_range_test(p, radical_bottom_delimiter(p));
                    break;
                case fraction_noad:
                 // tex_aux_node_range_test(p, fraction_numerator(p));
                 // tex_aux_node_range_test(p, fraction_denominator(p));
                    tex_aux_node_range_test(p, fraction_left_delimiter(p));
                    tex_aux_node_range_test(p, fraction_right_delimiter(p));
                    tex_aux_node_range_test(p, fraction_middle_delimiter(p));
                    break;
                case accent_noad:
                    tex_aux_node_range_test(p, accent_top_character(p));
                    tex_aux_node_range_test(p, accent_bottom_character(p));
                    tex_aux_node_range_test(p, accent_middle_character(p));
                    break;
            }
         // tex_aux_node_range_test(p, noad_extra_attr(p));
            break;
        case fence_noad:
            tex_aux_node_range_test(p, fence_delimiter(p));
            tex_aux_node_range_test(p, fence_delimiter_top(p));
            tex_aux_node_range_test(p, fence_delimiter_bottom(p));
         // tex_aux_node_range_test(p, noad_extra_attr(p));
            break;
        case par_node:
            tex_aux_node_range_test(p, par_box_left(p));
            tex_aux_node_range_test(p, par_box_right(p));
            tex_aux_node_range_test(p, par_box_middle(p));
            tex_aux_node_range_test(p, par_left_skip(p));
            tex_aux_node_range_test(p, par_right_skip(p));
            tex_aux_node_range_test(p, par_baseline_skip(p));
            tex_aux_node_range_test(p, par_line_skip(p));
            tex_aux_node_range_test(p, par_par_shape(p));
            tex_aux_node_range_test(p, par_club_penalties(p));
            tex_aux_node_range_test(p, par_inter_line_penalties(p));
            tex_aux_node_range_test(p, par_widow_penalties(p));
            tex_aux_node_range_test(p, par_display_widow_penalties(p));
            tex_aux_node_range_test(p, par_broken_penalties(p));
            tex_aux_node_range_test(p, par_orphan_penalties(p));
            tex_aux_node_range_test(p, par_fitness_classes(p));
            tex_aux_node_range_test(p, par_adjacent_demerits(p));
            tex_aux_node_range_test(p, par_orphan_line_factors(p));
            tex_aux_node_range_test(p, par_par_fill_left_skip(p));
            tex_aux_node_range_test(p, par_par_fill_right_skip(p));
            tex_aux_node_range_test(p, par_par_init_left_skip(p));
            tex_aux_node_range_test(p, par_par_init_right_skip(p));
            tex_aux_node_range_test(p, par_emergency_left_skip(p));
            tex_aux_node_range_test(p, par_emergency_right_skip(p));
            tex_aux_node_range_test(p, par_par_passes(p));
            break;
        default:
            break;
    }
}

/*
halfword fix_node_list(halfword head)
{
    if (head) {
        halfword tail = head;
        halfword next = node_next(head);
        while (next) {
            node_prev(next) = tail;
            tail = next;
            next = node_next(tail);
        }
        return tail;
    } else {
        return null;
    }
}
*/

halfword tex_get_node(int size)
{
    if (size < max_chain_size) { /*tex This test should not be needed! */
        halfword p = lmt_node_memory_state.free_chain[size];
        if (p) {
            lmt_node_memory_state.free_chain[size] = node_next(p);
            lmt_node_memory_state.nodesizes[p] = (char) size;
            node_next(p) = null;
            lmt_node_memory_state.nodes_data.ptr += size;
            return p;
        } else {
            return tex_aux_allocated_node(size);
        }
    } else {
        return tex_normal_error("nodes", "there is a problem in getting a node, case 1");
    }
}

void tex_free_node(halfword p, int size) /* no need to pass size, we can get it here */
{
    if (p > lmt_node_memory_state.reserved && size < max_chain_size) {
        lmt_node_memory_state.nodesizes[p] = 0;
        node_next(p) = lmt_node_memory_state.free_chain[size];
        lmt_node_memory_state.free_chain[size] = p;
        lmt_node_memory_state.nodes_data.ptr -= size;
    } else {
        tex_formatted_error("nodes", "node number %d of type %d with size %d should not be freed", (int) p, node_type(p), size);
    }
}

/*tex
    Although it is tempting to have dedicated get and free helpers for if and expression nodes it
    gives no gain (at least not in the third decimal on millions of tests). So I didn't even bother
    to keep the code here. A real dedicated stack might actually be faster.
*/

/*tex

    At the start of the node memory area we reserve some special nodes, for instance frequently
    used glue specifications. We could as well just use new_glue here but for the moment we stick
    to the traditional approach. We can omit the zeroing because it's already done.

*/

static void tex_aux_initialize_glue(halfword n, scaled amount, scaled stretch, scaled shrink, halfword stretchorder, halfword shrinkorder)
{
 // memset((void *) (node_memory_state.nodes + n), 0, (sizeof(memoryword) * node_memory_state.nodesizes[glue_spec_node]));
    node_type(n) = glue_spec_node;
    glue_amount(n) = amount;
    glue_stretch(n) = stretch;
    glue_shrink(n) = shrink;
    glue_stretch_order(n) = stretchorder;
    glue_shrink_order(n) = shrinkorder;
}

static void tex_aux_initialize_whatever_node(halfword n, quarterword type)
{
 // memset((void *) (node_memory_state.nodes + n), 0, (sizeof(memoryword) * node_memory_state.nodesizes[t]));
    node_type(n) = type;
}

static void tex_aux_initialize_character(halfword n, halfword chr)
{
 // memset((void *) (node_memory_state.nodes + n), 0, (sizeof(memoryword) * node_memory_state.nodesizes[glyph_node]));
    node_type(n) = glyph_node;
    glyph_character(n) = chr;
}
# define reserved_node_slots 32

void tex_initialize_node_mem()
{
    memoryword *nodes = NULL;
    char *sizes = NULL;
    int size = 0;
    if (lmt_main_state.run_state == initializing_state) {
        size = lmt_node_memory_state.nodes_data.minimum;
        lmt_node_memory_state.reserved = last_reserved;
        lmt_node_memory_state.nodes_data.top = last_reserved + 1;
        lmt_node_memory_state.nodes_data.allocated = size;
        lmt_node_memory_state.nodes_data.ptr = last_reserved;
    } else {
        size = lmt_node_memory_state.nodes_data.allocated;
        lmt_node_memory_state.nodes_data.initial = lmt_node_memory_state.nodes_data.ptr;
    }
    if (size > 0) {
        nodes = aux_allocate_clear_array(sizeof(memoryword), size, reserved_node_slots);
        sizes = aux_allocate_clear_array(sizeof(char), size, reserved_node_slots);
    }
    if (nodes && sizes) {
        lmt_node_memory_state.nodes = nodes;
        lmt_node_memory_state.nodesizes = sizes;
    } else {
        tex_overflow_error("nodes", size);
    }
}

void tex_initialize_nodes(void)
{
    if (lmt_main_state.run_state == initializing_state) {
        /*tex Initialize static glue specs. */

        tex_aux_initialize_glue(zero_glue,     0,      0,     0,               0,              0);
        tex_aux_initialize_glue(fi_glue,       0,      0,     0,   fi_glue_order,              0); /* why not |unity| here */
        tex_aux_initialize_glue(fi_l_glue,     0,  unity,     0,  fil_glue_order,              0); /* |\[vh]fil|    */
        tex_aux_initialize_glue(fi_ll_glue,    0,  unity,     0, fill_glue_order,              0); /* |\[vh]fill|   */
        tex_aux_initialize_glue(fi_ss_glue,    0,  unity, unity,  fil_glue_order, fil_glue_order); /* |\[vh]ss|     */
        tex_aux_initialize_glue(fi_l_neg_glue, 0, -unity,     0,  fil_glue_order,              0); /* |\[vh]filneg| */

        /*tex Initialize node list heads. */

        tex_aux_initialize_whatever_node(page_insert_head,  temp_node); /* actually a split node */
        tex_aux_initialize_whatever_node(contribute_head,   temp_node);
        tex_aux_initialize_whatever_node(page_head,         temp_node);
        tex_aux_initialize_whatever_node(temp_head,         temp_node);
        tex_aux_initialize_whatever_node(hold_head,         temp_node);
        tex_aux_initialize_whatever_node(post_adjust_head,  temp_node);
        tex_aux_initialize_whatever_node(pre_adjust_head,   temp_node);
        tex_aux_initialize_whatever_node(post_migrate_head, temp_node);
        tex_aux_initialize_whatever_node(pre_migrate_head,  temp_node);
        tex_aux_initialize_whatever_node(align_head,        temp_node);
        tex_aux_initialize_whatever_node(active_head,       unhyphenated_node);
        tex_aux_initialize_whatever_node(end_span,          span_node);

        tex_aux_initialize_character(begin_period, '.');
        tex_aux_initialize_character(end_period,   '.');
    }
}

int tex_used_node_count(void)
{
    int used = 0;
    for  (int i = lmt_node_memory_state.nodes_data.top; i > lmt_node_memory_state.reserved; i--) {
        if (lmt_node_memory_state.nodesizes[i] > 0 && (node_type(i) <= max_node_type)) {
            ++used;
        }
    }
    return used;
}
int tex_free_node_count(void)
{
    int free = 0;
    for (int i = 1; i < max_chain_size; i++) {
        halfword p = lmt_node_memory_state.free_chain[i];
        while (p) {
            ++free;
            p = node_next(p);
        }
    }
    return free;
}

void tex_dump_node_mem(dumpstream f)
{
    dump_int(f, lmt_node_memory_state.nodes_data.allocated);
    dump_int(f, lmt_node_memory_state.nodes_data.top);
    dump_things(f, lmt_node_memory_state.nodes[0], (size_t) lmt_node_memory_state.nodes_data.top + 1);
    dump_things(f, lmt_node_memory_state.nodesizes[0], lmt_node_memory_state.nodes_data.top);
    dump_things(f, lmt_node_memory_state.free_chain[0], max_chain_size);
    dump_int(f, lmt_node_memory_state.nodes_data.ptr);
    dump_int(f, lmt_node_memory_state.reserved);
}

/*tex
    Node memory is (currently) also used for some stack related nodes. Using dedicated arrays instead
    makes sense but on the other hand this is the charm of \TEX. Variable nodes are no longer using
    the node pool so we don't need clever code to reclaim space. We have plenty anyway.
*/

void tex_undump_node_mem(dumpstream f) // todo: check allocation
{
    undump_int(f, lmt_node_memory_state.nodes_data.allocated);
    undump_int(f, lmt_node_memory_state.nodes_data.top);
    tex_initialize_node_mem();
    undump_things(f, lmt_node_memory_state.nodes[0], (size_t) lmt_node_memory_state.nodes_data.top + 1);
    undump_things(f, lmt_node_memory_state.nodesizes[0], (size_t) lmt_node_memory_state.nodes_data.top);
    undump_things(f, lmt_node_memory_state.free_chain[0], max_chain_size);
    undump_int(f, lmt_node_memory_state.nodes_data.ptr);
    undump_int(f, lmt_node_memory_state.reserved);
}

static halfword tex_aux_allocated_node(int s)
{
    int old = lmt_node_memory_state.nodes_data.top;
    int new = old + s;
    if (new > lmt_node_memory_state.nodes_data.allocated) {
        if (lmt_node_memory_state.nodes_data.allocated + lmt_node_memory_state.nodes_data.step <= lmt_node_memory_state.nodes_data.size) {
            memoryword *nodes = aux_reallocate_array(lmt_node_memory_state.nodes, sizeof(memoryword), lmt_node_memory_state.nodes_data.allocated + lmt_node_memory_state.nodes_data.step, reserved_node_slots);
            char *sizes = aux_reallocate_array(lmt_node_memory_state.nodesizes, sizeof(char), lmt_node_memory_state.nodes_data.allocated + lmt_node_memory_state.nodes_data.step, reserved_node_slots);
            if (nodes && sizes) {
                lmt_node_memory_state.nodes = nodes;
                lmt_node_memory_state.nodesizes = sizes;
                memset((void *) (nodes + lmt_node_memory_state.nodes_data.allocated), 0, (size_t) lmt_node_memory_state.nodes_data.step * sizeof(memoryword));
                memset((void *) (sizes + lmt_node_memory_state.nodes_data.allocated), 0, (size_t) lmt_node_memory_state.nodes_data.step * sizeof(char));
                lmt_node_memory_state.nodes_data.allocated += lmt_node_memory_state.nodes_data.step;
                lmt_run_memory_callback("node", 1);
            } else {
                lmt_run_memory_callback("node", 0);
                tex_overflow_error("node memory size", lmt_node_memory_state.nodes_data.size);
            }
        }
        if (new > lmt_node_memory_state.nodes_data.allocated) {
            tex_overflow_error("node memory size", lmt_node_memory_state.nodes_data.size);
        }
    }
    /* We allocate way larger than the maximum size. */
 // printf("old=%i  size=%i  new=%i\n",old,s,new);
    lmt_node_memory_state.nodesizes[old] = (char) s;
    lmt_node_memory_state.nodes_data.top = new;
    return old;
}

int tex_n_of_used_nodes(int counts[])
{
    int n = 0;
    for (int i = 0; i < max_node_type; i++) {
        counts[i] = 0;
    }
    for (int i = lmt_node_memory_state.nodes_data.top; i > lmt_node_memory_state.reserved; i--) {
        if (lmt_node_memory_state.nodesizes[i] > 0 && (node_type(i) <= max_node_type)) {
            counts[node_type(i)] += 1;
        }
    }
    for (int i = 0; i < max_node_type; i++) {
        n += counts[i];
    }
    return n;
}

/* needs checking as we take from the pool */

halfword tex_list_node_mem_usage(void)
{
    char *saved_varmem_sizes = aux_allocate_array(sizeof(char), lmt_node_memory_state.nodes_data.allocated, 1);
    if (saved_varmem_sizes) {
        halfword q = null;
        halfword p = null;
        memcpy(saved_varmem_sizes, lmt_node_memory_state.nodesizes, (size_t) lmt_node_memory_state.nodes_data.allocated);
        for (halfword i = lmt_node_memory_state.reserved + 1; i < (lmt_node_memory_state.nodes_data.allocated - 1); i++) {
            if (saved_varmem_sizes[i] > 0) {
                halfword j = tex_copy_node(i);
                if (p) {
                    node_next(p) = j;
                } else {
                    q = j;
                }
                p = j;
            }
        }
        aux_deallocate_array(saved_varmem_sizes);
        return q;
    } else {
        return null;
    }
}

/*
    Now comes some attribute stuff. We could have a fast allocator for them and a dedicated pool
    (actually for each node type I guess).
*/

extern void tex_change_attribute_register(halfword a, halfword id, halfword value)
{
    /* actually global should also kick in when we're not global yet */

 // if ((eq_value(id) != value) || (eq_level(id) != level_one && is_global(a))) {

    if (eq_value(id) != value) {
        if (is_global(a)) {
         // for (int i = (lmt_save_state.save_stack_data.ptr - 1); i >= 0; i--) {
         //     if (save_type(i) == level_boundary_save_type) {
         //         delete_attribute_reference(save_value_2(i)); /* watch the _2 here */
         //         save_value_2(i) = attribute_cache_disabled;  /* watch the _2 here */
         //     }
         // }
            int ptr = lmt_save_state.save_stack_data.ptr - 1;
            while (ptr >= 0) {
                if (save_type(ptr) == level_boundary_save_type) {
                    delete_attribute_reference(save_value_2(ptr)); /* watch the _2 here */
                    save_value_2(ptr) = attribute_cache_disabled;  /* watch the _2 here */
                    if (ptr) {
                        ptr = save_value(ptr);
                    } else {
                        break;
                    }
                } else {
                    /* something is wrong here */
                    break;
                }
            }
        } else {
            delete_attribute_reference(current_attribute_state);
        }
        set_current_attribute_state(attribute_cache_disabled);
    }
}

static inline halfword tex_aux_new_attribute_list_node(void)
{
    halfword r = tex_get_node(attribute_node_size);
    node_type(r) = attribute_node;
    node_subtype(r) = attribute_list_subtype;
    attribute_unset(r) = 0;
    attribute_count(r) = 0;
    return r;
}

static inline halfword tex_aux_new_attribute_node(halfword index, int value)
{
    halfword r = tex_get_node(attribute_node_size);
    node_type(r) = attribute_node;
    node_subtype(r) = attribute_value_subtype;
    attribute_index(r) = (quarterword) index;
    attribute_value(r) = value;
    return r;
}

static inline halfword tex_aux_copy_attribute_node(halfword n)
{
    halfword a = tex_get_node(attribute_node_size);
    memcpy((void *) (lmt_node_memory_state.nodes + a), (void *) (lmt_node_memory_state.nodes + n), (sizeof(memoryword) * attribute_node_size));
    return a;
}

halfword tex_copy_attribute_list(halfword a_old)
{
    if (a_old && a_old != attribute_cache_disabled) {
        halfword a_new = tex_aux_new_attribute_list_node();
        halfword p_old = a_old;
        halfword p_new = a_new;
        p_old = node_next(p_old);
        while (p_old) {
            halfword a = tex_copy_node(p_old);
            node_next(p_new) = a;
            p_new = a;
            p_old = node_next(p_old);
        }
        node_next(p_new) = null;
        return a_new;
    } else {
        return a_old;
    }
}

halfword tex_copy_attribute_list_set(halfword a_old, int index, int value)
{
    halfword a_new = tex_aux_new_attribute_list_node();
    halfword p_new = a_new;
    int done = 0;
    if (a_old && a_old != attribute_cache_disabled) {
        halfword p_old = node_next(a_old);
        while (p_old) {
            halfword i = attribute_index(p_old);
            if (! done && i >= index) {
                if (value != unused_attribute_value) {
                    halfword a = tex_aux_new_attribute_node(index, value);
                    node_next(p_new) = a;
                    p_new = a;
                }
                done = 1;
                if (i == index) {
                    goto CONTINUE;
                }
            }
       /* APPEND: */
            {
                halfword a = tex_aux_copy_attribute_node(p_old);
                node_next(p_new) = a;
                p_new = a;
            }
          CONTINUE:
            p_old = node_next(p_old);
        }
        node_next(p_new) = null;
    }
    if (! done && value != unused_attribute_value) {
        halfword b = tex_aux_new_attribute_node(index, value);
        node_next(p_new) = b;
    }
    return a_new;
}

static void tex_aux_update_attribute_cache(void)
{
    halfword p = tex_aux_new_attribute_list_node();
    set_current_attribute_state(p);
    for (int i = 0; i <= lmt_node_memory_state.max_used_attribute; i++) {
        int v = attribute_register(i);
        if (v > unused_attribute_value) {
            halfword r = tex_aux_new_attribute_node(i, v);
            node_next(p) = r;
            p = r;
        }
    }
    if (! node_next(current_attribute_state)) {
        tex_free_node(current_attribute_state, attribute_node_size);
        set_current_attribute_state(null);
    } else {
        add_attribute_reference(current_attribute_state);
    }
}

void tex_build_attribute_list(halfword target)
{
    if (lmt_node_memory_state.max_used_attribute >= 0) {
        if (! current_attribute_state || current_attribute_state == attribute_cache_disabled) {
            tex_aux_update_attribute_cache();
            if (! current_attribute_state) {
                return;
            }
        }
        add_attribute_reference(current_attribute_state);
        /*tex Checking for validity happens before the call; the subtype can be unset (yet). */
        node_attr(target) = current_attribute_state;
    }
}

halfword tex_current_attribute_list(void)
{
    if (lmt_node_memory_state.max_used_attribute >= 0) {
        if (! current_attribute_state || current_attribute_state == attribute_cache_disabled) {
            tex_aux_update_attribute_cache();
        }
        return current_attribute_state;
    } else {
        return null;
    }
}

/*tex

    There can be some gain in setting |attr_last_unset_enabled| but only when a lot of unsetting
    happens with rather long attribute lists, which actually is rare.

    One tricky aspect if attributes is that when we test for a list head being the same, we have
    the problem that freeing and (re)allocating can result in the same node address. Flushing in
    reverse order sort of prevents that.

*/

void tex_dereference_attribute_list(halfword a)
{
    if (a && a != attribute_cache_disabled) {
        if (node_type(a) == attribute_node && node_subtype(a) == attribute_list_subtype){
            if (attribute_count(a) > 0) {
                --attribute_count(a);
                if (attribute_count(a) == 0) {
                    if (a == current_attribute_state) {
                        set_current_attribute_state(attribute_cache_disabled);
                    }
                    {
                        int u = 0;
                        /* this works (different order) */
                        if (0) { /* chains are often short */
                            while (a) {
                                halfword n = node_next(a);
                                lmt_node_memory_state.nodesizes[a] = 0;
                                node_next(a) = lmt_node_memory_state.free_chain[attribute_node_size];
                                lmt_node_memory_state.free_chain[attribute_node_size] = a;
                                ++u;
                                a = n;
                            }
                        } else {
                            /* this doesn't always (which is weird) */
                            halfword h = a;
                            halfword t = a;
                            while (a) {
                                lmt_node_memory_state.nodesizes[a] = 0;
                                ++u;
                                t = a;
                                a = node_next(a);
                            }
                            node_next(t) = lmt_node_memory_state.free_chain[attribute_node_size];
                            lmt_node_memory_state.free_chain[attribute_node_size] = h;
                        }
                        /* */
                        lmt_node_memory_state.nodes_data.ptr -= u * attribute_node_size;
                    }
                }
            } else {
                tex_formatted_error("nodes", "zero referenced attribute list %i", a);
            }
        } else {
            tex_formatted_error("nodes", "trying to delete an attribute reference of a non attribute list node %i (%i)", a, node_type(a));
        }
    }
}

/*tex
    Here |p| is an attr list head, or zero. This one works on a copy, so we can overwrite a value!
*/

halfword tex_patch_attribute_list(halfword list, int index, int value)
{
    if (list == attribute_cache_disabled) {
        return list;
    } else if (list) {
        halfword current = node_next(list);
        halfword previous = list;
        while (current) {
            int i = attribute_index(current);
            if (i == index) {
                /*tex Replace: */
                attribute_value(current) = value;
                return list;
            } else if (i > index) {
                /*tex Prepend: */
                halfword r = tex_aux_new_attribute_node(index, value);
                node_next(previous) = r;
                node_next(r) = current;
                return list;
            } else {
                previous = current;
                current = node_next(current);
            }
        }
        {
            /*tex Append: */
            halfword r = tex_aux_new_attribute_node(index, value);
            node_next(r) = node_next(previous);
            node_next(previous) = r;
        }
    } else {
        /*tex Watch out, we don't set a ref count, this branch is not seen anyway. */
        halfword r = tex_aux_new_attribute_node(index, value);
        list = tex_aux_new_attribute_list_node();
        node_next(list) = r;
    }
    return list;
}

halfword tex_merge_attribute_list(halfword first, halfword second)
{
    if (first) {
        if (second) {
            halfword list = tex_copy_attribute_list(first);
            halfword current = node_next(second);
            while (current) {
                list = tex_patch_attribute_list(list, attribute_index(current), attribute_value(current));
                current = node_next(current);
            }
            add_attribute_reference(list);
            return list;
        } else {
            add_attribute_reference(first);
            return first;
        }
    } else {
        if (second) {
            add_attribute_reference(second);
            return second;
        } else {
            return null;
        }
    }
}

/* todo: combine set and unset */

void tex_set_attribute(halfword target, int index, int value)
{
    /*tex Not all nodes can have an attribute list. */
    if (tex_nodetype_has_attributes(node_type(target))) {
        if (value == unused_attribute_value) {
            tex_unset_attribute(target, index, value);
        } else {
            /*tex If we have no list, we create one and quit. */
            halfword a = node_attr(target);
            /* needs checking: can we get an empty one here indeed, the |node_next| test case ... */
            if (a) {
                halfword p = node_next(a);
                while (p) {
                    int i = attribute_index(p);
                    if (i == index) {
                        if (attribute_value(p) == value) {
                            return;
                        } else {
                            break;
                        }
                    } else if (i > index) {
                        break;
                    } else {
                        p = node_next(p);
                    }
                }
         //     p = tex_copy_attribute_list_set(a, index, value);
         //     tex_attach_attribute_list_attribute(target, p);
         // } else {
         //     halfword p = tex_copy_attribute_list_set(null, index, value);
         //     tex_attach_attribute_list_attribute(target, p);
         // }
            }
            a = tex_copy_attribute_list_set(a, index, value);
            tex_attach_attribute_list_attribute(target, a);
        }
    }
}

int tex_unset_attribute(halfword target, int index, int value)
{
    if (tex_nodetype_has_attributes(node_type(target))) {
        halfword p = node_attr(target);
        if (p) {
            halfword c = node_next(p);
            while (c) {
                halfword i = attribute_index(c);
                if (i == index) {
                    halfword v = attribute_value(c);
                    if (v != value) {
                        halfword l = tex_copy_attribute_list_set(p, index, value);
                        tex_attach_attribute_list_attribute(target, l);
                    }
                    return v;
                } else if (i > index) {
                    return unused_attribute_value;
                }
                c = node_next(c);
            }
        }
    }
    return unused_attribute_value;
}

void tex_unset_attributes(halfword first, halfword last, int index)
{
    halfword a = null;
    halfword q = null;
    halfword n = first;
    while (n) {
        if (tex_nodetype_has_attributes(node_type(n))) {
            halfword p = node_attr(n);
            if (p) {
                if (p == q) {
                    tex_attach_attribute_list_attribute(n, a);
                } else {
                    halfword c = node_next(p);
                    while (c) {
                        halfword i = attribute_index(c);
                        if (i == index) {
                            q = p;
                            a = tex_copy_attribute_list_set(p, index, unused_attribute_value); /* check */
                            tex_attach_attribute_list_attribute(n, a);
                            break;
                        } else if (i > index) {
                            break;
                        }
                        c = node_next(c);
                    }
                }
            }
        }
        if (n == last) {
            break;
        } else {
            n = node_next(n);
        }
    }
}

int tex_has_attribute(halfword n, int index, int value)
{
    if (tex_nodetype_has_attributes(node_type(n))) {
        halfword p = node_attr(n);
        if (p) {
            p = node_next(p);
            while (p) {
                if (attribute_index(p) == index) {
                    int v = attribute_value(p);
                    if (value == v || value == unused_attribute_value) {
                        return v;
                    } else {
                        return unused_attribute_value;
                    }
                } else if (attribute_index(p) > index) {
                    return unused_attribute_value;
                }
                p = node_next(p);
            }
        }
    }
    return unused_attribute_value;
}

/*tex
    Because we have more detail available we provide node names and show a space when we have one.
    The disc nodes are also more granular. I might drop the font in showing glyph nodes. A previous
    version used full node types inside brackets but we now collapse the node types and use only
    the first character of the type. Eventually I might come up with some variants.
 */

void tex_print_short_node_contents(halfword p)
{
    int collapsing = 0;
    while (p) {
        switch (node_type(p)) {
            case rule_node:
                if (collapsing) { tex_print_char(']'); collapsing = 0; }
                tex_print_char('|');
                break;
            case glue_node:
                switch (node_subtype(p)) {
                    case space_skip_glue:
                    case xspace_skip_glue:
                    case zero_space_skip_glue:
                        if (collapsing) { tex_print_char(']'); collapsing = 0; }
                        tex_print_char(' ');
                        break;
                    default:
                        goto DEFAULT;
                }
                break;
            case math_node:
                if (collapsing) { tex_print_char(']'); collapsing = 0; }
                tex_print_char('$');
                break;
            case disc_node:
                if (collapsing) { tex_print_char(']'); collapsing = 0; }
                tex_print_str("[[");
                tex_print_short_node_contents(disc_pre_break_head(p));
                tex_print_str("][");
                tex_print_short_node_contents(disc_post_break_head(p));
                tex_print_str("][");
                tex_print_short_node_contents(disc_no_break_head(p));
                tex_print_str("]]");
                break;
            case dir_node:
                if (collapsing) { tex_print_char(']'); collapsing = 0; }
                if (node_subtype(p) == cancel_dir_subtype) {
                    tex_print_str(" >");
                } else {
                    tex_print_str(dir_direction(p) ? "<r2l " : "<l2r ");
                }
                break;
            case glyph_node:
                if (collapsing) { tex_print_char(']'); collapsing = 0; }
                if (glyph_font(p) != lmt_print_state.font_in_short_display) {
                    tex_print_font_identifier(glyph_font(p));
                    tex_print_char(' ');
                    lmt_print_state.font_in_short_display = glyph_font(p);
                }
                tex_print_tex_str(glyph_character(p));
                break;
            case par_node:
                if (collapsing) { tex_print_char(']'); collapsing = 0; }
                tex_print_str(par_direction(p) ? "<r2l p>" : "<l2r p>");
                break;
            default:
              DEFAULT:
                if (! collapsing) {
                    tex_print_char('[');
                    collapsing = 1;
                }
                tex_print_char(lmt_interface.node_data[node_type(p)].name[0]);
                break;
        }
        p = node_next(p);
    }
    if (collapsing) {
        tex_print_char(']');
    }
}

/*tex

    Now we are ready for |show_node_list| itself. This procedure has been written to be \quote
    {extra robust} in the sense that it should not crash or get into a loop even if the data
    structures have been messed up by bugs in the rest of the program. You can safely call its
    parent routine |show_box(p)| for arbitrary values of |p| when you are debugging \TEX. However,
    in the presence of bad data, the procedure may fetch a |memoryword| whose variant is different
    from the way it was stored; for example, it might try to read |mem[p].hh| when |mem[p]|
    contains a scaled integer, if |p| is a pointer that has been clobbered or chosen at random.

*/

void tex_print_node_list(halfword p, const char *what, int threshold, int max)
{
    if (p) {
        if (what) {
            tex_append_char('.');
            tex_append_char('.');
            tex_print_levels();
            tex_print_current_string();
            tex_print_str_esc(what);
        } else {
            /*tex This happens in math. */
        }
        tex_append_char('.');
        tex_append_char('.');
        tex_show_node_list(p, threshold, max); // show_box_depth_par, show_box_breadth_par
        tex_flush_char();
        tex_flush_char();
        if (what) {
            tex_flush_char();
            tex_flush_char();
        }
    }
}

/*tex

    Print a node list symbolically. This one is adaped to the fact that we have a bit more
    granularity in subtypes and some more fields. It is therefore not compatible with traditional
    \TEX. This is work in progress. I will also normalize some subtype names so ...

*/

static void tex_aux_show_attr_list(halfword p)
{
     p = node_attr(p);
     if (p) {
        int callback_id = lmt_callback_defined(get_attribute_callback);
        if (tracing_nodes_par > 1) {
            tex_print_format("<%i#%i>", p, attribute_count(p));
        }
        tex_print_char('[');
        p = node_next(p);
        while (p) {
            halfword k = attribute_index(p);
            halfword v = attribute_value(p);
            if (callback_id) {
                strnumber u = tex_save_cur_string();
                char *ks = NULL;
                char *vs = NULL;
                lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dd->RR", k, v, &ks, &vs);
                tex_restore_cur_string(u);
                if (ks) {
                    tex_print_str(ks);
                    lmt_memory_free(ks);
                } else {
                    tex_print_int(k);
                }
                tex_print_char('=');
                if (vs) {
                    tex_print_str(vs);
                    lmt_memory_free(vs);
                } else {
                    tex_print_int(v);
                }
            } else {
                tex_print_format("%i=%i", k, v);
            };
            p = node_next(p);
            if (p) {
                tex_print_char(',');
            }
        }
        tex_print_char(']');
    }
}

void tex_print_name(halfword n, const char* what)
{
    tex_print_str_esc(what);
    if (tracing_nodes_par > 0) {
        tex_print_format("<%i>", n);
    }
}

static void tex_aux_print_subtype_and_attributes_str(halfword p, const char *n)
{
    if (show_node_details_par > 0) {
        tex_print_format("[%s]",n);
    }
    if (show_node_details_par > 1 && tex_nodetype_has_attributes(node_type(p))) {
        tex_aux_show_attr_list(p);
    }
}

void tex_print_extended_subtype(halfword p, quarterword s)
{
    halfword st = s;
    switch (p ? node_type(p) : simple_noad) {
        case hlist_node:
            if (s > noad_class_list_base) {
                st -= noad_class_list_base;
            }
        case simple_noad:
        case math_char_node:
            {
                int callback_id = lmt_callback_defined(get_noad_class_callback);
                if (callback_id) {
                    strnumber u = tex_save_cur_string();
                    char *v = NULL;
                    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "d->R", st, &v);
                    tex_restore_cur_string(u);
                    if (v) {
                        if (p && node_type(p) == hlist_node) {
                            tex_print_str("math");
                        }
                        tex_print_str(v);
                        lmt_memory_free(v);
                        break;
                    }
                }
                /* fall through */
            }
            break;
        default:
            tex_print_int(s);
            break;
    }
}

static void tex_print_subtype_and_attributes_info(halfword p, quarterword s, node_info *data)
{
    if (show_node_details_par > 0) {
        tex_print_char('[');
        if (data && data->subtypes && s >= data->first && s <= data->last) {
            tex_print_str(data->subtypes[s].name);
        } else {
            tex_print_extended_subtype(p, s);
        }
        tex_print_char(']');
    }
    if (show_node_details_par > 1 && tex_nodetype_has_attributes(node_type(p))) {
        tex_aux_show_attr_list(p);
    }
}

static void tex_print_node_and_details(halfword p)
{
    halfword type = node_type(p);
    quarterword subtype = node_subtype(p);
    tex_print_name(p, lmt_interface.node_data[type].name);
    switch (type) {
        case temp_node:
        case whatsit_node:
            return;
    }
    tex_print_subtype_and_attributes_info(p, subtype, &lmt_interface.node_data[type]);
}

static void tex_aux_print_subtype_and_attributes_int(halfword p, halfword n)
{
    if (show_node_details_par > 0) { \
        tex_print_format("[%i]", n);
    }
    if (show_node_details_par > 1 && tex_nodetype_has_attributes(node_type(p))) {
        tex_aux_show_attr_list(p);
    }
}

const char *tex_aux_subtype_str(halfword n)
{
    if (n) {
        node_info *data = &lmt_interface.node_data[node_type(n)];
        if (data && data->subtypes && node_subtype(n) >= data->first && node_subtype(n) <= data->last) {
            return data->subtypes[node_subtype(n)].name;
        }
    }
    return "";
}

/*tex

    We're not downward compatible here and it might even evolve a bit (and maybe I'll add a
    compatibility mode too). We have way more information and plenty of log space so there is no
    need to be compact. Consider it work in progress.

    I admit that there is some self interest here in adding more detail. At some point (around
    ctx 2019) I needed to see attribute values in the trace so I added that option which in turn
    made me reformat the output a bit. Of course it makes sense to have the whole show be a
    callback (and I might actually do that) but on the other hand it's so integral to \TEX\ that
    it doesn't add much and in all the years that \LUATEX| is now around I never really needed
    it anyway.

    One option is to go completely |\node[key=value,key={value,value}]| here as that can be easily
    parsed. It's to be decided.

    What is the string pool char data used for here?

    Per version 2.09.22 we use the values from the node definitions which is more consistent and
    also makes the binary somewhat smaller. It's all in the details. It's a typical example of
    a change done when we're stable for a while (as it influences tracing).

*/

static void tex_print_specnode(halfword v, int unit) /* for now local */
{
    if (tracing_nodes_par > 2) {
        tex_print_format("<%i>", v);
    }
    tex_print_spec(v, unit);
}

void tex_aux_show_dictionary(halfword p, halfword properties, halfword group, halfword index,halfword font, halfword character)
{
    int callback_id = lmt_callback_defined(get_math_dictionary_callback);
    if (callback_id) {
        strnumber u = tex_save_cur_string();
        char *s = NULL;
        lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "Nddddd->R", p, properties, group, index, font, character, &s);
        tex_restore_cur_string(u);
        if (s) {
            tex_print_format(", %s", s);
            lmt_memory_free(s);
            return;
        }
    }
    if (properties) {
        tex_print_format(", properties %x", properties);
    }
    if (group) {
        tex_print_format(", group %x", group);
    }
    if (index) {
        tex_print_format(", index %x", index);
    }
}

void tex_show_node_list(halfword p, int threshold, int max)
{
    if ((int) lmt_string_pool_state.string_temp_top > threshold) {
        if (p > null) {
            /*tex Indicate that there's been some truncation. */
            tex_print_format("[tracing depth %i reached]", threshold);
        }
        return;
    } else {
        /*tex The number of items already printed at this level: */
        int n = 0;
        if (max <= 0) {
            max = 5;
        }
        while (p) {
            tex_print_levels();
            tex_print_current_string();
            ++n;
            if (n > max) {
                /*tex Time to stop. */
                halfword t = tex_tail_of_node_list(p);
                if (t == p) {
                    /*tex We've showed the whole list. */
                    return;
                } else if (p == node_prev(t)) {
                 // /*tex We're just before the end. */
                } else {
                    tex_print_format("[tracing breadth %i reached]", max);
                    return;
                }
            }
            tex_print_node_and_details(p);
            switch (node_type(p)) {
                case glyph_node:
                    if (show_node_details_par > 0) {
                        scaledwhd whd = tex_char_whd_from_glyph(p);
                        if (glyph_protected(p)) {
                            tex_print_str(", protected");
                        }
                        /* effective */
                        if (whd.wd) {
                            tex_print_format(", wd %p", whd.wd);
                        }
                        if (whd.ht) {
                            tex_print_format(", ht %p", whd.ht);
                        }
                        if (whd.dp) {
                            tex_print_format(", dp %p", whd.dp);
                        }
                        if (whd.ic) {
                            tex_print_format(", ic %p", whd.ic);
                        }
                        /* */
                        if (get_glyph_language(p)) {
                            tex_print_format(", language (n=%i,l=%i,r=%i)", get_glyph_language(p), get_glyph_lhmin(p), get_glyph_rhmin(p));
                        }
                        if (get_glyph_script(p)) {
                            tex_print_format(", script %i", get_glyph_script(p));
                        }
                        if (get_glyph_hyphenate(p)) {
                            tex_print_format(", hyphenationmode %x", get_glyph_hyphenate(p));
                        }
                        if (glyph_x_offset(p)) {
                            tex_print_format(", xoffset %p", glyph_x_offset(p));
                        }
                        if (glyph_y_offset(p)) {
                            tex_print_format(", yoffset %p", glyph_y_offset(p));
                        }
                        if (glyph_left(p)) {
                            tex_print_format(", left %p", glyph_left(p));
                        }
                        if (glyph_right(p)) {
                            tex_print_format(", right %p", glyph_right(p));
                        }
                        if (glyph_raise(p)) {
                            tex_print_format(", raise %p", glyph_raise(p));
                        }
                        if (glyph_expansion(p)) {
                            tex_print_format(", expansion %i", glyph_expansion(p));
                        }
                        if (glyph_scale(p) && glyph_scale(p) != scaling_factor) {
                            tex_print_format(", scale %i", glyph_scale(p));
                        }
                        if (glyph_x_scale(p) && glyph_x_scale(p) != scaling_factor) {
                            tex_print_format(", xscale %i", glyph_x_scale(p));
                        }
                        if (glyph_y_scale(p) && glyph_y_scale(p) != scaling_factor) {
                            tex_print_format(", yscale %i", glyph_y_scale(p));
                        }
                        if (glyph_data(p)) {
                            tex_print_format(", data %i", glyph_data(p));
                        }
                        if (glyph_state(p)) {
                            tex_print_format(", state %i", glyph_state(p));
                        }
                        if (glyph_options(p)) {
                            tex_print_format(", options %x", glyph_options(p));
                        }
                        if (glyph_discpart(p)) {
                            tex_print_format(", discpart %i", glyph_discpart(p));
                        }
                        tex_aux_show_dictionary(p, glyph_properties(p), glyph_group(p), glyph_index(p), glyph_font(p), glyph_character(p));
                    }
                    tex_print_format(", font %F, glyph %U, %u", glyph_font(p), glyph_character(p), glyph_character(p));
                    break;
                case hlist_node:
                case vlist_node:
                case unset_node:
                    /*tex Display box |p|. */
                    if (box_width(p)) {
                        tex_print_format(", width %p", box_width(p));
                    }
                    if (box_height(p)) {
                        tex_print_format(", height %p", box_height(p));
                    }
                    if (box_depth(p)) {
                        tex_print_format(", depth %p", box_depth(p));
                    }
                    if (node_type(p) == unset_node) {
                        /*tex Display special fields of the unset node |p|. */
                        if (box_span_count(p)) {
                            tex_print_format(", columns %i", box_span_count(p) + 1);
                        }
                        if (box_glue_stretch(p)) {
                            tex_print_str(", stretch ");
                            tex_print_glue(box_glue_stretch(p), box_glue_order(p), no_unit);
                        }
                        if (box_glue_shrink(p)) {
                            tex_print_str(", shrink ");
                            tex_print_glue(box_glue_shrink(p), box_glue_sign(p), no_unit);
                        }
                    } else {
                        /*tex

                            Display the value of |glue_set(p)|. The code will have to change in
                            this place if |glue_ratio| is a structured type instead of an
                            ordinary |real|. Note that this routine should avoid arithmetic
                            errors even if the |glue_set| field holds an arbitrary random value.
                            The following code assumes that a properly formed nonzero |real|
                            number has absolute value $2^{20}$ or more when it is regarded as an
                            integer; this precaution was adequate to prevent floating point
                            underflow on the author's computer.

                        */
                        double g = (double) (box_glue_set(p));
                        if ((g != 0.0) && (box_glue_sign(p) != normal_glue_sign)) {
                            tex_print_str(", glue "); /*tex This was |glue set|. */
                            if (box_glue_sign(p) == shrinking_glue_sign) {
                                tex_print_str("- ");
                            }
                            if (g > 20000.0 || g < -20000.0) {
                                if (g > 0.0) {
                                    tex_print_char('>');
                                } else {
                                    tex_print_str("< -");
                                }
                                tex_print_glue(20000 * unity, box_glue_order(p), no_unit);
                            } else {
                                tex_print_glue((scaled) glueround(unity *g), box_glue_order(p), no_unit);
                            }
                        }
                        if (box_shift_amount(p) != 0) {
                            tex_print_format(", shifted %p", box_shift_amount(p));
                        }
                        if (valid_direction(box_direction(p))) {
                            tex_print_format(", direction %2", box_direction(p));
                        }
                        if (box_geometry(p)) {
                            tex_print_format(", geometry %x", box_geometry(p));
                            if (tex_has_box_geometry(p, orientation_geometry)) {
                                tex_print_format(", orientation %x", box_orientation(p));
                            }
                            if (tex_has_box_geometry(p, offset_geometry)) {
                                tex_print_format(", offset(%p,%p)", box_x_offset(p), box_y_offset(p));
                            }
                            if (tex_has_box_geometry(p, anchor_geometry)) {
                                if (box_anchor(p)) {
                                    tex_print_format(", anchor %x", box_anchor(p));
                                }
                                if (box_source_anchor(p)) {
                                    tex_print_format(", source %i", box_source_anchor(p));
                                }
                                if (box_target_anchor(p)) {
                                    tex_print_format(", target %i", box_target_anchor(p));
                                }
                            }
                        }
                        if (box_index(p)) {
                            tex_print_format(", index %i", box_index(p));
                        }
                        if (box_package_state(p)) {
                            tex_print_format(", state %i", box_package_state(p));
                        }
                    }
                    tex_print_node_list(box_pre_adjusted(p), "preadjusted", threshold, max);
                    tex_print_node_list(box_pre_migrated(p), "premigrated", threshold, max);
                    tex_print_node_list(box_list(p), "list", threshold, max);
                    tex_print_node_list(box_post_migrated(p), "postmigrated", threshold, max);
                    tex_print_node_list(box_post_adjusted(p), "postadjusted", threshold, max);
                    break;
                case rule_node:
                    /*tex Display rule |p|. */
                    if (rule_width(p)) {
                        tex_print_format(", width %R", rule_width(p));
                    }
                    if (rule_height(p)) {
                        tex_print_format(", height %R", rule_height(p));
                    }
                    if (rule_depth(p)) {
                        tex_print_format(", depth %R", rule_depth(p));
                    }
                    switch (node_subtype(p)) {
                        case virtual_rule_subtype:
                            if (rule_virtual_width(p)) {
                                tex_print_format(", virtual width %R", rule_virtual_width(p));
                            }
                            if (rule_virtual_height(p)) {
                                tex_print_format(", virtual height %R", rule_virtual_height(p));
                            }
                            if (rule_virtual_depth(p)) {
                                tex_print_format(", virtual depth %R", rule_virtual_depth(p));
                            }
                            break;
                        case strut_rule_subtype:
                            if (rule_strut_font(p)) {
                                if (rule_strut_font(p) >= rule_font_fam_offset) {
                                    tex_print_format(", family %i", rule_strut_font(p) - rule_font_fam_offset);
                                } else {
                                    tex_print_format(", font %F", rule_strut_font(p) < 0 ? 0 : rule_strut_font(p));
                                }
                            }
                            if (rule_strut_character(p)) {
                                tex_print_format(", character %U", rule_strut_character(p));
                            }
                            /* fall through */
                        default:
                            if (rule_left(p)) {
                                tex_print_format(", left / top %R", rule_left(p));
                            }
                            if (rule_right(p)) {
                                tex_print_format(", right / bottom %R", rule_right(p));
                            }
                            break;
                    }
                    if (rule_x_offset(p)) {
                        tex_print_format(", xoffset %R", rule_x_offset(p));
                    }
                    if (rule_y_offset(p)) {
                        tex_print_format(", yoffset %R", rule_y_offset(p));
                    }
                    if (rule_data(p)) {
                        tex_print_format(", data %R", rule_data(p));
                    }
                    break;
                case insert_node:
                    /*tex Display insertion |p|. The natural size is the sum of height and depth. */
                    tex_print_format(
                        ", index %i, total height %p, max depth %p, split glue (",
                        insert_index(p),
                        insert_total_height(p),
                        insert_max_depth(p)
                    );
                    tex_print_specnode(insert_split_top(p), no_unit); /* todo: formatter for specnode but what CHAR to use */
                    tex_print_format(
                     "), float cost %i",
                        insert_float_cost(p)
                    );
                    tex_print_node_list(insert_list(p), "list", threshold, max);
                    break;
                case dir_node:
                    tex_print_format(", direction %2, level %i", dir_direction(p), dir_level(p));
                    break;
                case par_node:
                    {
                        halfword v;
                        /*tex We're already past processing so we only show the stored values. */
                        tex_print_format(", direction %2", par_direction(p));
                        if (node_subtype(p) == vmode_par_par_subtype) {
                            if (tex_par_state_is_set(p, par_par_shape_code)               ) { v = par_par_shape(p)               ; if (v)                     { tex_print_str(", parshape * ");              } }
                            if (tex_par_state_is_set(p, par_inter_line_penalties_code)    ) { v = par_inter_line_penalties(p)    ; if (v)                     { tex_print_str(", interlinepenalties * ");    } }
                            if (tex_par_state_is_set(p, par_club_penalties_code)          ) { v = par_club_penalties(p)          ; if (v)                     { tex_print_str(", clubpenalties * ");         } }
                            if (tex_par_state_is_set(p, par_widow_penalties_code)         ) { v = par_widow_penalties(p)         ; if (v)                     { tex_print_str(", widowpenalties * ");        } }
                            if (tex_par_state_is_set(p, par_display_widow_penalties_code) ) { v = par_display_widow_penalties(p) ; if (v)                     { tex_print_str(", displaywidowpenalties * "); } }
                            if (tex_par_state_is_set(p, par_broken_penalties_code)        ) { v = par_broken_penalties(p)        ; if (v)                     { tex_print_str(", displaywidowpenalties * "); } }
                            if (tex_par_state_is_set(p, par_orphan_penalties_code)        ) { v = par_orphan_penalties(p)        ; if (v)                     { tex_print_str(", orphanpenalties * ");       } }
                            if (tex_par_state_is_set(p, par_fitness_classes_code)         ) { v = par_fitness_classes(p)         ; if (v)                     { tex_print_str(", fitnessdemerits * ");       } }
                            if (tex_par_state_is_set(p, par_adjacent_demerits_code)       ) { v = par_adjacent_demerits(p)       ; if (v)                     { tex_print_str(", adjacentdemerits * ");      } }
                            if (tex_par_state_is_set(p, par_orphan_line_factors_code)     ) { v = par_orphan_line_factors(p)     ; if (v)                     { tex_print_str(", orphanlinefactors * ");     } }
                            if (tex_par_state_is_set(p, par_hang_indent_code)             ) { v = par_hang_indent(p)             ; if (v)                     { tex_print_str(", hangindent ");              tex_print_dimension(v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_hang_after_code)              ) { v = par_hang_after(p)              ; if (v)                     { tex_print_str(", hangafter ");               tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_hsize_code)                   ) { v = par_hsize(p)                   ; if (v)                     { tex_print_str(", hsize ");                   tex_print_dimension(v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_right_skip_code)              ) { v = par_right_skip(p)              ; if (! tex_glue_is_zero(v)) { tex_print_str(", rightskip ");               tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_left_skip_code)               ) { v = par_left_skip(p)               ; if (! tex_glue_is_zero(v)) { tex_print_str(", leftskip ");                tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_last_line_fit_code)           ) { v = par_last_line_fit(p)           ; if (v)                     { tex_print_str(", lastlinefit ");             tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_pre_tolerance_code)           ) { v = par_pre_tolerance(p)           ; if (v)                     { tex_print_str(", pretolerance ");            tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_tolerance_code)               ) { v = par_tolerance(p)               ; if (v)                     { tex_print_str(", tolerance ");               tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_looseness_code)               ) { v = par_looseness(p)               ; if (v)                     { tex_print_str(", looseness ");               tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_adjust_spacing_code)          ) { v = par_adjust_spacing(p)          ; if (v)                     { tex_print_str(", adjustspacing ");           tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_adj_demerits_code)            ) { v = par_adj_demerits(p)            ; if (v)                     { tex_print_str(", adjdemerits ");             tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_protrude_chars_code)          ) { v = par_protrude_chars(p)          ; if (v)                     { tex_print_str(", protrudechars ");           tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_line_penalty_code)            ) { v = par_line_penalty(p)            ; if (v)                     { tex_print_str(", linepenalty ");             tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_double_hyphen_demerits_code)  ) { v = par_double_hyphen_demerits(p)  ; if (v)                     { tex_print_str(", doublehyphendemerits ");    tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_final_hyphen_demerits_code)   ) { v = par_final_hyphen_demerits(p)   ; if (v)                     { tex_print_str(", finalhyphendemerits ");     tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_inter_line_penalty_code)      ) { v = par_inter_line_penalty(p)      ; if (v)                     { tex_print_str(", interlinepenalty ");        tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_club_penalty_code)            ) { v = par_club_penalty(p)            ; if (v)                     { tex_print_str(", clubpenalty ");             tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_widow_penalty_code)           ) { v = par_widow_penalty(p)           ; if (v)                     { tex_print_str(", widowpenalty ");            tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_display_widow_penalty_code)   ) { v = par_display_widow_penalty(p)   ; if (v)                     { tex_print_str(", displaywidowpenalty ");     tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_toddler_penalties_code)       ) { v = par_toddler_penalties(p)       ; if (v)                     { tex_print_str(", toddlerpenalties ");        } }
                            if (tex_par_state_is_set(p, par_left_twin_demerits_code)      ) { v = par_left_twin_demerits(p)      ; if (v)                     { tex_print_str(", lefttwindemerits ");        tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_right_twin_demerits_code)     ) { v = par_right_twin_demerits(p)     ; if (v)                     { tex_print_str(", righttwindemerits ");       tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_single_line_penalty_code)     ) { v = par_single_line_penalty(p)     ; if (v)                     { tex_print_str(", singlelinepenalty ");       tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_hyphen_penalty_code)          ) { v = par_hyphen_penalty(p)          ; if (v)                     { tex_print_str(", hyphenlinepenalty ");       tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_broken_penalty_code)          ) { v = par_broken_penalty(p)          ; if (v)                     { tex_print_str(", brokenpenalty ");           tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_emergency_stretch_code)       ) { v = par_emergency_stretch(p)       ; if (v)                     { tex_print_str(", emergencystretch ");        tex_print_dimension(v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_par_indent_code)              ) { v = par_par_indent(p)              ; if (v)                     { tex_print_str(", parindent ");               tex_print_dimension(v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_par_fill_left_skip_code)      ) { v = par_par_fill_left_skip(p)      ; if (! tex_glue_is_zero(v)) { tex_print_str(", parfilleftskip ");          tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_par_fill_right_skip_code)     ) { v = par_par_fill_right_skip(p)     ; if (! tex_glue_is_zero(v)) { tex_print_str(", parfillskip ");             tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_par_init_left_skip_code)      ) { v = par_par_init_left_skip(p)      ; if (! tex_glue_is_zero(v)) { tex_print_str(", parinitleftskip ");         tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_par_init_right_skip_code)     ) { v = par_par_init_right_skip(p)     ; if (! tex_glue_is_zero(v)) { tex_print_str(", parinitrightskip ");        tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_emergency_left_skip_code)     ) { v = par_emergency_left_skip(p)     ; if (! tex_glue_is_zero(v)) { tex_print_str(", emergencyleftskip ");       tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_emergency_right_skip_code)    ) { v = par_emergency_right_skip(p)    ; if (! tex_glue_is_zero(v)) { tex_print_str(", emergencyrightskip ");      tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_baseline_skip_code)           ) { v = par_baseline_skip(p)           ; if (! tex_glue_is_zero(v)) { tex_print_str(", baselineskip ");            tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_line_skip_code)               ) { v = par_line_skip(p)               ; if (! tex_glue_is_zero(v)) { tex_print_str(", lineskip ");                tex_print_specnode (v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_line_skip_limit_code)         ) { v = par_line_skip_limit(p)         ; if (v)                     { tex_print_str(", lineskiplimt ");            tex_print_dimension(v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_adjust_spacing_step_code)     ) { v = par_adjust_spacing_step(p)     ; if (v > 0)                 { tex_print_str(", adjustspacingstep ");       tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_adjust_spacing_shrink_code)   ) { v = par_adjust_spacing_shrink(p)   ; if (v > 0)                 { tex_print_str(", adjustspacingshrink ");     tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_adjust_spacing_stretch_code)  ) { v = par_adjust_spacing_stretch(p)  ; if (v > 0)                 { tex_print_str(", adjustspacingstretch ");    tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_hyphenation_mode_code)        ) { v = par_hyphenation_mode(p)        ; if (v > 0)                 { tex_print_str(", hyphenationmode ");         tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_shaping_penalties_mode_code)  ) { v = par_shaping_penalties_mode(p)  ; if (v > 0)                 { tex_print_str(", shapingpenaltiesmode ");    tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_shaping_penalty_code)         ) { v = par_shaping_penalty(p)         ; if (v > 0)                 { tex_print_str(", shapingpenalty ");          tex_print_int      (v);          } }
                            if (tex_par_state_is_set(p, par_emergency_extra_stretch_code) ) { v = par_emergency_extra_stretch(p) ; if (v)                     { tex_print_str(", emergencyextrastretch ");   tex_print_dimension(v, pt_unit); } }
                            if (tex_par_state_is_set(p, par_par_passes_code)              ) { v = par_par_passes(p)              ; if (v)                     { tex_print_str(", parpasses * ");                                              } }
                            if (tex_par_state_is_set(p, par_line_break_checks_code)       ) { v = par_line_break_checks(p)       ; if (v)                     { tex_print_str(", linebreakchecks ");         tex_print_int      (v);          } }
                        }
                        /* local boxes */
                        v = tex_get_local_left_width(p)  ; if (v) { tex_print_format(", leftboxwidth %p", v); }
                        v = tex_get_local_right_width(p) ; if (v) { tex_print_format(", rightboxwidth %p", v); }
                        tex_print_node_list(par_box_left(p), "leftbox", threshold, max);
                        tex_print_node_list(par_box_right(p), "rightbox", threshold, max);
                        tex_print_node_list(par_box_middle(p), "middlebox", threshold, max);
                    }
                    break;
                case boundary_node:
                    if (boundary_data(p)) {
                        tex_print_format(", data %i", boundary_data(p));
                    }
                    break;
                case whatsit_node:
                    {
                        int callback_id = lmt_callback_defined(show_whatsit_callback);
                        /*tex we always print this */
                        if (callback_id) {
                            strnumber u = tex_save_cur_string();
                            char *s = NULL;
                            lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "Nd->S", p, 1, &s);
                            tex_restore_cur_string(u);
                            if (s) {
                                tex_aux_print_subtype_and_attributes_str(p, s);
                                lmt_memory_free(s);
                            } else {
                                tex_aux_print_subtype_and_attributes_int(p, node_subtype(p));
                            }
                        } else {
                            tex_aux_print_subtype_and_attributes_int(p, node_subtype(p));
                        }
                        /*tex but optionally there can be more */
                        if (callback_id) {
                            int l = lmt_string_pool_state.string_temp_top / 2;
                            strnumber u = tex_save_cur_string();
                            /*tex Todo: the tracing needs checking. */
                            lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "Nddddd->", p, 2, l, (tracing_levels_par & (tracing_levels_group | tracing_levels_input)), cur_level, lmt_input_state.input_stack_data.ptr);
                            tex_restore_cur_string(u);
                        }
                    }
                    break;
                case glue_node:
                    /*tex Display glue |p|. */
                    if (is_leader(p)) {
                        /*tex Display leaders |p|. */
                        tex_print_str(", leader ");
                        tex_print_specnode(p, no_unit);
                        tex_print_node_list(glue_leader_ptr(p), "list", threshold, max);
                    } else {
                        if (node_subtype(p) != conditional_math_glue && node_subtype(p) != rulebased_math_glue) {
                            tex_print_char(' ');
                            tex_print_specnode(p, node_subtype(p) < conditional_math_glue ? pt_unit : mu_unit); /* was |no_unit : mu_unit| */
                        }
                        if (glue_data(p)) {
                            tex_print_format(", data %i", glue_data(p));
                        }
                        if (node_subtype(p) == space_skip_glue && glue_font(p)) {
                            tex_print_format(", font %i", glue_font(p));
                        }
                        if (glue_options(p)) {
                            tex_print_format(", options %x", glue_options(p));
                        }
                    }
                    break;
                case kern_node:
                    /*tex Display kern |p| */
                    if (node_subtype(p) != explicit_math_kern_subtype) {
                        tex_print_format(", amount %p", kern_amount(p));
                        if (kern_expansion(p)) {
                            tex_print_format(", expansion %i", kern_expansion(p));
                        }
                    } else {
                        tex_print_format(", amount %D", kern_amount(p), mu_unit);
                    }
                    break;
                case math_node:
                    /*tex Display math node |p|. */
                    if (! tex_math_glue_is_zero(p)) {
                        tex_print_str(", glued ");
                        tex_print_specnode(p, no_unit);
                    } else if (math_surround(p)) {
                        tex_print_format(", surrounded %p", math_surround(p));
                    }
                    if (math_penalty(p)) {
                        tex_print_format(", penalty %i", math_penalty(p));
                    }
                    if (math_pre_tolerance(p)) {
                        tex_print_format(", pretolerance %i", math_pre_tolerance(p));
                    }
                    if (math_tolerance(p)) {
                        tex_print_format(", tolerance %i", math_tolerance(p));
                    }
                    if (math_options(p)) {
                        tex_print_format(", options %x", math_options(p));
                    }
                    break;
                case penalty_node:
                    /*tex Display penalty |p|. */
                    tex_print_format(", amount %i", penalty_amount(p));
                    break;
                case disc_node:
                    if (disc_class(p) != unset_disc_class) {
                        tex_print_format(", class %i", disc_class(p));
                    }
                    if (disc_options(p)) {
                        tex_print_format(", options %x", disc_options(p));
                    }
                    tex_print_format(", penalty %i", disc_penalty(p));
                    tex_print_node_list(disc_pre_break_head(p), "prebreaklist", threshold, max);
                    tex_print_node_list(disc_post_break_head(p), "postbreaklist", threshold, max);
                    tex_print_node_list(disc_no_break_head(p), "nobreaklist", threshold, max);
                    break;
                case mark_node:
                    /*tex Display mark |p|. */
                    tex_print_format(", index %i", mark_index(p));
                    if (node_subtype(p) == reset_mark_value_code) {
                        tex_print_str(", reset");
                    } else {
                        tex_print_token_list(NULL, token_link(mark_ptr(p))); /*tex We have a ref count token. */
                    }
                    break;
                case adjust_node:
                    /*tex Display adjustment |p|. */
                    if (adjust_options(p)) {
                        tex_print_format(", options %x", adjust_options(p));
                    }
                    if (adjust_index(p)) {
                        tex_print_format(", index %i", adjust_index(p));
                    }
                    if (has_adjust_option(p, adjust_option_depth_before) && adjust_depth_before(p)) {
                        tex_print_format(", depthbefore %p", adjust_depth_before(p));
                    }
                    if (has_adjust_option(p, adjust_option_depth_after) &&adjust_depth_before(p)) {
                        tex_print_format(", depthafter %p", adjust_depth_after(p));
                    }
                    tex_print_node_list(adjust_list(p), "list", threshold, max);
                    break;
                case glue_spec_node:
                case math_spec_node:
                case font_spec_node:
                    /*tex This is actually an error! */
                    break;
                case align_record_node:
                    tex_print_token_list(NULL, align_record_pre_part(p)); /*tex No ref count token here. */
                    tex_print_levels();
                    tex_print_str("..<content>");
                    tex_print_token_list(NULL, align_record_post_part(p)); /*tex No ref count token here. */
                    break;
                case temp_node:
                    break;
                default:
                    if (! tex_show_math_node(p, threshold, max)) {
                        tex_print_format("<unknown node type %i>", node_type(p));
                    }
                    break;
            }
            p = node_next(p);
        }
    }
}

/*tex

    This routine finds the base width of a horizontal box, using the same logic that \TEX82\ used
    for |\predisplaywidth|.

*/

static halfword tex_aux_get_actual_box_width(halfword r, halfword p, scaled initial_width)
{
    /*tex calculated |size| */
    scaled w = -max_dimension;
    /*tex |w| plus possible glue amount */
    scaled v = initial_width;
    while (p) {
        /*tex increment to |v| */
        scaled d;
        switch (node_type(p)) {
            case glyph_node:
                d = tex_glyph_width(p);
                goto FOUND;
            case hlist_node:
            case vlist_node:
                d = box_width(p);
                goto FOUND;
            case rule_node:
                d = rule_width(p);
                goto FOUND;
            case kern_node:
                d = kern_amount(p);
                break;
            case disc_node:
                /*tex At the end of the line we should actually take the |pre|. */
                if (disc_no_break(p)) {
                    d = tex_aux_get_actual_box_width(r, disc_no_break_head(p),0);
                    if (d <= -max_dimension || d >= max_dimension) {
                        d = 0;
                    }
                } else {
                    d = 0;
                }
                goto FOUND;
            case math_node:
                if (tex_math_glue_is_zero(p) || tex_ignore_math_skip(p)) {
                    d = math_surround(p);
                } else {
                    d = math_amount(p);
                    switch (box_glue_sign(r)) {
                        case stretching_glue_sign:
                            if ((box_glue_order(r) == math_stretch_order(p)) && math_stretch(p)) {
                                v = max_dimension;
                            }
                            break;
                        case shrinking_glue_sign:
                            if ((box_glue_order(r) == math_shrink_order(p)) && math_shrink(p)) {
                                v = max_dimension;
                            }
                            break;
                    }
                break;
                }
                break;
            case glue_node:
                /*tex
                    We need to be careful that |w|, |v|, and |d| do not depend on any |glue_set|
                    values, since such values are subject to system-dependent rounding. System
                    dependent numbers are not allowed to infiltrate parameters like
                    |pre_display_size|, since \TEX82 is supposed to make the same decisions on
                    all machines.
                */
                d = glue_amount(p);
                if (box_glue_sign(r) == stretching_glue_sign) {
                    if ((box_glue_order(r) == glue_stretch_order(p)) && glue_stretch(p)) {
                        v = max_dimension;
                    }
                } else if (box_glue_sign(r) == shrinking_glue_sign) {
                    if ((box_glue_order(r) == glue_shrink_order(p)) && glue_shrink(p)) {
                        v = max_dimension;
                    }
                }
                if (is_leader(p)) {
                    goto FOUND;
                }
                break;
            default:
                d = 0;
                break;
        }
        if (v < max_dimension) {
            v += d;
        }
        goto NOT_FOUND;
      FOUND:
        if (v < max_dimension) {
            v += d;
            w = v;
        } else {
            w = max_dimension;
            break;
        }
      NOT_FOUND:
        p = node_next(p);
    }
    return w;
}

halfword tex_actual_box_width(halfword r, scaled base_width)
{
    /*tex

        Often this is the same as:

        \starttyping
        return + shift_amount(r) + base_width +
            natural_sizes(list_ptr(r),null,(glueratio) box_glue_set(r),box_glue_sign(r),box_glue_order(r),box_dir(r));
        \stoptyping
    */
    return tex_aux_get_actual_box_width(r, box_list(r), box_shift_amount(r) + base_width);
}

int tex_list_has_glyph(halfword list)
{
    while (list) {
        switch (node_type(list)) {
            case glyph_node:
            case disc_node:
                return 1;
            default:
                list = node_next(list);
                break;
        }
    }
    return 0;
}

/*tex

    Attribute lists need two extra globals to increase processing efficiency. |max_used_attr|
    limits the test loop that checks for set attributes, and |attr_cache| contains a pointer to an
    already created attribute list. It is set to the special value |cache_disabled| when the
    current value can no longer be trusted: after an assignment to an attribute register, and after
    a group has ended.

    From the computer's standpoint, \TEX's chief mission is to create horizontal and vertical
    lists. We shall now investigate how the elements of these lists are represented internally as
    nodes in the dynamic memory.

    A horizontal or vertical list is linked together by |link| fields in the first word of each
    node. Individual nodes represent boxes, glue, penalties, or special things like discretionary
    hyphens; because of this variety, some nodes are longer than others, and we must distinguish
    different kinds of nodes. We do this by putting a |type| field in the first word, together
    with the link and an optional |subtype|.

    Character nodes appear only in horizontal lists, never in vertical lists.

    An |hlist_node| stands for a box that was made from a horizontal list. Each |hlist_node| is
    seven words long, and contains the following fields (in addition to the mandatory |type| and
    |link|, which we shall not mention explicitly when discussing the other node types): The
    |height| and |width| and |depth| are scaled integers denoting the dimensions of the box. There
    is also a |shift_amount| field, a scaled integer indicating how much this box should be
    lowered (if it appears in a horizontal list), or how much it should be moved to the right (if
    it appears in a vertical list). There is a |list_ptr| field, which points to the beginning of
    the list from which this box was fabricated; if |list_ptr| is |null|, the box is empty. Finally,
    there are three fields that represent the setting of the glue: |glue_set(p)| is a word of type
    |glue_ratio| that represents the proportionality constant for glue setting; |glue_sign(p)| is
    |stretching| or |shrinking| or |normal| depending on whether or not the glue should stretch or
    shrink or remain rigid; and |glue_order(p)| specifies the order of infinity to which glue
    setting applies (|normal|, |sfi|, |fil|, |fill|, or |filll|). The |subtype| field is not used.

    The |new_null_box| function returns a pointer to an |hlist_node| in which all subfields have
    the values corresponding to |\hbox{}|. The |subtype| field is set to |min_quarterword|, since
    that's the desired |span_count| value if this |hlist_node| is changed to an |unset_node|.

*/

/*tex Create a new box node. */

halfword tex_new_null_box_node(quarterword t, quarterword s)
{
 // halfword p = tex_new_node(hlist_node, min_quarterword);
    halfword p = tex_new_node(t, s);
    box_direction(p) = (singleword) text_direction_par;
    return p;
}

/*tex

    A |vlist_node| is like an |hlist_node| in all respects except that it contains a vertical list.

    A |rule_node| stands for a solid black rectangle; it has |width|, |depth|, and |height| fields
    just as in an |hlist_node|. However, if any of these dimensions is $-2^{30}$, the actual value
    will be determined by running the rule up to the boundary of the innermost enclosing box. This
    is called a \quote {running dimension}. The |width| is never running in an hlist; the |height|
    and |depth| are never running in a~vlist.

    A new rule node is delivered by the |new_rule| function. It makes all the dimensions \quote
    {running}, so you have to change the ones that are not allowed to run.

*/

halfword tex_new_rule_node(quarterword s)
{
    return tex_new_node(rule_node, s);
}

/*tex

    Insertions are represented by |insert_node| records, where the |subtype| indicates the
    corresponding box number. For example, |\insert 250| leads to an |insert_node| whose |subtype|
    is |250 + min_quarterword|. The |height| field of an |insert_node| is slightly misnamed; it
    actually holds the natural height plus depth of the vertical list being inserted. The |depth|
    field holds the |split_max_depth| to be used in case this insertion is split, and the
    |split_top_ptr| points to the corresponding |split_top_skip|. The |float_cost| field holds the
    |floating_penalty| that will be used if this insertion floats to a subsequent page after a
    split insertion of the same class. There is one more field, the |insert_ptr|, which points to the
    beginning of the vlist for the insertion.

    A |mark_node| has a |mark_ptr| field that points to the reference count of a token list that
    contains the user's |\mark| text. In addition there is a |mark_class| field that contains the
    mark class.

    An |adjust_node|, which occurs only in horizontal lists, specifies material that will be moved
    out into the surrounding vertical list; i.e., it is used to implement \TEX's |\vadjust|
    operation. The |adjust_ptr| field points to the vlist containing this material.

    A |glyph_node|, which occurs only in horizontal lists, specifies a glyph in a particular font,
    along with its attribute list. Older versions of \TEX\ could use token memory for characters,
    because the font,char combination would fit in a single word (both values were required to be
    strictly less than $2^{16}$). In \LUATEX, room is needed for characters that are larger than
    that, as well as a pointer to a potential attribute list, and the two displacement values.

    In turn, that made the node so large that it made sense to merge ligature glyphs as well, as
    that requires only one extra pointer. A few extra classes of glyph nodes will be introduced
    later. The unification of all those types makes it easier to manipulate lists of glyphs. The
    subtype differentiates various glyph kinds.

    First, here is a function that returns a pointer to a glyph node for a given glyph in a given
    font. If that glyph doesn't exist, |null| is returned instead. Nodes of this subtype are
    directly created only for accents and their base (through |make_accent|), and math nucleus
    items (in the conversion from |mlist| to |hlist|).

    We no longer check if the glyph exists because a replacement can be used instead. We copy some
    properties when there is a parent passed.

*/

halfword tex_new_glyph_node(quarterword subtype, halfword fnt, halfword chr, halfword parent)
{
    halfword p = parent  && node_type(parent) == glyph_node ? tex_copy_node(parent) : tex_aux_new_glyph_node_with_attributes(parent);
    node_subtype(p) = subtype;
    glyph_font(p) = fnt;
    glyph_character(p) = chr;
    tex_char_process(fnt, chr);
    return p;
}

/*tex

    A subset of the glyphs nodes represent ligatures: characters fabricated from the interaction
    of two or more actual characters. The characters that generated the ligature have not been
    forgotten, since they are needed for diagnostic messages; the |lig_ptr| field points to a
    linked list of character nodes for all original characters that have been deleted. (This list
    might be empty if the characters that generated the ligature were retained in other nodes.)
    Remark: we no longer keep track of ligatures via |lig_ptr| because there is no guarantee that
    they are consistently tracked; they are something internal anyway. Of course one can provide an
    alternative at the \LUA\ end (which is what we do in \CONTEXT).

    The |subtype| field of these |glyph_node|s is 1, plus 2 and/or 1 if the original source of the
    ligature included implicit left and/or right boundaries. These nodes are created by the C
    function |new_ligkern|.

    A third general type of glyphs could be called a character, as it only appears in lists that
    are not yet processed by the ligaturing and kerning steps of the program.

    |main_control| inserts these, and they are later converted to |subtype_normal| by |new_ligkern|.

*/

/*
quarterword norm_min(int h)
{
    if (h <= 0)
        return 1;
    else if (h >= 255)
        return 255;
    else
        return (quarterword) h;
}
*/

halfword tex_new_char_node(quarterword subtype, halfword fnt, halfword chr, int all)
{
    halfword p = tex_aux_new_glyph_node_with_attributes(null);
    node_subtype(p) = subtype;
    glyph_font(p) = fnt;
    glyph_character(p) = chr;
    if (all) {
        glyph_data(p) = glyph_data_par;
        /* no state */
        set_glyph_script(p, glyph_script_par);
        set_glyph_language(p, cur_lang_par);
        set_glyph_control(p, tex_get_cc_code(chr));
        set_glyph_lhmin(p, left_hyphen_min_par);
        set_glyph_rhmin(p, right_hyphen_min_par);
        set_glyph_hyphenate(p, hyphenation_mode_par);
        set_glyph_options(p, glyph_options_par);
        set_glyph_scale(p, glyph_scale_par);
        set_glyph_x_scale(p, glyph_x_scale_par);
        set_glyph_y_scale(p, glyph_y_scale_par);
        set_glyph_x_offset(p, glyph_x_offset_par);
        set_glyph_y_offset(p, glyph_y_offset_par);
        set_glyph_slant(p, glyph_slant_par);
        set_glyph_weight(p, glyph_weight_par);
    }
    if (tex_char_exists(fnt, chr)) {
        tex_char_process(fnt, chr);
    } else {
        tex_missing_character(p, fnt, chr, missing_character_text_glyph);
    }
    return p;
}

/*tex
    This one is called in the lua node interface so we assume checking being done at that end.
*/

halfword tex_new_text_glyph(halfword fnt, halfword chr)
{
    halfword p = tex_get_node(glyph_node_size);
    memset((void *) (lmt_node_memory_state.nodes + p + 1), 0, (sizeof(memoryword) * (glyph_node_size - 1)));
    node_type(p) = glyph_node;
    node_subtype(p) = glyph_unset_subtype;
    glyph_font(p) = fnt;
    glyph_character(p) = chr;
    glyph_data(p) = glyph_data_par;
    /* no state */
    set_glyph_script(p, glyph_script_par);
    set_glyph_language(p, cur_lang_par);
    set_glyph_control(p, tex_get_cc_code(chr));
    set_glyph_lhmin(p, left_hyphen_min_par);
    set_glyph_rhmin(p, right_hyphen_min_par);
    set_glyph_hyphenate(p, hyphenation_mode_par);
    set_glyph_options(p, glyph_options_par);
    set_glyph_scale(p, glyph_scale_par);
    set_glyph_x_scale(p, glyph_x_scale_par);
    set_glyph_y_scale(p, glyph_y_scale_par);
    set_glyph_x_offset(p, glyph_x_offset_par);
    set_glyph_y_offset(p, glyph_y_offset_par);
    set_glyph_slant(p, glyph_slant_par);
    set_glyph_weight(p, glyph_weight_par);
    return p;
}

/*tex

    Here are a few handy helpers used by the list output routines.

    We had an xadvance but dropped it but it might come back eventually. The offsets are mostly
    there to deal with anchoring and we assume kerns to be used to complement x offsets if needed:
    just practical decisions made long ago.

    Why do we check y offset being positive for dp but not for ht? Maybe change this to be
    consistent? Anyway, we have adapted \LUATEX\ so ...

    \startitemize
    \startitem what we had before \stopitem
    \startitem compensate height and depth \stopitem
    \startitem compensate height and depth, take max \stopitem
    \startitem we keep height and depth \stopitem
    \stopitemize

*/

/*tex These should move to the texfont.c as we have too many variants now. */

scaled tex_glyph_width(halfword p)
{
    scaled w = tex_char_width_from_glyph(p);
    scaled x = glyph_x_offset(p);
    if (x && tex_has_glyph_option(p, glyph_option_apply_x_offset)) {
        w += x; /* or after expansion? needs testing */
    }
    w -= (glyph_left(p) + glyph_right(p));
    return w;
}

scaled tex_glyph_width_ex(halfword p)
{
    scaled w = tex_char_width_from_glyph(p);
    scaled x = glyph_x_offset(p);
    if (x && tex_has_glyph_option(p, glyph_option_apply_x_offset)) {
        w += x; /* or after expansion? needs testing */
    }
    w -= (glyph_left(p) + glyph_right(p));
    if (glyph_expansion(p)) {
        w = w + tex_ext_xn_over_d(w, scaling_factor_squared + glyph_expansion(p), scaling_factor_squared);
    }
    return w;
}

scaled tex_glyph_height(halfword p)
{
    scaled h = tex_char_height_from_glyph(p) + glyph_raise(p);
    scaled y = glyph_y_offset(p);
    if (y && tex_has_glyph_option(p, glyph_option_apply_y_offset)) {
        h += y;
    }
    return h < 0 ? 0 : h;
}

scaled tex_glyph_depth(halfword p) /* not used */
{
    scaled d = tex_char_depth_from_glyph(p) - glyph_raise(p);
    scaled y = glyph_y_offset(p);
    if (y && tex_has_glyph_option(p, glyph_option_apply_y_offset)) {
        d -= y;
    }
    return d < 0 ? 0 : d;
}

scaledwhd tex_glyph_dimensions(halfword p)
{
    scaledwhd whd = tex_char_whd_from_glyph(p);
    scaled x = glyph_x_offset(p);
    scaled y = glyph_y_offset(p);
    whd.ht += glyph_raise(p);
    whd.dp -= glyph_raise(p);
    whd.wd += (glyph_left(p) + glyph_right(p));
    if (x && tex_has_glyph_option(p, glyph_option_apply_x_offset)) {
        whd.wd += x;
    }
    if (y && tex_has_glyph_option(p, glyph_option_apply_y_offset)) {
        whd.ht += y;
        whd.dp -= y;
    }
    if (whd.ht < 0) {
        whd.ht = 0;
    }
    if (whd.dp < 0) {
        whd.dp = 0;
    }
    return whd;
}

scaledwhd tex_glyph_dimensions_ex(halfword p)
{
    scaledwhd whd = tex_char_whd_from_glyph(p);
    scaled x = glyph_x_offset(p);
    scaled y = glyph_y_offset(p);
    whd.ht += glyph_raise(p);
    whd.dp -= glyph_raise(p);
    whd.wd -= (glyph_left(p) + glyph_right(p));
    if (x && tex_has_glyph_option(p, glyph_option_apply_x_offset)) {
        whd.wd += x;
    }
    if (y && tex_has_glyph_option(p, glyph_option_apply_y_offset)) {
        whd.ht += y;
        whd.dp -= y;
    }
    if (whd.ht < 0) {
        whd.ht = 0;
    }
    if (whd.dp < 0) {
        whd.dp = 0;
    }
    if (whd.wd && glyph_expansion(p)) {
        whd.wd = tex_ext_xn_over_d(whd.wd, scaling_factor_squared + glyph_expansion(p), scaling_factor_squared);
    }
    return whd;
}

scaled tex_glyph_total(halfword p)
{
    return tex_char_total_from_glyph(p);
}

int tex_glyph_has_dimensions(halfword p)
{
    scaledwhd whd = tex_char_whd_from_glyph(p);
    scaled offset = glyph_x_offset(p);
    scaled amount = whd.wd;
    if (offset && tex_has_glyph_option(p, glyph_option_apply_x_offset)) {
        amount += offset;
    }
    amount -= (glyph_left(p) + glyph_right(p));
    if (amount) {
        return 1;
    } else {
        /* here offset and raise just moves */
        return whd.ht > 0 || whd.dp > 0;
    }
}

halfword tex_kern_dimension(halfword p)
{
    return kern_amount(p);
}

halfword tex_kern_dimension_ex(halfword p)
{
    halfword k = kern_amount(p);
    if (k && kern_expansion(p)) {
        k = tex_ext_xn_over_d(k, scaling_factor_squared + kern_expansion(p), scaling_factor_squared);
    }
    return k;
}

scaledwhd tex_pack_dimensions(halfword p)
{
    scaledwhd siz = { .wd = 0, .ht = 0, .dp = 0, .ns = 0 };
    siz.ht = box_height(p);
    siz.dp = box_depth(p);
    siz.wd = box_width(p);
    return siz;
}

/*tex

    The program above includes a bunch of \quote {hooks} that allow further capabilities to be
    added without upsetting \TEX's basic structure. Most of these hooks are concerned with \quote
    {whatsit} nodes, which are intended to be used for special purposes; whenever a new extension
    to \TEX\ involves a new kind of whatsit node, a corresponding change needs to be made to the
    routines below that deal with such nodes, but it will usually be unnecessary to make many
    changes to the other parts of this program.

    In order to demonstrate how extensions can be made, we shall treat |\write|, |\openout|,
    |\closeout|, |\immediate|, and |\special| as if they were extensions. These commands are
    actually primitives of \TEX, and they should appear in all implementations of the system; but
    let's try to imagine that they aren't. Then the program below illustrates how a person could
    add them.

    Sometimes, of course, an extension will require changes to \TEX\ itself; no system of hooks
    could be complete enough for all conceivable extensions. The features associated with |\write|
    are almost all confined to the following paragraphs, but there are small parts of the |print_ln|
    and |print_char| procedures that were introduced specifically to |\write| characters.
    Furthermore one of the token lists recognized by the scanner is a |write_text|; and there are a
    few other miscellaneous places where we have already provided for some aspect of |\write|. The
    goal of a \TeX\ extender should be to minimize alterations to the standard parts of the program,
    and to avoid them completely if possible. He or she should also be quite sure that there's no
    easy way to accomplish the desired goals with the standard features that \TEX\ already has.
    \quote {Think thrice before extending}, because that may save a lot of work, and it will also
    keep incompatible extensions of \TEX\ from proliferating.

    First let's consider the format of whatsit nodes that are used to represent the data associated
    with |\write| and its relatives. Recall that a whatsit has |type=whatsit_node|, and the |subtype|
    is supposed to distinguish different kinds of whatsits. Each node occupies two or more words;
    the exact number is immaterial, as long as it is readily determined from the |subtype| or other
    data.

    We shall introduce five |subtype| values here, corresponding to the control sequences |\openout|,
    |\write|, |\closeout|, and |\special|. The second word of I/O whatsits has a |write_stream|
    field that identifies the write-stream number (0 to 15, or 16 for out-of-range and positive, or
    17 for out-of-range and negative). In the case of |\write| and |\special|, there is also a field
    that points to the reference count of a token list that should be sent. In the case of |\openout|,
    we need three words and three auxiliary subfields to hold the string numbers for name, area, and
    extension.

    Extensions might introduce new command codes; but it's best to use |extension| with a modifier,
    whenever possible, so that |main_control| stays the same.

    The sixteen possible |\write| streams are represented by the |write_file| array. The |j|th file
    is open if and only if |write_open[j]=true|. The last two streams are special; |write_open[16]|
    represents a stream number greater than 15, while |write_open[17]| represents a negative stream
    number, and both of these variables are always |false|.

    Writing to files is delegated to \LUA, so we have no write channels.

    To write a token list, we must run it through \TEX's scanner, expanding macros and |\the| and
    |\number|, etc. This might cause runaways, if a delimited macro parameter isn't matched, and
    runaways would be extremely confusing since we are calling on \TEX's scanner in the middle of
    a |\shipout| command. Therefore we will put a dummy control sequence as a \quote {stopper},
    right after the token list. This control sequence is artificially defined to be |\outer|.

    The presence of |\immediate| causes the |do_extension| procedure to descend to one level of
    recursion. Nothing happens unless |\immediate| is followed by |\openout|, |\write|, or
    |\closeout|.

    Here is a subroutine that creates a whatsit node having a given |subtype| and a given number
    of words. It initializes only the first word of the whatsit, and appends it to the current
    list.

    A |whatsit_node| is a wild card reserved for extensions to \TEX. The |subtype| field in its
    first word says what |whatsit| it is, and implicitly determines the node size (which must be
    2 or more) and the format of the remaining words. When a |whatsit_node| is encountered in a
    list, special actions are invoked; knowledgeable people who are careful not to mess up the
    rest of \TEX\ are able to make \TEX\ do new things by adding code at the end of the program.
    For example, there might be a \quote {\TEX nicolor} extension to specify different colors of
    ink, and the whatsit node might contain the desired parameters.

    The present implementation of \TEX\ treats the features associated with |\write| and |\special|
    as if they were extensions, in order to illustrate how such routines might be coded. We shall
    defer further discussion of extensions until the end of this program.

    However, in \LUAMETATEX\ we only have a generic whatsit node, a small one that can be used to
    implement whatever you like, using \LUA. So, all we have here is the above comment as
    guideline for that.

    \TEX\ makes use of the fact that |hlist_node|, |vlist_node|, |rule_node|, |insert_node|,
    |mark_node|, |adjust_node|, |disc_node|, |whatsit_node|, and |math_node| are at the low end of
    the type codes, by permitting a break at glue in a list if and only if the |type| of the
    previous node is less than |math_node|. Furthermore, a node is discarded after a break if its
    type is |math_node| or~more.

    A |glue_node| represents glue in a list. However, it is really only a pointer to a separate
    glue specification, since \TEX\ makes use of the fact that many essentially identical nodes of
    glue are usually present. If |p| points to a |glue_node|, |glue_ptr(p)| points to another packet
    of words that specify the stretch and shrink components, etc.

    Glue nodes also serve to represent leaders; the |subtype| is used to distinguish between
    ordinary glue (which is called |normal|) and the three kinds of leaders (which are called
    |a_leaders|, |c_leaders|, and |x_leaders|). The |leader_ptr| field points to a rule node or to
    a box node containing the leaders; it is set to |null| in ordinary glue nodes.

    Many kinds of glue are computed from \TEX's skip parameters, and it is helpful to know which
    parameter has led to a particular glue node. Therefore the |subtype| is set to indicate the
    source of glue, whenever it originated as a parameter. We will be defining symbolic names for
    the parameter numbers later (e.g., |line_skip_code = 0|, |baseline_skip_code = 1|, etc.); it
    suffices for now to say that the |subtype| of parametric glue will be the same as the parameter
    number, plus~one.

    In math formulas there are two more possibilities for the |subtype| in a glue node: |mu_glue|
    denotes an |\mskip| (where the units are scaled |mu| instead of scaled |pt|); and
    |cond_math_glue| denotes the |\nonscript| feature that cancels the glue node immediately
    following if it appears in a subscript.

    A glue specification has a halfword reference count in its first word, representing |null|
    plus the number of glue nodes that point to it (less one). Note that the reference count
    appears in the same position as the |link| field in list nodes; this is the field that is
    initialized to |null| when a node is allocated, and it is also the field that is flagged by
    |empty_flag| in empty nodes.

    Glue specifications also contain three |scaled| fields, for the |width|, |stretch|, and
    |shrink| dimensions. Finally, there are two one-byte fields called |stretch_order| and
    |shrink_order|; these contain the orders of infinity (|normal|, |sfi|, |fil|, |fill|, or
    |filll|) corresponding to the stretch and shrink values.

    Here is a function that returns a pointer to a copy of a glue spec. The reference count in the
    copy is |null|, because there is assumed to be exactly one reference to the new specification.

*/

halfword tex_new_glue_spec_node(halfword q)
{
    if (q) {
        switch (node_type(q)) {
            case glue_spec_node:
                return tex_copy_node(q);
            case glue_node:
                {
                    halfword p = tex_copy_node(zero_glue);
                    glue_amount(p) = glue_amount(q);
                    glue_stretch(p) = glue_stretch(q);
                    glue_shrink(p) = glue_shrink(q);
                    glue_stretch_order(p) = glue_stretch_order(q);
                    glue_shrink_order(p) = glue_shrink_order(q);
                    return p;
                }
        }
    }
    return tex_copy_node(zero_glue);
}

/*tex

    And here's a function that creates a glue node for a given parameter identified by its code
    number; for example, |new_param_glue(line_skip_code)| returns a pointer to a glue node for the
    current |\lineskip|.

*/

halfword tex_new_param_glue_node(quarterword p, quarterword s)
{
    halfword n = tex_new_node(glue_node, s);
    halfword g = glue_parameter(p);
    if (g) {
        memcpy((void *) (lmt_node_memory_state.nodes + n + 2), (void *) (lmt_node_memory_state.nodes + g + 2), (glue_spec_size - 2) * (sizeof(memoryword)));
    }
    return n;
}

/*tex

    Glue nodes that are more or less anonymous are created by |new_glue|, whose argument points to
    a glue specification.

*/

halfword tex_new_glue_node(halfword q, quarterword s)
{
    halfword p = tex_new_node(glue_node, s);
    memcpy((void *) (lmt_node_memory_state.nodes + p + 2), (void *) (lmt_node_memory_state.nodes + q + 2), (glue_spec_size - 2) * (sizeof(memoryword)));
    return p;
}

/*tex

    Still another subroutine is needed: |new_skip_param|. This one is sort of a combination of
    |new_param_glue| and |new_glue|. It creates a glue node for one of the current glue parameters,
    but it makes a fresh copy of the glue specification, since that specification will probably be
    subject to change, while the parameter will stay put.

    Remark: as we have copies we don't need this one can use |new_param_glue| instead.

*/

/*tex

    A |kern_node| has a |width| field to specify a (normally negative) amount of spacing. This
    spacing correction appears in horizontal lists between letters like A and V when the font
    designer said that it looks better to move them closer together or further apart. A kern node
    can also appear in a vertical list, when its |width| denotes additional spacing in the vertical
    direction. The |subtype| is either |normal| (for kerns inserted from font information or math
    mode calculations) or |explicit| (for kerns inserted from |\kern| and |\/| commands) or
    |acc_kern| (for kerns inserted from non-math accents) or |mu_glue| (for kerns inserted from
    |\mkern| specifications in math formulas).

    The |new_kern| function creates a (font) kern node having a given width.

*/

halfword tex_new_kern_node(scaled w, quarterword s)
{
    halfword p = tex_new_node(kern_node, s);
    kern_amount(p) = w;
    return p;
}

/*tex

    A |penalty_node| specifies the penalty associated with line or page breaking, in its |penalty|
    field. This field is a fullword integer, but the full range of integer values is not used:
    Any penalty |>=10000| is treated as infinity, and no break will be allowed for such high values.
    Similarly, any penalty |<= -10000| is treated as negative infinity, and a break will be forced.

    Anyone who has been reading the last few sections of the program will be able to guess what
    comes next.

*/

halfword tex_new_penalty_node(halfword m, quarterword s)
{
    halfword p = tex_new_node(penalty_node, s);
    penalty_amount(p) = m;
    return p;
}

/*tex

    You might think that we have introduced enough node types by now. Well, almost, but there is
    one more: An |unset_node| has nearly the same format as an |hlist_node| or |vlist_node|; it is
    used for entries in |\halign| or |\valign| that are not yet in their final form, since the box
    dimensions are their \quote {natural} sizes before any glue adjustment has been made. The
    |glue_set| word is not present; instead, we have a |glue_stretch| field, which contains the
    total stretch of order |glue_order| that is present in the hlist or vlist being boxed.
    Similarly, the |shift_amount| field is replaced by a |glue_shrink| field, containing the total
    shrink of order |glue_sign| that is present. The |subtype| field is called |span_count|; an
    unset box typically contains the data for |qo(span_count)+1| columns. Unset nodes will be
    changed to box nodes when alignment is completed.

    In fact, there are still more types coming. When we get to math formula processing we will
    see that a |style_node| has |type=14|; and a number of larger type codes will also be defined,
    for use in math mode only.

    Warning: If any changes are made to these data structure layouts, such as changing any of the
    node sizes or even reordering the words of nodes, the |copy_node_list| procedure and the memory
    initialization code below may have to be changed. However, other references to the nodes are
    made symbolically in terms of the \WEB\ macro definitions above, so that format changes will
    leave \TEX's other algorithms intact.

    Some day we might store the current paragraph properties in this node. Actually, we already
    store the interline and broken penalties. But it then also demands adaptation if the functions
    that deal with breaking (we can just pass the local par node) and related specification node
    cleanups. We could either snapshot parameters before a group ends, or we can add a lots of
    |\local...| parameters.

    This section might move to its own file.

*/

halfword tex_new_par_node(quarterword subtype)
{
    halfword par = tex_new_node(par_node, subtype);
    if (subtype == parameter_par_subtype) {
        tex_set_local_interline_penalty(par, local_interline_penalty_par);
        tex_set_local_broken_penalty(par, local_broken_penalty_par);
        tex_set_local_tolerance(par, local_tolerance_par);
        tex_set_local_pre_tolerance(par, local_pre_tolerance_par);
    }
    par_direction(par) = (singleword) par_direction_par;
    par_options(par) = (singleword) par_options_par;
    tex_add_local_boxes(par); /*tex Is this really always needed? */
    if (subtype != local_box_par_subtype) {
        lmt_insert_par_callback(par, subtype);
    }
    return par;
}

static halfword tex_aux_internal_to_par_code(halfword cmd, halfword index) {
    switch (cmd) {
        case internal_integer_cmd:
            switch (index) {
                case hang_after_code             : return par_hang_after_code;
                case adjust_spacing_code         : return par_adjust_spacing_code;
                case protrude_chars_code         : return par_protrude_chars_code;
                case pre_tolerance_code          : return par_pre_tolerance_code;
                case tolerance_code              : return par_tolerance_code;
                case looseness_code              : return par_looseness_code;
                case last_line_fit_code          : return par_last_line_fit_code;
                case line_penalty_code           : return par_line_penalty_code;
                case inter_line_penalty_code     : return par_inter_line_penalty_code;
                case club_penalty_code           : return par_club_penalty_code;
                case widow_penalty_code          : return par_widow_penalty_code;
                case display_widow_penalty_code  : return par_display_widow_penalty_code;
                case left_twin_demerits_code     : return par_left_twin_demerits_code;
                case right_twin_demerits_code    : return par_right_twin_demerits_code;
                case single_line_penalty_code    : return par_single_line_penalty_code;
                case hyphen_penalty_code         : return par_hyphen_penalty_code;
                case ex_hyphen_penalty_code      : return par_ex_hyphen_penalty_code;
                case broken_penalty_code         : return par_broken_penalty_code;
                case adj_demerits_code           : return par_adj_demerits_code;
                case double_hyphen_demerits_code : return par_double_hyphen_demerits_code;
                case final_hyphen_demerits_code  : return par_final_hyphen_demerits_code;
                case shaping_penalties_mode_code : return par_shaping_penalties_mode_code;
                case shaping_penalty_code        : return par_shaping_penalty_code;
                case line_break_checks_code      : return par_line_break_checks_code;
                case orphan_line_factors_code    : return par_orphan_line_factors_code;
                case adjust_spacing_step_code    : return par_adjust_spacing_step_code;
                case adjust_spacing_shrink_code  : return par_adjust_spacing_shrink_code;
                case adjust_spacing_stretch_code : return par_adjust_spacing_stretch_code;
                case hyphenation_mode_code       : return par_hyphenation_mode_code;
            }
            break;
        case internal_dimension_cmd:
            switch (index) {
                case hsize_code                   : return par_hsize_code;
                case hang_indent_code             : return par_hang_indent_code;
                case par_indent_code              : return par_par_indent_code;
                case emergency_stretch_code       : return par_emergency_stretch_code;
                case line_skip_limit_code         : return par_line_skip_limit_code;
                case emergency_extra_stretch_code : return par_emergency_extra_stretch_code;
            }
            break;
        case internal_glue_cmd:
            switch (index) {
                case left_skip_code              : return par_left_skip_code;
                case right_skip_code             : return par_right_skip_code;
                case par_fill_left_skip_code     : return par_par_fill_left_skip_code;
                case par_fill_right_skip_code    : return par_par_fill_right_skip_code;
                case par_init_left_skip_code     : return par_par_init_left_skip_code;
                case par_init_right_skip_code    : return par_par_init_right_skip_code;
                case emergency_left_skip_code    : return par_emergency_left_skip_code;
                case emergency_right_skip_code   : return par_emergency_right_skip_code;
                case baseline_skip_code          : return par_baseline_skip_code;
                case line_skip_code              : return par_line_skip_code;
            }
            break;

        case specification_reference_cmd:
            switch (index) {
                case par_shape_code              : return par_par_shape_code;
                case inter_line_penalties_code   : return par_inter_line_penalties_code;
                case club_penalties_code         : return par_club_penalties_code;
                case widow_penalties_code        : return par_widow_penalties_code;
                case display_widow_penalties_code: return par_display_widow_penalties_code;
                case orphan_penalties_code       : return par_orphan_penalties_code;
                case toddler_penalties_code      : return par_toddler_penalties_code;
                case par_broken_penalties_code   : return par_broken_penalties_code;
                case fitness_classes_code        : return par_fitness_classes_code;
                case adjacent_demerits_code      : return par_adjacent_demerits_code;
                case orphan_line_factors_code    : return par_orphan_line_factors_code;
                case par_passes_code             : return par_par_passes_code;
            }
            break;
    }
    return -1;
}

void tex_update_par_par(halfword cmd, halfword index)
{
    halfword code = tex_aux_internal_to_par_code(cmd, index);
    if (code >= 0) {
        halfword par = tex_find_par_par(cur_list.head);
        if (par) {
            tex_snapshot_par(par, code);
        }
    }
}

halfword tex_get_par_par(halfword p, halfword what)
{
    int set = tex_par_state_is_set(p, what);
    switch (what) {
        case par_par_shape_code:               return set ? par_par_shape(p)               : par_shape_par;
        case par_inter_line_penalties_code:    return set ? par_inter_line_penalties(p)    : inter_line_penalties_par;
        case par_club_penalties_code:          return set ? par_club_penalties(p)          : club_penalties_par;
        case par_widow_penalties_code:         return set ? par_widow_penalties(p)         : widow_penalties_par;
        case par_display_widow_penalties_code: return set ? par_display_widow_penalties(p) : display_widow_penalties_par;
        case par_broken_penalties_code:        return set ? par_broken_penalties(p)        : broken_penalties_par;
        case par_orphan_penalties_code:        return set ? par_orphan_penalties(p)        : orphan_penalties_par;
        case par_fitness_classes_code:         return set ? par_fitness_classes(p)         : fitness_classes_par;
        case par_adjacent_demerits_code:       return set ? par_adjacent_demerits(p)       : adjacent_demerits_par;
        case par_orphan_line_factors_code:     return set ? par_orphan_line_factors(p)     : null; /* maybe */
        case par_hang_indent_code:             return set ? par_hang_indent(p)             : hang_indent_par;
        case par_hang_after_code:              return set ? par_hang_after(p)              : hang_after_par;
        case par_hsize_code:                   return set ? par_hsize(p)                   : hsize_par;
        case par_left_skip_code:               return set ? par_left_skip(p)               : left_skip_par;
        case par_right_skip_code:              return set ? par_right_skip(p)              : right_skip_par;
        case par_last_line_fit_code:           return set ? par_last_line_fit(p)           : last_line_fit_par;
        case par_pre_tolerance_code:           return set ? par_pre_tolerance(p)           : pre_tolerance_par;
        case par_tolerance_code:               return set ? par_tolerance(p)               : tolerance_par;
        case par_looseness_code:               return set ? par_looseness(p)               : looseness_par;
        case par_adjust_spacing_code:          return set ? par_adjust_spacing(p)          : adjust_spacing_par;
        case par_adj_demerits_code:            return set ? par_adj_demerits(p)            : adj_demerits_par;
        case par_protrude_chars_code:          return set ? par_protrude_chars(p)          : protrude_chars_par;
        case par_line_penalty_code:            return set ? par_line_penalty(p)            : line_penalty_par;
        case par_double_hyphen_demerits_code:  return set ? par_double_hyphen_demerits(p)  : double_hyphen_demerits_par;
        case par_final_hyphen_demerits_code:   return set ? par_final_hyphen_demerits(p)   : final_hyphen_demerits_par;
        case par_inter_line_penalty_code:      return set ? par_inter_line_penalty(p)      : inter_line_penalty_par;
        case par_club_penalty_code:            return set ? par_club_penalty(p)            : club_penalty_par;
        case par_widow_penalty_code:           return set ? par_widow_penalty(p)           : widow_penalty_par;
        case par_display_widow_penalty_code:   return set ? par_display_widow_penalty(p)   : display_widow_penalty_par;
        case par_toddler_penalties_code:       return set ? par_toddler_penalties(p)       : toddler_penalties_par;
        case par_left_twin_demerits_code:      return set ? par_left_twin_demerits(p)      : left_twin_demerits_par;
        case par_right_twin_demerits_code:     return set ? par_right_twin_demerits(p)     : right_twin_demerits_par;
        case par_single_line_penalty_code:     return set ? par_single_line_penalty(p)     : single_line_penalty_par;
        case par_hyphen_penalty_code:          return set ? par_hyphen_penalty(p)          : hyphen_penalty_par;
        case par_ex_hyphen_penalty_code:       return set ? par_ex_hyphen_penalty(p)       : ex_hyphen_penalty_par;
        case par_broken_penalty_code:          return set ? par_broken_penalty(p)          : broken_penalty_par;
        case par_emergency_stretch_code:       return set ? par_emergency_stretch(p)       : emergency_stretch_par;
        case par_par_indent_code:              return set ? par_par_indent(p)              : par_indent_par;
        case par_par_fill_left_skip_code:      return set ? par_par_fill_left_skip(p)      : par_fill_left_skip_par;
        case par_par_fill_right_skip_code:     return set ? par_par_fill_right_skip(p)     : par_fill_right_skip_par;
        case par_par_init_left_skip_code:      return set ? par_par_init_left_skip(p)      : par_init_left_skip_par;
        case par_par_init_right_skip_code:     return set ? par_par_init_right_skip(p)     : par_init_right_skip_par;
        case par_emergency_left_skip_code:     return set ? par_emergency_left_skip(p)     : emergency_left_skip_par;
        case par_emergency_right_skip_code:    return set ? par_emergency_right_skip(p)    : emergency_right_skip_par;
        case par_baseline_skip_code:           return set ? par_baseline_skip(p)           : baseline_skip_par;
        case par_line_skip_code:               return set ? par_line_skip(p)               : line_skip_par;
        case par_line_skip_limit_code:         return set ? par_line_skip_limit(p)         : line_skip_limit_par;
        case par_adjust_spacing_step_code:     return set ? par_adjust_spacing_step(p)     : adjust_spacing_step_par;
        case par_adjust_spacing_shrink_code:   return set ? par_adjust_spacing_shrink(p)   : adjust_spacing_shrink_par;
        case par_adjust_spacing_stretch_code:  return set ? par_adjust_spacing_stretch(p)  : adjust_spacing_stretch_par;
        case par_hyphenation_mode_code:        return set ? par_hyphenation_mode(p)        : hyphenation_mode_par;
        case par_shaping_penalties_mode_code:  return set ? par_shaping_penalties_mode(p)  : shaping_penalties_mode_par;
        case par_shaping_penalty_code:         return set ? par_shaping_penalty(p)         : shaping_penalty_par;
        case par_emergency_extra_stretch_code: return set ? par_emergency_extra_stretch(p) : emergency_extra_stretch_par;
     // case par_par_passes_code:              return set ? par_par_passes(p)              : par_passes_par;
        case par_par_passes_code:              return set ? par_par_passes(p)              : (par_passes_exception_par ? par_passes_exception_par : par_passes_par);
        case par_line_break_checks_code:       return set ? par_line_break_checks(p)       : line_break_checks_par;
    }
    return null;
}

void tex_set_par_par(halfword p, halfword what, halfword v, int force)
{
    if (force || tex_par_state_is_set(p, what)) {
        switch (what) {
            case par_hsize_code:
                par_hsize(p) = v;
                break;
            case par_left_skip_code:
                if (par_left_skip(p)) {
                    tex_flush_node(par_left_skip(p));
                }
                par_left_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_right_skip_code:
                if (par_right_skip(p)) {
                    tex_flush_node(par_right_skip(p));
                }
                par_right_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_hang_indent_code:
                par_hang_indent(p) = v;
                break;
            case par_hang_after_code:
                par_hang_after(p) = v;
                break;
            case par_par_indent_code:
                par_par_indent(p) = v;
                break;
            case par_par_fill_left_skip_code:
                if (par_par_fill_left_skip(p)) {
                    tex_flush_node(par_par_fill_left_skip(p));
                }
                par_par_fill_left_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_par_fill_right_skip_code:
                if (par_par_fill_right_skip(p)) {
                    tex_flush_node(par_par_fill_right_skip(p));
                }
                par_par_fill_right_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_par_init_left_skip_code:
                if (par_par_init_left_skip(p)) {
                    tex_flush_node(par_par_init_left_skip(p));
                }
                par_par_init_left_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_par_init_right_skip_code:
                if (par_par_init_right_skip(p)) {
                    tex_flush_node(par_par_init_right_skip(p));
                }
                par_par_init_right_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_emergency_left_skip_code:
                if (par_emergency_left_skip(p)) {
                    tex_flush_node(par_emergency_left_skip(p));
                }
                par_emergency_left_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_emergency_right_skip_code:
                if (par_emergency_right_skip(p)) {
                    tex_flush_node(par_emergency_right_skip(p));
                }
                par_emergency_right_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_adjust_spacing_code:
                par_adjust_spacing(p) = v;
                break;
            case par_protrude_chars_code:
                par_protrude_chars(p) = v;
                break;
            case par_pre_tolerance_code:
                par_pre_tolerance(p) = v;
                break;
            case par_tolerance_code:
                par_tolerance(p) = v;
                break;
            case par_emergency_stretch_code:
                par_emergency_stretch(p) = v;
                break;
            case par_looseness_code:
                par_looseness(p) = v;
                break;
            case par_single_line_penalty_code:
                par_single_line_penalty(p) = v;
                break;
            case par_hyphen_penalty_code:
                par_hyphen_penalty(p) = v;
                break;
            case par_ex_hyphen_penalty_code:
                par_ex_hyphen_penalty(p) = v;
                break;
            case par_last_line_fit_code:
                par_last_line_fit(p) = v;
                break;
            case par_line_penalty_code:
                par_line_penalty(p) = v;
                break;
            case par_inter_line_penalty_code:
                par_inter_line_penalty(p) = v;
                break;
            case par_club_penalty_code:
                par_club_penalty(p) = v;
                break;
            case par_widow_penalty_code:
                par_widow_penalty(p) = v;
                break;
            case par_display_widow_penalty_code:
                par_display_widow_penalty(p) = v;
                break;
            case par_left_twin_demerits_code:
                par_left_twin_demerits(p) = v;
                break;
            case par_right_twin_demerits_code:
                par_right_twin_demerits(p) = v;
                break;
            case par_broken_penalty_code:
                par_broken_penalty(p) = v;
                break;
            case par_adj_demerits_code:
                par_adj_demerits(p) = v;
                break;
            case par_double_hyphen_demerits_code:
                par_double_hyphen_demerits(p) = v;
                break;
            case par_final_hyphen_demerits_code:
                par_final_hyphen_demerits(p) = v;
                break;
            case par_par_shape_code:
                if (par_par_shape(p)) {
                    tex_flush_node(par_par_shape(p));
                }
                par_par_shape(p) = v ? tex_copy_node(v) : null;
                break;
            case par_inter_line_penalties_code:
                if (par_inter_line_penalties(p)) {
                    tex_flush_node(par_inter_line_penalties(p));
                }
                par_inter_line_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_club_penalties_code:
                if (par_club_penalties(p)) {
                    tex_flush_node(par_club_penalties(p));
                }
                par_club_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_widow_penalties_code:
                if (par_widow_penalties(p)) {
                    tex_flush_node(par_widow_penalties(p));
                }
                par_widow_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_display_widow_penalties_code:
                if (par_display_widow_penalties(p)) {
                    tex_flush_node(par_display_widow_penalties(p));
                }
                par_display_widow_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_broken_penalties_code:
                if (par_broken_penalties(p)) {
                    tex_flush_node(par_broken_penalties(p));
                }
                par_broken_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_orphan_penalties_code:
                if (par_orphan_penalties(p)) {
                    tex_flush_node(par_orphan_penalties(p));
                }
                par_orphan_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_toddler_penalties_code:
                if (par_toddler_penalties(p)) {
                    tex_flush_node(par_toddler_penalties(p));
                }
                par_toddler_penalties(p) = v ? tex_copy_node(v) : null;
                break;
            case par_fitness_classes_code:
                if (par_fitness_classes(p)) {
                    tex_flush_node(par_fitness_classes(p));
                }
                par_fitness_classes(p) = v ? tex_copy_node(v) : null;
                break;
            case par_adjacent_demerits_code:
                if (par_adjacent_demerits(p)) {
                    tex_flush_node(par_adjacent_demerits(p));
                }
                par_adjacent_demerits(p) = v ? tex_copy_node(v) : null;
                break;
            case par_orphan_line_factors_code:
                if (par_orphan_line_factors(p)) {
                    tex_flush_node(par_orphan_line_factors(p));
                }
                par_orphan_line_factors(p) = v ? tex_copy_node(v) : null;
                break;
            case par_baseline_skip_code:
                if (par_baseline_skip(p)) {
                    tex_flush_node(par_baseline_skip(p));
                }
                par_baseline_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_line_skip_code:
                if (par_line_skip(p)) {
                    tex_flush_node(par_line_skip(p));
                }
                par_line_skip(p) = v ? tex_copy_node(v) : null;
                break;
            case par_line_skip_limit_code:
                par_line_skip_limit(p) = v;
                break;
            case par_adjust_spacing_step_code:
                par_adjust_spacing_step(p) = v;
                break;
            case par_adjust_spacing_shrink_code:
                par_adjust_spacing_shrink(p) = v;
                break;
            case par_adjust_spacing_stretch_code:
                par_adjust_spacing_stretch(p) = v;
                break;
            case par_hyphenation_mode_code:
                par_hyphenation_mode(p) = v;
                break;
            case par_shaping_penalties_mode_code:
                par_shaping_penalties_mode(p) = v;
                break;
            case par_shaping_penalty_code:
                par_shaping_penalty(p) = v;
                break;
            case par_emergency_extra_stretch_code:
                par_emergency_extra_stretch(p) = v;
                break;
            case par_par_passes_code:
                if (par_par_passes(p)) {
                    tex_flush_node(par_par_passes(p));
                }
                par_par_passes(p) = v ? tex_copy_node(v) : null;
                break;
            case par_line_break_checks_code:
                par_line_break_checks(p) = v;
                break;
        }
        tex_set_par_state(p, what);
    }
}

/*
    This is the reference but as it's called often we use an inlined variant with less redundant
    testing and branching.
*/

/*
    void tex_snapshot_par(halfword p, halfword what)
    {
        if (p && lmt_main_state.run_state != initializing_state) {
            int unset = 0;
            if (what) {
                if (what < 0) {
                    unset = 1;
                    what = -what;
                }
                if (what > par_all_category) {
                    what = par_all_category;
                }
            } else {
                unset = 1;
                what = par_all_category;
            }
            if (tex_par_to_be_set(what, par_hsize_code))                   { tex_set_par_par(p, par_hsize_code,                   unset ? null : hsize_par,                   1); }
            if (tex_par_to_be_set(what, par_left_skip_code))               { tex_set_par_par(p, par_left_skip_code,               unset ? null : left_skip_par,               1); }
            if (tex_par_to_be_set(what, par_right_skip_code))              { tex_set_par_par(p, par_right_skip_code,              unset ? null : right_skip_par,              1); }
            if (tex_par_to_be_set(what, par_hang_indent_code))             { tex_set_par_par(p, par_hang_indent_code,             unset ? null : hang_indent_par,             1); }
            if (tex_par_to_be_set(what, par_hang_after_code))              { tex_set_par_par(p, par_hang_after_code,              unset ? null : hang_after_par,              1); }
            if (tex_par_to_be_set(what, par_par_indent_code))              { tex_set_par_par(p, par_par_indent_code,              unset ? null : par_indent_par,              1); }
            if (tex_par_to_be_set(what, par_par_fill_left_skip_code))      { tex_set_par_par(p, par_par_fill_left_skip_code,      unset ? null : par_fill_left_skip_par,      1); }
            if (tex_par_to_be_set(what, par_par_fill_right_skip_code))     { tex_set_par_par(p, par_par_fill_right_skip_code,     unset ? null : par_fill_right_skip_par,     1); }
            if (tex_par_to_be_set(what, par_par_init_left_skip_code))      { tex_set_par_par(p, par_par_init_left_skip_code,      unset ? null : par_init_left_skip_par,      1); }
            if (tex_par_to_be_set(what, par_par_init_right_skip_code))     { tex_set_par_par(p, par_par_init_right_skip_code,     unset ? null : par_init_right_skip_par,     1); }
            if (tex_par_to_be_set(what, par_adjust_spacing_code))          { tex_set_par_par(p, par_adjust_spacing_code,          unset ? null : adjust_spacing_par,          1); }
            if (tex_par_to_be_set(what, par_protrude_chars_code))          { tex_set_par_par(p, par_protrude_chars_code,          unset ? null : protrude_chars_par,          1); }
            if (tex_par_to_be_set(what, par_pre_tolerance_code))           { tex_set_par_par(p, par_pre_tolerance_code,           unset ? null : pre_tolerance_par,           1); }
            if (tex_par_to_be_set(what, par_tolerance_code))               { tex_set_par_par(p, par_tolerance_code,               unset ? null : tolerance_par,               1); }
            if (tex_par_to_be_set(what, par_emergency_stretch_code))       { tex_set_par_par(p, par_emergency_stretch_code,       unset ? null : emergency_stretch_par,       1); }
            if (tex_par_to_be_set(what, par_looseness_code))               { tex_set_par_par(p, par_looseness_code,               unset ? null : looseness_par,               1); }
            if (tex_par_to_be_set(what, par_last_line_fit_code))           { tex_set_par_par(p, par_last_line_fit_code,           unset ? null : last_line_fit_par,           1); }
            if (tex_par_to_be_set(what, par_line_penalty_code))            { tex_set_par_par(p, par_line_penalty_code,            unset ? null : line_penalty_par,            1); }
            if (tex_par_to_be_set(what, par_inter_line_penalty_code))      { tex_set_par_par(p, par_inter_line_penalty_code,      unset ? null : inter_line_penalty_par,      1); }
            if (tex_par_to_be_set(what, par_club_penalty_code))            { tex_set_par_par(p, par_club_penalty_code,            unset ? null : club_penalty_par,            1); }
            if (tex_par_to_be_set(what, par_widow_penalty_code))           { tex_set_par_par(p, par_widow_penalty_code,           unset ? null : widow_penalty_par,           1); }
            if (tex_par_to_be_set(what, par_display_widow_penalty_code))   { tex_set_par_par(p, par_display_widow_penalty_code,   unset ? null : display_widow_penalty_par,   1); }
            if (tex_par_to_be_set(what, par_broken_penalty_code))          { tex_set_par_par(p, par_broken_penalty_code,          unset ? null : broken_penalty_par,          1); }
            if (tex_par_to_be_set(what, par_adj_demerits_code))            { tex_set_par_par(p, par_adj_demerits_code,            unset ? null : adj_demerits_par,            1); }
            if (tex_par_to_be_set(what, par_double_hyphen_demerits_code))  { tex_set_par_par(p, par_double_hyphen_demerits_code,  unset ? null : double_hyphen_demerits_par,  1); }
            if (tex_par_to_be_set(what, par_final_hyphen_demerits_code))   { tex_set_par_par(p, par_final_hyphen_demerits_code,   unset ? null : final_hyphen_demerits_par,   1); }
            if (tex_par_to_be_set(what, par_par_shape_code))               { tex_set_par_par(p, par_par_shape_code,               unset ? null : par_shape_par,               1); }
            if (tex_par_to_be_set(what, par_inter_line_penalties_code))    { tex_set_par_par(p, par_inter_line_penalties_code,    unset ? null : inter_line_penalties_par,    1); }
            if (tex_par_to_be_set(what, par_club_penalties_code))          { tex_set_par_par(p, par_club_penalties_code,          unset ? null : club_penalties_par,          1); }
            if (tex_par_to_be_set(what, par_widow_penalties_code))         { tex_set_par_par(p, par_widow_penalties_code,         unset ? null : widow_penalties_par,         1); }
            if (tex_par_to_be_set(what, par_display_widow_penalties_code)) { tex_set_par_par(p, par_display_widow_penalties_code, unset ? null : display_widow_penalties_par, 1); }
            if (tex_par_to_be_set(what, par_orphan_penalties_code))        { tex_set_par_par(p, par_orphan_penalties_code,        unset ? null : orphan_penalties_par,        1); }
            if (tex_par_to_be_set(what, par_toddler_penalties_code))       { tex_set_par_par(p, par_toddler_penalties_code,       unset ? null : toddler_penalties_par,       1); }
            if (tex_par_to_be_set(what, par_fitness_demerits_code))        { tex_set_par_par(p, par_fitness_demerits_code,        unset ? null : fitness_demerits_par,        1); }
            if (tex_par_to_be_set(what, par_baseline_skip_code))           { tex_set_par_par(p, par_baseline_skip_code,           unset ? null : baseline_skip_par,           1); }
            if (tex_par_to_be_set(what, par_line_skip_code))               { tex_set_par_par(p, par_line_skip_code,               unset ? null : line_skip_par,               1); }
            if (tex_par_to_be_set(what, par_line_skip_limit_code))         { tex_set_par_par(p, par_line_skip_limit_code,         unset ? null : line_skip_limit_par,         1); }
            if (tex_par_to_be_set(what, par_adjust_spacing_step_code))     { tex_set_par_par(p, par_adjust_spacing_step_code,     unset ? null : adjust_spacing_step_par,     1); }
            if (tex_par_to_be_set(what, par_adjust_spacing_shrink_code))   { tex_set_par_par(p, par_adjust_spacing_shrink_code,   unset ? null : adjust_spacing_shrink_par,   1); }
            if (tex_par_to_be_set(what, par_adjust_spacing_stretch_code))  { tex_set_par_par(p, par_adjust_spacing_stretch_code,  unset ? null : adjust_spacing_stretch_par,  1); }
            if (tex_par_to_be_set(what, par_hyphenation_mode_code))        { tex_set_par_par(p, par_hyphenation_mode_code,        unset ? null : hyphenation_mode_par,        1); }
            if (tex_par_to_be_set(what, par_shaping_penalties_mode_code))  { tex_set_par_par(p, par_shaping_penalties_mode_code,  unset ? null : shaping_penalties_mode_par,  1); }
            if (tex_par_to_be_set(what, par_shaping_penalty_code))         { tex_set_par_par(p, par_shaping_penalty_code,         unset ? null : shaping_penalty_par,         1); }

            if (what == par_all_category) {
                par_state(p) = unset ? 0 : par_all_category;
            } else if (unset) {
                par_state(p) &= ~(what | par_state(p));
            } else {
                par_state(p) |= what;
            }
        }
    }
*/

void tex_snapshot_par(halfword p, halfword what)
{
    if (p && lmt_main_state.run_state != initializing_state) {
        int unset = 0;
        if (what) {
            if (what < 0) {
                unset = 1;
                what = -what;
            }
            if (what > par_all_category) {
                what = par_all_category;
            }
        } else {
            unset = 1;
            what = par_all_category;
        }
        if (tex_par_to_be_set(what, par_hsize_code)) {
             par_hsize(p) = unset ? null : hsize_par;
        }
        if (tex_par_to_be_set(what, par_left_skip_code)) {
            halfword v = unset ? null : left_skip_par;
            if (par_left_skip(p)) {
                tex_flush_node(par_left_skip(p));
            }
            par_left_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_right_skip_code)) {
            halfword v = unset ? null : right_skip_par;
            if (par_right_skip(p)) {
                tex_flush_node(par_right_skip(p));
            }
            par_right_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_hang_indent_code)) {
            par_hang_indent(p) = unset ? null : hang_indent_par;
        }
        if (tex_par_to_be_set(what, par_hang_after_code)) {
            par_hang_after(p) = unset ? null : hang_after_par;
        }
        if (tex_par_to_be_set(what, par_par_indent_code)) {
            par_par_indent(p) = unset ? null : par_indent_par;
        }
        if (tex_par_to_be_set(what, par_par_fill_left_skip_code)) {
            halfword v = unset ? null : par_fill_left_skip_par;
            if (par_par_fill_left_skip(p)) {
                tex_flush_node(par_par_fill_left_skip(p));
            }
            par_par_fill_left_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_par_fill_right_skip_code)) {
            halfword v = unset ? null : par_fill_right_skip_par;
            if (par_par_fill_right_skip(p)) {
                tex_flush_node(par_par_fill_right_skip(p));
            }
            par_par_fill_right_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_par_init_left_skip_code)) {
            halfword v = unset ? null : par_init_left_skip_par;
            if (par_par_init_left_skip(p)) {
                tex_flush_node(par_par_init_left_skip(p));
            }
            par_par_init_left_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_par_init_right_skip_code)) {
            halfword v = unset ? null : par_init_right_skip_par;
            if (par_par_init_right_skip(p)) {
                tex_flush_node(par_par_init_right_skip(p));
            }
            par_par_init_right_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_emergency_left_skip_code)) {
            halfword v = unset ? null : emergency_left_skip_par;
            if (par_emergency_left_skip(p)) {
                tex_flush_node(par_emergency_left_skip(p));
            }
            par_emergency_left_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_emergency_right_skip_code)) {
            halfword v = unset ? null : emergency_right_skip_par;
            if (par_emergency_right_skip(p)) {
                tex_flush_node(par_emergency_right_skip(p));
            }
            par_emergency_right_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_adjust_spacing_code)) {
            par_adjust_spacing(p) = unset ? null : adjust_spacing_par;
        }
        if (tex_par_to_be_set(what, par_protrude_chars_code)) {
            par_protrude_chars(p) = unset ? null : protrude_chars_par;
        }
        if (tex_par_to_be_set(what, par_pre_tolerance_code)) {
            par_pre_tolerance(p) = unset ? null : pre_tolerance_par;
        }
        if (tex_par_to_be_set(what, par_tolerance_code)) {
            par_tolerance(p) = unset ? null : tolerance_par;
        }
        if (tex_par_to_be_set(what, par_emergency_stretch_code)) {
            par_emergency_stretch(p) = unset ? null : emergency_stretch_par;
        }
        if (tex_par_to_be_set(what, par_looseness_code)) {
            par_looseness(p) = unset ? null : looseness_par;
        }
        if (tex_par_to_be_set(what, par_last_line_fit_code)) {
            par_last_line_fit(p) = unset ? null : last_line_fit_par;
        }
        if (tex_par_to_be_set(what, par_line_penalty_code)) {
            par_line_penalty(p) = unset ? null : line_penalty_par;
        }
        if (tex_par_to_be_set(what, par_inter_line_penalty_code)) {
            par_inter_line_penalty(p) = unset ? null : inter_line_penalty_par;
        }
        if (tex_par_to_be_set(what, par_club_penalty_code)) {
            par_club_penalty(p) = unset ? null : club_penalty_par;
        }
        if (tex_par_to_be_set(what, par_widow_penalty_code)) {
            par_widow_penalty(p) = unset ? null : widow_penalty_par;
        }
        if (tex_par_to_be_set(what, par_display_widow_penalty_code)) {
            par_display_widow_penalty(p) = unset ? null : display_widow_penalty_par;
        }
        if (tex_par_to_be_set(what, par_left_twin_demerits_code)) {
            par_left_twin_demerits(p) = unset ? null : left_twin_demerits_par;
        }
        if (tex_par_to_be_set(what, par_right_twin_demerits_code)) {
            par_right_twin_demerits(p) = unset ? null : right_twin_demerits_par;
        }
        if (tex_par_to_be_set(what, par_single_line_penalty_code)) {
            par_single_line_penalty(p) = unset ? null : single_line_penalty_par;
        }
        if (tex_par_to_be_set(what, par_hyphen_penalty_code)) {
            par_hyphen_penalty(p) = unset ? null : hyphen_penalty_par;
        }
        if (tex_par_to_be_set(what, par_ex_hyphen_penalty_code)) {
            par_ex_hyphen_penalty(p) = unset ? null : ex_hyphen_penalty_par;
        }
        if (tex_par_to_be_set(what, par_broken_penalty_code)) {
            par_broken_penalty(p) = unset ? null : broken_penalty_par;
        }
        if (tex_par_to_be_set(what, par_adj_demerits_code)) {
            par_adj_demerits(p) = unset ? null : adj_demerits_par;
        }
        if (tex_par_to_be_set(what, par_double_hyphen_demerits_code)){
            par_double_hyphen_demerits(p) = unset ? null : double_hyphen_demerits_par;
        }
        if (tex_par_to_be_set(what, par_final_hyphen_demerits_code)) {
            par_final_hyphen_demerits(p) = unset ? null : final_hyphen_demerits_par;
        }
        if (tex_par_to_be_set(what, par_par_shape_code))  {
            halfword v = unset ? null : par_shape_par;
            if (par_par_shape(p)) {
                tex_flush_node(par_par_shape(p));
            }
            par_par_shape(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_inter_line_penalties_code)) {
            halfword v = unset ? null : inter_line_penalties_par;
            if (par_inter_line_penalties(p)) {
                tex_flush_node(par_inter_line_penalties(p));
            }
            par_inter_line_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_club_penalties_code)) {
            halfword v = unset ? null : club_penalties_par;
            if (par_club_penalties(p)) {
                tex_flush_node(par_club_penalties(p));
            }
            par_club_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_widow_penalties_code)) {
            halfword v = unset ? null : widow_penalties_par;
            if (par_widow_penalties(p)) {
                tex_flush_node(par_widow_penalties(p));
            }
            par_widow_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_display_widow_penalties_code)) {
            halfword v = unset ? null : display_widow_penalties_par;
            if (par_display_widow_penalties(p)) {
                tex_flush_node(par_display_widow_penalties(p));
            }
            par_display_widow_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_broken_penalties_code)) {
            halfword v = unset ? null : broken_penalties_par;
            if (par_broken_penalties(p)) {
                tex_flush_node(par_broken_penalties(p));
            }
            par_broken_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_orphan_penalties_code)) {
            halfword v = unset ? null : orphan_penalties_par;
            if (par_orphan_penalties(p)) {
                tex_flush_node(par_orphan_penalties(p));
            }
            par_orphan_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_toddler_penalties_code)) {
            halfword v = unset ? null : toddler_penalties_par;
            if (par_toddler_penalties(p)) {
                tex_flush_node(par_toddler_penalties(p));
            }
            par_toddler_penalties(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_fitness_classes_code)) {
            halfword v = unset ? null : fitness_classes_par;
            if (par_fitness_classes(p)) {
                tex_flush_node(par_fitness_classes(p));
            }
            par_fitness_classes(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_adjacent_demerits_code)) {
            halfword v = unset ? null : adjacent_demerits_par;
            if (par_adjacent_demerits(p)) {
                tex_flush_node(par_adjacent_demerits(p));
            }
            par_adjacent_demerits(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_orphan_line_factors_code)) {
            halfword v = unset ? null : orphan_line_factors_par;
            if (par_orphan_line_factors(p)) {
                tex_flush_node(par_orphan_line_factors(p));
            }
            par_orphan_line_factors(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_baseline_skip_code)) {
            halfword v = unset ? null : baseline_skip_par;
            if (par_baseline_skip(p)) {
                tex_flush_node(par_baseline_skip(p));
            }
            par_baseline_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_line_skip_code)) {
            halfword v = unset ? null : line_skip_par;
            if (par_line_skip(p)) {
                tex_flush_node(par_line_skip(p));
            }
            par_line_skip(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_line_skip_limit_code)) {
            par_line_skip_limit(p) = unset ? null : line_skip_limit_par;
        }
        if (tex_par_to_be_set(what, par_adjust_spacing_step_code)) {
            par_adjust_spacing_step(p) = unset ? null : adjust_spacing_step_par;
        }
        if (tex_par_to_be_set(what, par_adjust_spacing_shrink_code)) {
            par_adjust_spacing_shrink(p) = unset ? null : adjust_spacing_shrink_par;
        }
        if (tex_par_to_be_set(what, par_adjust_spacing_stretch_code)) {
            par_adjust_spacing_stretch(p) = unset ? null : adjust_spacing_stretch_par;
        }
        if (tex_par_to_be_set(what, par_hyphenation_mode_code)) {
            par_hyphenation_mode(p) = unset ? null : hyphenation_mode_par;
        }
        if (tex_par_to_be_set(what, par_shaping_penalties_mode_code)) {
            par_shaping_penalties_mode(p) = unset ? null : shaping_penalties_mode_par;
        }
        if (tex_par_to_be_set(what, par_shaping_penalty_code)) {
            par_shaping_penalty(p) = unset ? null : shaping_penalty_par;
        }
        if (tex_par_to_be_set(what, par_emergency_extra_stretch_code)) {
            par_emergency_extra_stretch(p) = unset ? null : emergency_extra_stretch_par;
        }
        if (tex_par_to_be_set(what, par_par_passes_code))  {
         // halfword v = unset ? null : par_passes_par;
            halfword v = unset ? null : (par_passes_exception_par ? par_passes_exception_par : par_passes_par);
            if (par_par_passes(p)) {
                tex_flush_node(par_par_passes(p));
            }
            par_par_passes(p) = v ? tex_copy_node(v) : null;
        }
        if (tex_par_to_be_set(what, par_line_break_checks_code)) {
            par_line_break_checks(p) = unset ? null : line_break_checks_par;
        }
        if (tex_par_to_be_set(what, par_single_line_penalty_code)) {
            par_single_line_penalty(p) = unset ? null : single_line_penalty_par;
        }
        if (tex_par_to_be_set(what, par_hyphen_penalty_code)) {
            par_hyphen_penalty(p) = unset ? null : hyphen_penalty_par;
        }
        if (tex_par_to_be_set(what, par_ex_hyphen_penalty_code)) {
            par_ex_hyphen_penalty(p) = unset ? null : ex_hyphen_penalty_par;
        }
     // tex_set_par_state(p, what);
        if (what == par_all_category) {
            par_state(p) = unset ? 0 : par_all_category;
        } else if (unset) {
            par_state(p) &= ~(what | par_state(p));
        } else {
            par_state(p) |= what;
        }
    }
}

halfword tex_find_par_par(halfword head)
{
    if (head) {
        if (node_type(head) == temp_node) {
            head = node_next(head);
        }
        if (head && node_type(head) == par_node) {
            return head;
        }
    }
    return null;
}

halfword tex_reversed_node_list(halfword list)
{
    if (list) {
        halfword prev = list;
        halfword last = list;
        halfword next = node_next(list);
        if (next) {
            while (1) {
                tex_couple_nodes(list, prev);
                if (node_type(list) == dir_node) {
                    node_subtype(list) = node_subtype(list) == cancel_dir_subtype ? normal_dir_subtype : cancel_dir_subtype;
                }
                if (next) {
                    prev = list;
                    list = next;
                    next = node_next(list);
                } else {
                    node_next(last) = null;
                    node_prev(list) = null;
                    return list;
                }
            }
        }
    }
    return list;
}

/* */

static void *tex_aux_allocate_specification(halfword p, int n, size_t *size)
{
    void *l = NULL;
    switch (node_subtype(p)) {
        case par_passes_code:
            n *= par_passes_size;
            break;
        case balance_shape_code:
            n *= balance_shape_size;
            break;
        case balance_passes_code:
            n *= balance_passes_size;
            break;
        default:
            break;
    }
    *size = n * sizeof(memoryword);
    lmt_node_memory_state.nodes_data.extra += (int) *size;
    if (n) {
        l = lmt_memory_calloc(n, sizeof(memoryword));
        if (! l) {
            tex_overflow_error("nodes", (int) *size);
        }
    }
    return l;
}

static void tex_aux_deallocate_specification(void *p, int size)
{
    lmt_node_memory_state.nodes_data.extra -= size;
    lmt_memory_free(p);
}

void tex_new_specification_list(halfword a, halfword n)
{
    size_t size = 0;
    specification_pointer(a) = tex_aux_allocate_specification(a, n, &size);
    specification_count(a) = specification_pointer(a) ? n : 0;
    specification_size(a) = (halfword) size;
 }

void tex_null_specification_list(halfword a)
{
    specification_pointer(a) = NULL;
    specification_count(a) = 0;
    specification_size(a) = 0;
}

halfword tex_new_specification_node(halfword n, quarterword s, halfword options)
{
    halfword p = tex_new_node(specification_node, s);
    if (n) {
        tex_new_specification_list(p, n);
    } else {
        tex_null_specification_list(p);
    }
    specification_options(p) = options;
    return p;
}

void tex_dispose_specification_list(halfword a)
{
    if (specification_pointer(a)) {
        switch (node_subtype(a)) {
            case par_passes_code:
                for (int i = 1; i <= specification_count(a); i++) {
                    halfword f = tex_get_passes_fitnessclasses(a, i);
                    if (f) {
                       tex_flush_node(f);
                       tex_set_passes_fitnessclasses(a, i, null);
                    }
                }
                break;
            case balance_shape_code:
                for (int i = 1; i <= specification_count(a); i++) {
                    halfword t = tex_get_balance_topskip(a, i);
                    halfword b = tex_get_balance_bottomskip(a, i);
                    if (t) {
                       tex_flush_node(t);
                       tex_set_balance_topskip(a, i, null);
                    }
                    if (b) {
                       tex_flush_node(b);
                       tex_set_balance_bottomskip(a, i, null);
                    }
                }
                break;
            case balance_passes_code:
                for (int i = 1; i <= specification_count(a); i++) {
                    halfword f = tex_get_balance_passes_fitnessclasses(a, i);
                    if (f) {
                       tex_flush_node(f);
                       tex_set_balance_passes_fitnessclasses(a, i, null);
                    }
                }
                break;
        }
        tex_aux_deallocate_specification(specification_pointer(a), specification_size(a));
        specification_pointer(a) = NULL;
        specification_size(a) = 0;
    }
}

void tex_copy_specification_list(halfword target, halfword source)
{
    if (specification_pointer(source)) {
        size_t size = 0;
        specification_pointer(target) = tex_aux_allocate_specification(source, specification_count(source), &size);
        if (specification_pointer(target) && specification_pointer(source)) {
            /* Aren't these already copied, along with the other fields. */
            specification_count(target) = specification_count(source);
            specification_size(target) = specification_size(source);
            memcpy(specification_pointer(target), specification_pointer(source), size);
            /* */
            switch (node_subtype(target)) {
                case par_passes_code:
                    for (int i = 1; i <= specification_count(source); i++) {
                        halfword f = tex_get_passes_fitnessclasses(source, i);
                        if (f) {
                            halfword c = tex_copy_node(f);
                            tex_set_passes_fitnessclasses(target, i, c);
                        }
                    }
                    break;
                case balance_shape_code:
                    for (int i = 1; i <= specification_count(source); i++) {
                        halfword t = tex_get_balance_topskip(source, i);
                        halfword b = tex_get_balance_bottomskip(source, i);
                        if (t) {
                            halfword c = tex_copy_node(t);
                            tex_set_balance_topskip(target, i, c);
                        }
                        if (b) {
                            halfword c = tex_copy_node(t);
                            tex_set_balance_bottomskip(target, i, c);
                        }
                    }
                    break;
                case balance_passes_code:
                    /* todo */
                    break;
            }
            /* */
        } else {
            specification_pointer(target) = NULL;
            specification_size(target) = 0;
        }
    } else {
        specification_pointer(target) = NULL;
        specification_size(target) = 0;
    }
}

void tex_dispose_specification_nodes(void)
{
    if (par_shape_par) {
        tex_flush_node(par_shape_par);
        par_shape_par = null;
    }
    if (par_passes_exception_par) {
        tex_flush_node(par_passes_exception_par);
        par_passes_exception_par = null;
    }
}

# define specification_version (specificationspec_cmd * 1000000 + specification_reference_cmd * 1000 + 1)

void tex_dump_specification_data(dumpstream f) {
    int total = 0;
    dump_via_int(f, specification_version);
    for (int cs = 0; cs < (eqtb_size + lmt_hash_state.hash_data.ptr + 1); cs++) {
        int code = eq_type(cs);
        switch (code) {
            case specificationspec_cmd:
            case specification_reference_cmd:
                {
                    halfword value = eq_value(cs);
                    if (value) {
                        halfword subtype = node_subtype(value);
                        halfword size = specification_size(value);
                        dump_int(f, code);
                        dump_int(f, cs);
                        dump_int(f, subtype);
                        dump_int(f, specification_count(value));
                        dump_int(f, specification_options(value));
                        dump_int(f, specification_size(value));
                        dump_int(f, specification_anything_1(value));
                        dump_int(f, specification_anything_2(value));
                        /* dump_mem */
                        if (size) {
                         // dump_things(f, specification_pointer(value), size); // /sizeof(memoryword));
                            dump_items(f, specification_pointer(value), 1, size); // /sizeof(memoryword));
                        }
                        /*tex Nodes are already dumped so the stired pointer is not valid! */
                        if (specification_pointer(value)) {
                            lmt_memory_free(specification_pointer(value));
                            specification_pointer(value) = NULL;
                        }
                        ++total;
                    }
                    break;
                }
        }
    }
    dump_via_int(f, 0);
    dump_int(f, total);
}

void tex_undump_specification_data(dumpstream f) {
    int version = 0;
    undump_int(f, version);
    if (version == specification_version) {
        int total = 0;
        int check = 0;
        while (1) {
            int code;
            undump_int(f, code);
            switch (code) {
                case specificationspec_cmd:
                case specification_reference_cmd:
                    {
                        int cs;
                        undump_int(f, cs);
                        if (cs) {
                            /* The node is stored but its list pointer is invalid. */
                            halfword value = eq_value(cs);
                            halfword subtype, count, options, size, unused_1, unused_2;
                            undump_int(f, subtype);
                            undump_int(f, count);
                            undump_int(f, options);
                            undump_int(f, size);
                            undump_int(f, unused_1);
                            undump_int(f, unused_2);
                            if (value) {
                                if (specification_size(value) == size) {
                                    specification_anything_1(value) = unused_1;
                                    specification_anything_2(value) = unused_2;
                                    if (size) {
                                        specification_pointer(value) = lmt_memory_malloc(size);
                                     // undump_things(f, specification_pointer(value), size); // /sizeof(memoryword));
                                        undump_items(f, specification_pointer(value), 1, size); // /sizeof(memoryword));
                                    } else {
                                        specification_pointer(value) = NULL;
                                    }
                                    ++total;
                                } else {
                                    tex_fatal_undump_error("specifications (size)");
                                }
                            } else {
                                tex_fatal_undump_error("specifications (memory)");
                            }
                        } else {
                            tex_fatal_undump_error("specifications (csname)");
                        }
                    }
                    break;
                default:
                    goto DONE;
            }
        }
      DONE:
        undump_int(f, check);
        if (check != total) {
            tex_fatal_undump_error("specifications (total)");
        }
    } else {
        tex_fatal_undump_error("specifications (version)");
    }
}

void tex_shift_specification_list(halfword shape, int n, int rotate)
{
    /*tex Maybe we should have an extra check for it being a parshape indeed. */
    if (specification_pointer(shape)) {
        halfword count = specification_count(shape);
        if (rotate) {
            if (n > 0 && count > 0 && n < count && count != n) {
                size_t size = 0;
                memoryword *target = tex_aux_allocate_specification(shape, count, &size);
                memoryword *source = specification_pointer(shape);
                halfword m = count - n;
                memcpy(target, source + n, m * sizeof(memoryword));
                memcpy(target + m, source, n * sizeof(memoryword));
                tex_aux_deallocate_specification(source, (halfword) size);
                specification_pointer(shape) = target;
            }
        } else {
            /* changed: zero check, else we wipe */
            if (n > 0) {
                size_t oldsize = 0;
                size_t newsize = 0;
                memoryword *target = NULL;
                memoryword *source = specification_pointer(shape);
                halfword slice = 0;
                if (count > 0 && n < count) {
                    slice = count - n;
                    target = tex_aux_allocate_specification(shape, slice, &newsize);
                    memcpy(target, source + n, newsize);
                }
                if (oldsize > 0) {
                    tex_aux_deallocate_specification(source, (halfword) oldsize);
                }
                specification_pointer(shape) = target;
                specification_count(shape) = slice;
                specification_size(shape) = (halfword) newsize;
            }
        }
    }
}

int tex_flatten_leaders(halfword box, int grp, int just_pack, int location, int checkline)
{
    halfword head = box ? box_list(box) : null;
    if (head) {
        halfword current = head;
        int count = 0;
        while (current) {
            halfword next = node_next(current);
            if (node_type(current) == glue_node && node_subtype(current) == u_leaders) {
                if (! checkline || tex_has_glue_option(current, glue_option_u_leaders_line)) {
                    halfword prev = node_prev(current);
                    halfword leader = glue_leader_ptr(current);
                    if (leader && (node_type(leader) == hlist_node || node_type(leader) == vlist_node)) {
                        halfword packed = null;
                        halfword amount = glue_amount(current);
                        halfword callback = glue_callback(current);
                        double width = (double) amount;
                        switch (box_glue_sign(box)) {
                            case stretching_glue_sign:
                                if (glue_stretch_order(current) == box_glue_order(box)) {
                                    width += glue_stretch(current) * (double) box_glue_set(box);
                                }
                                break;
                            case shrinking_glue_sign:
                                if (glue_shrink_order(current) == box_glue_order(box)) {
                                    width -= glue_shrink(current) * (double) box_glue_set(box);
                                }
                                break;
                        }
                        /* maybe just get the dimensions instead of packing */
                        if (node_type(leader) == hlist_node) {
                            packed = tex_hpack(box_list(leader), scaledround(width), packing_exactly, box_direction(leader), holding_none_option, box_limit_none);
                        } else {
                            packed = tex_vpack(box_list(leader), scaledround(width), packing_exactly, 0, box_direction(leader), holding_none_option, NULL);
                        }
                        box_list(leader) = box_list(packed);
                        box_width(leader) = box_width(packed);
                        box_height(leader) = box_height(packed);
                        box_depth(leader) = box_depth(packed);
                        box_glue_order(leader) = box_glue_order(packed);
                        box_glue_sign(leader) = box_glue_sign(packed);
                        box_glue_set(leader) = box_glue_set(packed);
                        set_box_package_state(leader, package_u_leader_set);
                        box_list(packed) = null;
                        tex_flush_node(packed);
                        glue_leader_ptr(current) = null;
                        tex_flush_node(current);
                        if (callback && ! just_pack) {
                            node_prev(leader) = null;
                            node_next(leader) = null;
                            leader = lmt_uleader_callback(leader, grp, callback, box, location);
                        }
                        tex_try_couple_nodes(leader, next);
                        if (current == head) {
                            box_list(box) = leader;
                        } else {
                            tex_try_couple_nodes(prev, leader);
                        }
                        count += 1;
                    }
                }
            }
            current = next;
        }
        return count;
    } else {
        return 0;
    }
}

/*tex
    This could of course be done in a \LUA\ loop but this is likely to be applied always so we
    provide a helper, also because we need to check the font. Adding this sort of violates the
    principle that we should this in \LUA\ instead but this time I permits myself to cheat.
*/

void tex_soften_hyphens(halfword head, int *found, int *replaced)
{
    halfword current = head;
    while (current) {
        switch (node_type(current)) {
            case glyph_node:
                {
                    if (glyph_character(current) == 0x2D) {
                        ++(*found);
                     // switch (glyph_discpart(current)) {
                     //     case glyph_discpart_unset:
                     //         /*tex Never seen by any disc handler. */
                     //         set_glyph_discpart(current, glyph_discpart_always);
                     //         break;
                     //     case glyph_discpart_always:
                     //         /*tex A hard coded - in the input. */
                     //         break;
                     //     default :
                                if (glyph_disccode(current) == glyph_disc_syllable && tex_char_exists(glyph_font(current), 0xAD)) {
                                    ++(*replaced);
                                    glyph_character(current) = 0xAD;
                                }
                                break;
                     // }
                    }
                    break;
                }
            case hlist_node:
            case vlist_node:
                {
                    halfword list = box_list(current);
                    if (list) {
                        tex_soften_hyphens(list, found, replaced);
                    }
                    break;
                }
        }
        current = node_next(current);
    }
}

halfword tex_harden_spaces(halfword head, halfword tolerance, int* count)
{
    /* todo: take the context code */
    (void) tolerance;
    (void) count;
    return head;
}

halfword tex_get_special_node_list(special_node_list_types list, halfword *tail)
{
    halfword h = null;
    halfword t = null;
    switch (list) {
        case page_insert_list_type:
            h = node_next(page_insert_head);
            if (h == page_insert_head) {
                h = null;
            }
            break;
        case contribute_list_type:
            h = node_next(contribute_head);
            break;
        case page_list_type:
            h = node_next(page_head);
            t = lmt_page_builder_state.tail;
            break;
        case temp_list_type:
            h = node_next(temp_head);
            break;
        case hold_list_type:
            h = node_next(hold_head);
            break;
        case post_adjust_list_type:
            h = node_next(post_adjust_head);
            t = lmt_packaging_state.post_adjust_tail;
            break;
        case pre_adjust_list_type:
            h = node_next(pre_adjust_head);
            t = lmt_packaging_state.pre_adjust_tail;
            break;
        case post_migrate_list_type:
            h = node_next(post_migrate_head);
            t = lmt_packaging_state.post_migrate_tail;
            break;
        case pre_migrate_list_type:
            h = node_next(pre_migrate_head);
            t = lmt_packaging_state.pre_migrate_tail;
            break;
        case align_list_type:
            h = node_next(align_head);
            break;
        case page_discards_list_type:
            h = lmt_packaging_state.page_discards_head;
            break;
        case split_discards_list_type:
            h = lmt_packaging_state.split_discards_head;
            break;
    }
    node_prev(h) = null;
    if (tail) {
        *tail = t ? t : (h ? tex_tail_of_node_list(h) : null);
    }
    return h;
};

int tex_is_special_node_list(halfword n, int *istail)
{
    if (istail) {
        *istail = 0;
    }
    if (! n) {
        return -1;
    } else if (n == node_next(page_insert_head)) {
        return page_insert_list_type;
    } else if (n == node_next(contribute_head)) {
        return contribute_list_type;
    } else if (n == node_next(page_head) || n == lmt_page_builder_state.tail) {
        if (istail && n == lmt_page_builder_state.tail) {
            *istail = 0;
        }
        return page_list_type;
    } else if (n == node_next(temp_head)) {
        return temp_list_type;
    } else if (n == node_next(hold_head)) {
        return hold_list_type;
    } else if (n == node_next(post_adjust_head) || n == lmt_packaging_state.post_adjust_tail) {
        if (istail && n == lmt_packaging_state.post_adjust_tail) {
            *istail = 0;
        }
        return post_adjust_list_type;
    } else if (n == node_next(pre_adjust_head) || n == lmt_packaging_state.pre_adjust_tail) {
        if (istail && n == lmt_packaging_state.pre_adjust_tail) {
            *istail = 0;
        }
        return pre_adjust_list_type;
    } else if (n == node_next(post_migrate_head) || n == lmt_packaging_state.post_migrate_tail) {
        if (istail && n == lmt_packaging_state.post_migrate_tail) {
            *istail = 0;
        }
        return post_migrate_list_type;
    } else if (n == node_next(pre_migrate_head) || n == lmt_packaging_state.pre_migrate_tail) {
        if (istail && n == lmt_packaging_state.pre_migrate_tail) {
            *istail = 0;
        }
        return pre_migrate_list_type;
    } else if (n == node_next(align_head)) {
        return align_list_type;
    } else if (n == lmt_packaging_state.page_discards_head) {
        return page_discards_list_type;
    } else if (n == lmt_packaging_state.split_discards_head) {
        return split_discards_list_type;
 // } else if (n == lmt_page_builder_state.best_page_break) {
 //     return 10000;
    } else {
        return -1;
    }
};

void tex_set_special_node_list(special_node_list_types list, halfword head)
{
    switch (list) {
        case page_insert_list_type:
            /*tex This is a circular list where page_insert_head stays. */
            if (head) {
                node_next(page_insert_head) = head;
                node_next(tex_tail_of_node_list(head)) = page_insert_head;
            } else {
                node_next(page_insert_head) = page_insert_head;
            }
            break;
        case contribute_list_type:
            node_next(contribute_head) = head;
            contribute_tail = head ? tex_tail_of_node_list(head) : contribute_head;
            break;
        case page_list_type:
            node_next(page_head) = head;
            lmt_page_builder_state.tail = head ? tex_tail_of_node_list(head) : page_head;
            break;
        case temp_list_type:
            node_next(temp_head) = head;
            break;
        case hold_list_type:
            node_next(hold_head) = head;
            break;
        case post_adjust_list_type:
            node_next(post_adjust_head) = head;
            lmt_packaging_state.post_adjust_tail = head ? tex_tail_of_node_list(head) : post_adjust_head;
            break;
        case pre_adjust_list_type:
            node_next(pre_adjust_head) = head;
            lmt_packaging_state.pre_adjust_tail = head ? tex_tail_of_node_list(head) : pre_adjust_head;
            break;
        case post_migrate_list_type:
            node_next(post_migrate_head) = head;
            lmt_packaging_state.post_migrate_tail = head ? tex_tail_of_node_list(head) : post_migrate_head;
            break;
        case pre_migrate_list_type:
            node_next(pre_migrate_head) = head;
            lmt_packaging_state.pre_migrate_tail = head ? tex_tail_of_node_list(head) : pre_migrate_head;
            break;
        case align_list_type:
            node_next(align_head) = head;
            break;
        case page_discards_list_type:
            lmt_packaging_state.page_discards_head = head;
            break;
        case split_discards_list_type:
            lmt_packaging_state.split_discards_head = head;
            break;
    }
}

static inline scaled tex_aux_effective_glue(halfword parent, scaled amount, scaled stretch, scaled shrink, halfword stretch_order, halfword shrink_order)
{
    if (parent) {
        singleword sign = box_glue_sign(parent);
        if (sign != normal_glue_sign) {
            switch (node_type(parent)) {
                case hlist_node:
                case vlist_node:
                    {
                        double w = (double) amount;
                        switch (sign) {
                            case stretching_glue_sign:
                                if (stretch_order == box_glue_order(parent)) {
                                    w += stretch * (double) box_glue_set(parent);
                                }
                                break;
                            case shrinking_glue_sign:
                                if (shrink_order == box_glue_order(parent)) {
                                    w -= shrink * (double) box_glue_set(parent);
                                }
                                break;
                        }
                        amount = (scaled) lmt_roundedfloat(w);
                    }
            }
        }
    }
    return amount;
}

scaled tex_effective_glue(halfword parent, halfword n)
{
    if (parent && n) {
        switch (node_type(n)) {
            case glue_node:
            case glue_spec_node:
                return tex_aux_effective_glue(parent, glue_amount(n), glue_stretch(n), glue_shrink(n), glue_stretch_order(n), glue_shrink_order(n));
            case math_node:
                return tex_aux_effective_glue(parent, math_amount(n), math_stretch(n), math_shrink(n), math_stretch_order(n), math_shrink_order(n));
        }
    }
    return 0;
}
