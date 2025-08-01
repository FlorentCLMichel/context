/*
    See license.txt in the root of this project.
*/

# ifndef LMT_PACKAGING_H
# define LMT_PACKAGING_H

# include "luametatex.h"

/* We define some constants used when calling |hpack| to deal with font expansion. */

typedef enum hpack_packing_modes {
    packing_exactly,    /*tex a box dimension is pre-specified */
    packing_additional, /*tex a box dimension is increased from the natural one */
    packing_expanded,   /*tex calculate amount for font expansion after breaking paragraph into lines */
    packing_substitute, /*tex substitute fonts */
    packing_adapted, 
    packing_linebreak,  /*tex signals that we need to take the frozen adjust properties */
 // packing_trial, 
} hpack_packing_modes;

typedef enum box_codes {
    box_code,      /*tex |chr_code| for |\box| */
    copy_code,     /*tex |chr_code| for |\copy| */
    unpack_code,
    last_box_code, /*tex |chr_code| for |\lastbox| */
    tsplit_code,   
    vsplit_code,   /*tex |chr_code| for |\vsplit| */
    dsplit_code,  
 /* hsplit_code, */ 
    tpack_code,    
    vpack_code,    /*tex |chr_code| for |\vpack| */
    dpack_code,
    hpack_code,
    vtop_code,     
    vbox_code,     /*tex |chr_code| for |\vbox| */
    dbox_code,
    hbox_code,
    vbalance_code,
    vbalanced_box_code,
    vbalanced_top_code,
    vbalanced_insert_code,
    vbalanced_discard_code,
    vbalanced_deinsert_code,
    vbalanced_reinsert_code,
    flush_mvl_box_code,
    insert_box_code,
    insert_copy_code,
    local_left_box_box_code,
    local_right_box_box_code,
    local_middle_box_box_code,
    page_discards_code,
    split_discards_code,
    copy_split_discards_code,
} box_codes;

// typedef enum saved_full_spec_items {
//     saved_full_spec_item_context     =  0,
//     saved_full_spec_item_packaging   =  1,
//     saved_full_spec_item_direction   =  2,
//     saved_full_spec_item_attr_list   =  3,
//     saved_full_spec_item_only_pack   =  4,
//     saved_full_spec_item_orientation =  5,
//     saved_full_spec_item_anchor      =  6,
//     saved_full_spec_item_geometry    =  7,
//     saved_full_spec_item_xoffset     =  8,
//     saved_full_spec_item_yoffset     =  9,
//     saved_full_spec_item_xmove       = 10,
//     saved_full_spec_item_ymove       = 11,
//     saved_full_spec_item_reverse     = 12,
//     saved_full_spec_item_container   = 13,
//     saved_full_spec_item_shift       = 14, /* cleaner than passing it as context */
//     saved_full_spec_item_source      = 15,
//     saved_full_spec_item_target      = 16,
//     saved_full_spec_item_axis        = 17,
//     saved_full_spec_item_class       = 18,
//     saved_full_spec_item_state       = 19,
//     saved_full_spec_item_retain      = 20,
//     saved_full_spec_item_callback    = 21,
//     saved_full_spec_n_of_items       = 22,
// } saved_full_spec_items;

typedef enum holding_migration_options {
    holding_none_option    = 0x00,
    holding_marks_option   = 0x01,
    holding_inserts_option = 0x02,
    holding_adjusts_option = 0x04,
} holding_migration_options  ;  

# define retain_marks(r)   (((r | holding_migrations_par) & holding_marks_option  ) == holding_marks_option  )
# define retain_inserts(r) (((r | holding_migrations_par) & holding_inserts_option) == holding_inserts_option)
# define retain_adjusts(r) (((r | holding_migrations_par) & holding_adjusts_option) == holding_adjusts_option)

