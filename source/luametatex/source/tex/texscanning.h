/*
    See license.txt in the root of this project.
*/

# ifndef LMT_SCANNING_H
# define LMT_SCANNING_H

typedef enum value_level_code {
    posit_val_level,
    integer_val_level,       /*tex integer values */
    attribute_val_level,     /*tex integer values */
    dimension_val_level,     /*tex dimension values */
    glue_val_level,          /*tex glue specifications */
    muglue_val_level,        /*tex math glue specifications */
    token_val_level,         /*tex token lists */
    font_val_level ,         /*tex font identifier */
    mathspec_val_level ,
    fontspec_val_level ,
    specification_val_level, /*tex special purpose identifier */
    list_val_level,
    no_val_level,
} value_level_code;

# define first_value_level integer_val_level
# define last_value_level  muglue_val_level

typedef struct scanner_state_info {
    int      current_cmd;       /*tex current command set by |get_next| */
    halfword current_chr;       /*tex operand of current command */
    halfword current_cs;        /*tex control sequence found here, zero if none foucan_tond */
 // halfword current_flag;
    halfword current_tok;       /*tex packed representative of |cur_cmd| and |cur_chr| */
    int      current_val;       /*tex value returned by numeric scanners */
    int      current_val_level; /*tex the level of this value */
    halfword current_box;       /*tex the box to be placed into its context */
    halfword last_cs_name;      /*tex used in |\csname| and |\ifcsname| */
    int      arithmetic_error;
    int      expression_depth;
} scanner_state_info;

extern scanner_state_info lmt_scanner_state;

/*tex
    These are rather basic \TEX\ The Program variables (aliases) so for now we stick to the
    unqualified short names.
*/

# define cur_cmd       lmt_scanner_state.current_cmd
# define cur_chr       lmt_scanner_state.current_chr
# define cur_cs        lmt_scanner_state.current_cs
# define cur_tok       lmt_scanner_state.current_tok
# define cur_val       lmt_scanner_state.current_val
# define cur_val_level lmt_scanner_state.current_val_level
# define cur_box       lmt_scanner_state.current_box

typedef struct full_scanner_status {
    int      save_scanner_status;
    halfword save_def_ref;
    halfword save_warning_index;
} full_scanner_status;

static inline full_scanner_status tex_save_full_scanner_status(void)
{
    full_scanner_status a;
    a.save_scanner_status = lmt_input_state.scanner_status;
    a.save_def_ref        = lmt_input_state.def_ref;
    a.save_warning_index  = lmt_input_state.warning_index;
    return a;
}

static inline void tex_unsave_full_scanner_status(full_scanner_status *a)
{
    lmt_input_state.warning_index  = a->save_warning_index;
    lmt_input_state.def_ref        = a->save_def_ref;
    lmt_input_state.scanner_status = a->save_scanner_status;
}

extern void        tex_scan_something_simple          (halfword cmd, halfword code);
extern void        tex_scan_left_brace                (void);
extern void        tex_scan_optional_equals           (void);
extern int         tex_scan_cardinal                  (int optional_equal, unsigned *value, int dontbark);
extern halfword    tex_scan_integer                   (int optional_equal, int *radix, int *grouped);
extern void        tex_scan_integer_validate          (void);
extern halfword    tex_scan_positive_integer          (int optional_equal);
extern halfword    tex_scan_scale                     (int optional_equal);
extern halfword    tex_scan_scale_factor              (int optional_equal);
extern halfword    tex_scan_posit                     (int optional_equal);
extern halfword    tex_scan_dimension                 (int mu, int inf, int shortcut, int optional_equal, halfword *order, int *grouped);
extern void        tex_scan_dimension_validate        (void);
extern halfword    tex_scan_glue                      (int level, int optional_equal, int options_too);
extern halfword    tex_scan_font                      (int optional_equal);
extern halfword    tex_scan_general_text              (halfword *tail);
/*     halfword    tex_scan_toks                      (int macrodef, int xpand, int left_brace_found); */
extern halfword    tex_scan_toks_normal               (int left_brace_found, halfword *tail);
extern halfword    tex_scan_toks_expand               (int left_brace_found, halfword *tail, int expandconstant, int keepparameters);
extern void        tex_scan_toks_dropped              (int left_brace_found);
extern halfword    tex_scan_macro_normal              (void); // (int tolerant);
extern halfword    tex_scan_macro_expand              (void); // (int tolerant);
extern halfword    tex_scan_font_identifier           (halfword *spec);
extern halfword    tex_scan_fontspec_identifier       (void);
extern halfword    tex_scan_math_style_identifier     (int tolerant, int styles);
extern halfword    tex_scan_math_parameter            (void);
extern halfword    tex_scan_limited_scale             (int optional_equal);
extern halfword    tex_scan_positive_scale            (int optional_equal);
extern halfword    tex_scan_positive_number           (int optional_equal);
extern halfword    tex_scan_parameter_index           (void);

extern void        tex_initialize_units               (void);

extern quarterword tex_scan_direction                 (int optional_equal);
extern halfword    tex_scan_geometry                  (int optional_equal);
extern halfword    tex_scan_orientation               (int optional_equal);
extern halfword    tex_scan_anchor                    (int optional_equal);
extern halfword    tex_scan_anchors                   (int optional_equal);

extern halfword    tex_scan_expr                      (int level);
extern int         tex_scanned_expression             (int level);

