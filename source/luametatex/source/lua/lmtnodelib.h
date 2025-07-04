/*
    See license.txt in the root of this project.
*/

# ifndef LNODELIB_H
# define LNODELIB_H

extern void     lmt_push_node              (lua_State *L);
extern void     lmt_push_node_fast         (lua_State *L, halfword n);
extern void     lmt_push_directornode      (lua_State *L, halfword n, int isdirect);
extern int      lmt_get_math_style         (lua_State *L, int n, int dflt);
extern int      lmt_get_math_parameter     (lua_State *L, int n, int dflt);
extern halfword lmt_optional_isnode        (lua_State *L, int i);
extern halfword lmt_check_isnode           (lua_State *L, int i);
extern halfword lmt_check_isdirect         (lua_State *L, int i);
extern halfword lmt_check_isdirectornode   (lua_State *L, int i, int *isdirect);
extern void     lmt_initialize_properties  (int set_size);

extern void     lmt_node_list_to_lua       (lua_State *L, halfword n); /* only used in lmttexlib */
extern halfword lmt_node_list_from_lua     (lua_State *L, int n);      /* only used in lmttexlib */

extern void     lmt_push_head_to_callback  (lua_State *L, halfword n);
extern void     lmt_push_node_to_callback  (lua_State *L, halfword n);
extern halfword lmt_pop_node_from_callback (lua_State *L, int index);

extern halfword lmt_hpack_callback(
    halfword head,
    scaled   size,
    int      packtype,
    int      extrainfo,
    int      direction,
    halfword a
);

extern halfword lmt_vpack_callback(
    halfword head,
    scaled   size,
    int      packtype,
    scaled   depth,
    int      extrainfo,
    int      direction,
    halfword a
);

extern halfword lmt_packed_vbox_callback(
    halfword box,
    int      extrainfo
);

extern void lmt_around_linebreak_callback(
    int       callback,
    int       extrainfo,
    halfword  head,
    halfword *tail
);

extern int lmt_linebreak_callback(
    halfword  head,
    int       isbroken, /* display_math */
    halfword *newhead
);

extern void lmt_alignment_callback(
    halfword head,
    halfword context,
    halfword callback, 
    halfword attrlist,
    halfword preamble
);

extern void lmt_local_box_callback(
    halfword linebox,
    halfword leftbox,
    halfword rightbox,
    halfword middlebox,
    halfword linenumber,
    scaled   leftskip,
    scaled   rightskip,
    scaled   lefthang,
    scaled   righthang,
    scaled   indentation,
    scaled   parinitleftskip,
    scaled   parinitrightskip,
    scaled   parfillleftskip,
    scaled   parfillrightskip,
    scaled   overshoot
);

extern int lmt_append_to_vlist_callback(
    halfword  box,
    int       location,
    halfword  prevdepth,
    halfword *result,
    int      *nextdepth,
    int      *prevset,
    int      *checkdepth
);

extern void lmt_begin_paragraph_callback(
    int invmode,
    int *indented,
    int context
);

extern void lmt_paragraph_context_callback(
    int context,
    int *ignore
);


extern void lmt_buildpage_callback(
    int      context,
    halfword boundary
);

extern void lmt_append_pre_line_callback(
    void /* dummy function */
);

extern void lmt_append_line_callback(
    void
);

extern void lmt_append_adjust_callback(
    halfword context,
    halfword index
);

extern void lmt_append_migrate_callback(
    halfword context
);

extern int lmt_par_pass_callback(
    halfword               head,
    line_break_properties *properties, 
    halfword               identifier, 
    halfword               subpass, 
    halfword               callback, 
    halfword               features, 
    scaled                 overfull, 
    scaled                 underfull, 
    halfword               verdict, 
    halfword               classified,
    scaled                 threshold, 
    halfword               badness, 
    halfword               classes,
    int                   *repeat
);

extern void lmt_insert_par_callback(
    halfword node,
    halfword mode
);

extern halfword lmt_uleader_callback(
    halfword head,
    halfword index, 
    int      context,
    halfword box,
    int      location
);

extern halfword lmt_uinsert_callback(
    halfword callback, 
    halfword index,
    halfword order,
    halfword packed,
    scaled   height, 
    scaled   amount
);

extern scaled lmt_italic_correction_callback(
    halfword glyph,
    scaled   kern,
    halfword subtype
);

# endif