typedef struct packaging_state_info {
    scaled   total_stretch[6];       /*tex with one for padding, the results are also used in alignments */
    scaled   total_shrink[6];        /*tex glue found by |hpack| or |vpack|, the results are also used in alignments */
    int      last_badness;           /*tex badness of the most recently packaged box */
    scaled   last_overshoot;
    halfword post_adjust_tail;       /*tex tail of adjustment list */
    halfword pre_adjust_tail;
    halfword post_migrate_tail;      /*tex tail of adjustment list */
    halfword pre_migrate_tail;
    halfword last_leftmost_char;
    halfword last_rightmost_char;
    int      pack_begin_line;
    scaled   best_height_plus_depth; /*tex The height of the best box, without stretching or shrinking: */
    halfword previous_char_ptr;
    scaled   font_expansion_ratio;
    halfword page_discards_tail;
    halfword page_discards_head;
    halfword split_discards_head;
    scaled   split_last_height;
    scaled   split_last_depth;
    scaled   split_last_stretch;
    scaled   split_last_shrink;
    halfword except;
} packaging_state_info;

typedef enum box_limit_modes {
    box_limit_none  = 0x00,
    box_limit_hlist = 0x01,
    box_limit_vlist = 0x02,
    box_limit_line  = 0x04,
} box_limit_modes;

extern packaging_state_info lmt_packaging_state;

extern scaled    tex_char_stretch          (halfword p);
extern scaled    tex_char_shrink           (halfword p);
/*     void      tex_get_char_expansion    (halfword p, halfword *stretch, halfword *shrink); */ /* no gain */
extern scaled    tex_kern_stretch          (halfword p);
extern scaled    tex_kern_shrink           (halfword p);
extern scaled    tex_char_protrusion       (halfword p, int side);
/*     void      tex_kern_protrusion       (halfword p, int side, halfword *stretch, halfword *shrink); */
                                           
extern scaled    tex_left_marginkern       (halfword p);
extern scaled    tex_right_marginkern      (halfword p);
                                           
extern halfword  tex_filtered_hpack        (halfword p, halfword qt, scaled w, int m, int grp, halfword d, int just_pack, halfword attr, int state, int retain);
extern halfword  tex_filtered_vpack        (halfword p, scaled h, int m, scaled maxdepth, int grp, halfword direction, int just_pack, halfword attr, int state, int retain, int *excess);
                                           
extern scaledwhd tex_natural_hsizes        (halfword p, halfword pp, glueratio g_mult, int g_sign, int g_order);
extern scaledwhd tex_natural_vsizes        (halfword p, halfword pp, glueratio g_mult, int g_sign, int g_order, int inserts);
extern scaledwhd tex_natural_msizes        (halfword p, int ignoreprime);
extern halfword  tex_natural_width         (halfword p, halfword pp, glueratio g_mult, int g_sign, int g_order);
extern halfword  tex_natural_hsize         (halfword p, halfword *correction);
extern halfword  tex_natural_vsize         (halfword p);
                                           
extern halfword  tex_hpack                 (halfword p, scaled w, int m, singleword d, int retain, int limit);
extern halfword  tex_vpack                 (halfword p, scaled h, int m, scaled l, singleword d, int retain, int *excess);
                                           
extern void      tex_repack                (halfword p, scaled w, int m);
extern void      tex_limit                 (halfword p);
extern void      tex_freeze                (halfword p, int recurse, int limitate, halfword factor);
extern void      tex_migrate               (halfword head, halfword *first, halfword *last, int inserts, int marks);

extern scaled    tex_stretch               (halfword p);
extern scaled    tex_shrink                (halfword p);
                                           
extern void      tex_package               (singleword nature);
extern void      tex_run_unpackage         (void);
                                           
extern void      tex_append_to_vlist       (halfword b, int location, const line_break_properties *properties);
                                           
extern halfword  tex_prune_page_top        (halfword p, int s);
extern halfword  tex_vert_break            (halfword p, scaled height, scaled depth, int callback, scaled extra);
extern halfword  tex_vsplit                (halfword n, scaled h, int m);
                                           
extern void      tex_finish_vcenter_group  (void);
extern void      tex_run_vcenter           (void);
                                           
extern void      tex_show_packaging_group  (const char *package);
extern int       tex_show_packaging_record (void);

extern int       tex_get_packaging_context (void);
extern int       tex_get_packaging_shift   (void);

extern int       tex_is_effectively_empty  (halfword n, halfword options); 

typedef enum effective_empty_options {
    effective_empty_option_par         = 0x0001,
    effective_empty_option_dir         = 0x0002,
    effective_empty_option_indent_list = 0x0004,
    effective_empty_option_indent_glue = 0x0008,
    effective_empty_option_all         = 0xFFFF,
} effective_empty_options ;