extern halfword    tex_scan_integer_register_number   (void);
extern halfword    tex_scan_dimension_register_number (void);
extern halfword    tex_scan_attribute_register_number (void);
extern halfword    tex_scan_posit_register_number     (void);
extern halfword    tex_scan_glue_register_number      (void);
extern halfword    tex_scan_muglue_register_number    (void);
extern halfword    tex_scan_toks_register_number      (void);
extern halfword    tex_scan_box_register_number       (void);
extern halfword    tex_scan_unit_register_number      (int optional_equal);
extern halfword    tex_scan_mark_number               (void);
extern halfword    tex_scan_char_number               (int optional_equal);
extern halfword    tex_scan_math_char_number          (void);
extern halfword    tex_scan_math_family_number        (void);
extern halfword    tex_scan_math_class_number         (int optional_equal);
extern halfword    tex_scan_math_properties_number    (void);
extern halfword    tex_scan_math_group_number         (void);
extern halfword    tex_scan_math_index_number         (void);
extern halfword    tex_scan_math_discretionary_number (int optional_equal);
extern halfword    tex_scan_category_code             (int optional_equal);
extern halfword    tex_scan_classification_code       (int optional_equal);
extern halfword    tex_scan_space_factor              (int optional_equal);
extern singleword  tex_scan_box_index                 (void); /*tex For local boxes: small for now! */
extern singleword  tex_scan_box_axis                  (void);
extern halfword    tex_scan_function_reference        (int optional_equal);
extern halfword    tex_scan_bytecode_reference        (int optional_equal);

extern halfword    tex_the_value_toks                 (int unit, halfword *tail, halfword property); /* returns head */
extern halfword    tex_the_toks                       (int code, halfword *tail); /* returns head */
extern halfword    tex_the_detokenized_toks           (halfword *tail, int expand, int protect);
extern void        tex_detokenize_list                (halfword head);
extern strnumber   tex_the_scanned_result             (void);

extern void        tex_set_font_dimension             (void);
extern halfword    tex_get_font_dimension             (void);
extern void        tex_set_scaled_font_dimension      (int n);
extern halfword    tex_get_scaled_font_dimension      (int n);

extern int         tex_get_unit_class                 (halfword index);

extern int         tex_fract                          (int x, int n, int d, int max_answer);

extern halfword    tex_scan_lua_value                 (int index);

extern int         tex_scan_tex_value                 (halfword level, halfword *value);

extern halfword    tex_scan_attribute                 (halfword attrlist);
extern halfword    tex_scan_extra_attribute           (halfword attrlist);

extern int         tex_valid_userunit                 (halfword cmd, halfword chr, halfword cs);
extern int         tex_get_userunit                   (halfword index, scaled *value);

extern int         tex_quotient                       (int n, int d, int round);

/*
# define token_is_digit(t)       ((t >= zero_token  ) && (t <= nine_token ))
# define token_is_xdigit(t)     (((t >= zero_token  ) && (t <= nine_token )) || \
                                 ((t >= a_token_l   ) && (t <= f_token_l  )) || \
                                 ((t >= A_token_l   ) && (t <= F_token_l  )) || \
                                 ((t >= a_token_o   ) && (t <= f_token_o  )) || \
                                 ((t >= A_token_o   ) && (t <= F_token_o  )))
# define token_is_exponent(t)    ((t == E_token_l   ) || (t == e_token_l  ) || \
                                  (t == E_token_o   ) || (t == e_token_o  ))
# define token_is_xexponent(t)   ((t == P_token_l   ) || (t == p_token_l  )  || \
                                  (t == P_token_o   ) || (t == p_token_o  ))
# define token_is_hexadecimal(t) ((t == X_token_l   ) || (t == x_token_l  )  || \
                                  (t == X_token_o   ) || (t == x_token_o  ))
# define token_is_sign(t)        ((t == minus_token ) || (t == plus_token ))
# define token_is_seperator(t)   ((t == period_token) || (t == comma_token))
*/

static inline int tex_token_is_digit(halfword t)
{
    return (t >= zero_token) && (t <= nine_token);
}

static inline int tex_token_is_xdigit(halfword t) {
    return ((t >= zero_token) && (t <= nine_token))
        || ((t >= a_token_l ) && (t <= f_token_l))
        || ((t >= A_token_l ) && (t <= F_token_l))
        || ((t >= a_token_o ) && (t <= f_token_o))
        || ((t >= A_token_o ) && (t <= F_token_o));
}

static inline int tex_token_is_exponent(halfword t)
{
    return (t == E_token_l) || (t == e_token_l)
        || (t == E_token_o) || (t == e_token_o);
}

static inline int tex_token_is_xexponent(halfword t)
{
    return (t == P_token_l) || (t == p_token_l)
        || (t == P_token_o) || (t == p_token_o);
}

 static inline int tex_token_is_hexadecimal(halfword t)
{
    return (t == X_token_l) || (t == x_token_l)
        || (t == X_token_o) || (t == x_token_o);
}

static inline int tex_token_is_sign(halfword t) {
    return (t == minus_token) || (t == plus_token);
}

static inline int tex_token_is_separator(halfword t) {
    return (t == period_token) || (t == comma_token);
}

static inline int tex_token_is_operator(halfword t) {
    return (t == plus_token) || (t == minus_token) || (t == asterisk_token) || (t == slash_token) || (t == colon_token);
}

static inline int tex_token_is_unit(halfword t) {
    return (t >= a_token_l && t <= z_token_l) || (t >= a_token_o && t <= z_token_o)
        || (t >= A_token_l && t <= Z_token_l) || (t >= A_token_o && t <= Z_token_o);
}

# endif