//# define vpack(A,B,C,D) tex_vpackage(A,B,C,max_dimension,D)

# define first_un_box_code box_code
# define last_un_box_code  unpack_code
# define first_nu_box_code box_code
# define last_nu_box_code  local_middle_box_box_code /*tex needs checking */

/*tex

    Now let's turn to the question of how |\hbox| is treated. We actually need to consider also a
    slightly larger context, since constructions like

    \starttyping
    \setbox3={\\hbox...
    \leaders\hbox...
    \lower3.8pt\hbox...
    \stoptyping

    are supposed to invoke quite different actions after the box has been packaged. Conversely,
    constructions like |\setbox 3 =| can be followed by a variety of different kinds of boxes, and
    we would like to encode such things in an efficient way.

    In other words, there are two problems: To represent the context of a box, and to represent its
    type. The first problem is solved by putting a \quote {context code} on the |save_stack|, just
    below the two entries that give the dimensions produced by |scan_spec|. The context code is
    either a (signed) shift amount, or it is a large integer |>= box_flag|, where |box_flag = |
    $2^{30}$. Codes |box_flag| through |box_flag + biggest_reg| represent |\setbox0| through
    |\setbox biggest_reg|; codes |box_flag + biggest_reg + 1| through |box_flag + 2 * biggest_reg|
    represent |\global \setbox 0| through |\global\setbox| |biggest_reg|; code |box_flag + 2 *
    number_regs| represents |\shipout|; and codes |box_flag + 2 * number_regs + 1| through |box_flag
    + 2 * number_regs + 3| represent |\leaders|, |\cleaders|, and |\xleaders|.

    The second problem is solved by giving the command code |make_box| to all control sequences that
    produce a box, and by using the following |chr_code| values to distinguish between them:
    |box_code|, |copy_code|, |last_box_code|, |vsplit_code|, |vtop_code|, |vtop_code + vmode|, and
    |vtop_code + hmode|, where the latter two are used denote |\vbox| and |\hbox|, respectively.

    Originally the shift was encoded in the box context in case of a move. In fact even the local 
    and global register assignments were in that property but this is no longer the case. This
    actually makes implementing a |\boxspecdef| cleaner (a discarded experiment). The intermediate 
    cleaned up flags can be found in the history. 

*/

# define biggest_reg 65535  /*tex This could be in |textypes.h|. */

typedef enum box_flags {
    direct_box_flag     = 0x00,
 /* moved_box_flag      = 0x01, */
 /* vcenter_box_flag    = 0x02, */ 
    box_flag            = 0x02, /*tex context code for |\setbox0| */
    global_box_flag     = 0x03, /*tex context code for |\global\setbox0| */
    left_box_flag       = 0x04, /*tex context code for |\localleftbox| */
    right_box_flag      = 0x05, /*tex context code for |\localrightbox| */
    middle_box_flag     = 0x06, /*tex context code for |\localrightbox| */
    shipout_flag        = 0x07, /*tex context code for |\shipout| */
    lua_scan_flag       = 0x08, /*tex context code for |scan_list| */
    a_leaders_flag      = 0x09, /*tex context code for |\leaders| */
    c_leaders_flag      = 0x0A, /*tex context code for |\cleaders| */
    x_leaders_flag      = 0x0B, /*tex context code for |\xleaders| */
    g_leaders_flag      = 0x0C, /*tex context code for |\gleaders| */
    u_leaders_flag      = 0x0D, /*tex context code for |\uleaders| */

} box_flags;

# define box_leaders_flag(f) (f >= a_leaders_flag && f <= u_leaders_flag)

extern void tex_begin_box        (int boxcontext, scaled shift, halfword slot, halfword callback, halfword leaders);
extern int  tex_ignore_math_skip (halfword p);

static inline scaled tex_aux_checked_dimension1(halfword v)
{
    if (v > max_dimension) {
        return max_dimension;
    } else if (v < -max_dimension) {
        return -max_dimension;
    } else {
        return v;
    }
}

static inline scaled tex_aux_checked_dimension2(halfword v)
{
    if (v > max_dimension) {
        return max_dimension;
    } else if (v < 0) {
        return 0;
    } else {
        return v;
    }
}

# endif
