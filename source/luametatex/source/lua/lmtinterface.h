/*
    See license.txt in the root of this project.
*/

# ifndef LMT_LINTERFACE_H
# define LMT_LINTERFACE_H

# define lmt_linterface_inline 1

/*tex

    In this file we collect all kind of interface stuff related to \LUA. It is a large file because
    we also create \LUA\ string entries which speeds up the interfacing.

*/
# include "lua.h"
# include "lauxlib.h"
# include "lualib.h"

/*tex Just in case: */

extern int  tex_formatted_error   (const char *t, const char *fmt, ...);
extern void tex_formatted_warning (const char *t, const char *fmt, ...);
extern void tex_emergency_message (const char *t, const char *fmt, ...);

/*tex

    In the beginning we had multiple \LUA\ states but that didn't work out well if you want to
    intercace to the \TEX\ kernel. So, we dropped that and now have only one main state.

*/

typedef struct lua_state_info {
    lua_State   *lua_instance;
    int          used_bytes;
    int          used_bytes_max;
    int          function_table_id;
    int          function_callback_count;
    int          value_callback_count;
    int          bytecode_callback_count;
    int          local_callback_count;
    int          saved_callback_count;
    int          file_callback_count;
    int          direct_callback_count;
    int          message_callback_count;
    int          function_table_size;
    int          bytecode_bytes;
    int          bytecode_max;
    int          version_number;
    int          release_number;
    luaL_Buffer *used_buffer;
    int          integer_size;
} lua_state_info ;

extern lua_state_info lmt_lua_state;

/*tex The libraries are opened and initialized by the following functions. */

extern int  luaopen_aes         (lua_State *L);
extern int  luaopen_basexx      (lua_State *L);
extern int  luaopen_callback    (lua_State *L);
extern int  luaopen_filelib     (lua_State *L);
extern int  luaopen_fio         (lua_State *L);
extern int  luaopen_font        (lua_State *L);
extern int  luaopen_language    (lua_State *L);
extern int  luaopen_library     (lua_State *L);
extern int  luaopen_lpeg        (lua_State *L);
extern int  luaopen_lua         (lua_State *L);
extern int  luaopen_luac        (lua_State *L);
extern int  luaopen_md5         (lua_State *L);
extern int  luaopen_mime_core   (lua_State *L);
extern int  luaopen_mplib       (lua_State *L);
extern int  luaopen_node        (lua_State *L);
extern int  luaopen_helper      (lua_State *L);
extern int  luaopen_optional    (lua_State *L);
extern int  luaopen_pdfdecode   (lua_State *L);
extern int  luaopen_pdfe        (lua_State *L);
extern int  luaopen_pngdecode   (lua_State *L);
extern int  luaopen_posit       (lua_State *L);
extern int  luaopen_potrace     (lua_State *L);
extern int  luaopen_qrcodegen   (lua_State *L);
extern int  luaopen_sha2        (lua_State *L);
extern int  luaopen_sio         (lua_State *L);
extern int  luaopen_socket_core (lua_State *L);
extern int  luaopen_sparse      (lua_State *L);
extern int  luaopen_status      (lua_State *L);
extern int  luaopen_tex         (lua_State *L);
extern int  luaopen_texio       (lua_State *L);
extern int  luaopen_token       (lua_State *L);
extern int  luaopen_xcomplex    (lua_State *L);
extern int  luaopen_xdecimal    (lua_State *L);
extern int  luaopen_xmath       (lua_State *L);
extern int  luaopen_xzip        (lua_State *L);
extern int  luaopen_serial      (lua_State *L);
extern int  luaopen_vector      (lua_State *L);

extern int  luaextend_io        (lua_State *L);
extern int  luaextend_os        (lua_State *L);
extern int  luaextend_string    (lua_State *L);
extern int  luaextend_table     (lua_State *L);
extern int  luaextend_xcomplex  (lua_State *L);

/*tex

    We finetune the string hasher. When playing with \LUAJIT\ we found that its hashes was pretty
    inefficient and geared for \URL's! So there we switched to the \LUA\ hasher an din the process
    we also did experiments with the |LUA_HASHLIMIT| parameter. There's an (already old) article
    about that in one of the \LUATEX\ histories.

 */

# if !defined(LUAI_HASHLIMIT)
# define LUAI_HASHLIMIT 6
# endif

/*tex

    Now comes a large section. Here we create and use macros that preset the access pointers
    (indices) to keys which is faster in comparison. We also handle the arrays that hold the
    information about what fields there are in nodes. This is code from the early times when we
    found out that especially when a lot of access by key happens performance is hit because
    strings get rehashed. It might be that in the meantime \LUA\ suffers less from this but we
    keep this abstraction anyway.

    As we share this mechanism between all modules and because, although it is built from
    components, \LUAMETATEX\ is a whole, we now also moved the \MPLIB\ keys into the same
    namespace which saves code and macros. Some are shared anyway.

    There is also a bit of metatable lookup involved but we only do that for those modules for
    which it's critical.

    Contrary to \LUATEX\ we use a struct to collect the indices and keys instead of global
    pointers. This hides the symbols better.

*/

// todo: add L to some below

# define lua_key_eq(a,b)  (a == lmt_keys.ptr_##b)
# define lua_key_index(a) lmt_keys.idx_##a
# define lua_key(a)       lmt_keys.ptr_##a

# define init_lua_key(L,a) \
    lua_pushliteral(L, #a); \
    lmt_keys.ptr_##a = lua_tolstring(L, -1, NULL); \
    lmt_keys.idx_##a = luaL_ref(L, LUA_REGISTRYINDEX);

# define init_lua_key_alias(L,a,b) \
    lua_pushliteral(L, b); \
    lmt_keys.ptr_##a = lua_tolstring(L, -1, NULL); \
    lmt_keys.idx_##a = luaL_ref(L, LUA_REGISTRYINDEX);

# define make_lua_key_ptr(L,a)         const char *ptr_##a
# define make_lua_key_idx(L,a)         int         idx_##a

# define make_lua_key_ptr_alias(L,a,b) const char *ptr_##a
# define make_lua_key_idx_alias(L,a,b) int         idx_##a

# define make_lua_key
# define make_lua_key_alias

# define lua_push_key(a)          lua_rawgeti(L, LUA_REGISTRYINDEX, lua_key_index(a));
# define lua_push_key_by_index(a) lua_rawgeti(L, LUA_REGISTRYINDEX, a);

# define lua_get_metatablelua(a) do { \
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_key_index(a)); \
    lua_gettable(L, LUA_REGISTRYINDEX); \
} while (0)

# define lua_push_number(L,x)   lua_pushnumber (L, (lua_Number)  (x))
# define lua_push_integer(L,x)  lua_pushinteger(L, (int)         (x))
# define lua_push_halfword(L,x) lua_pushinteger(L, (lua_Integer) (x))

# define lua_push_number_at_index(L,i,x) \
    lua_pushnumber(L, (lua_Number) (x)); \
    lua_rawseti(L, -2, i);

# define lua_push_integer_at_index(L,i,x) \
    lua_pushinteger(L, (x)); \
    lua_rawseti(L, -2, i);

# define lua_push_string_at_index(L,i,s) \
    lua_pushstring(L, s); \
    lua_rawseti(L, -2, i);

# define lua_push_boolean_at_index(L,i,x) \
    lua_pushboolean(L, (x)); \
    lua_rawseti(L, -2, i);

/* why not lua_push_key(k); lua_rawseti(L, -2, i); */

# define lua_push_key_at_index(L,k,i) \
    lua_pushinteger(L, i); \
    lua_push_key(k); \
    lua_rawset(L, -3);

# define lua_push_nil_at_key(L,k) \
    lua_push_key(k); \
    lua_pushnil(L); \
    lua_rawset(L, -3);

# define lua_push_number_at_key(L,k,x) \
    lua_push_key(k); \
    lua_pushnumber(L, (lua_Number) (x)); \
    lua_rawset(L, -3);

# define lua_push_integer_at_key(L,k,x) \
    lua_push_key(k); \
    lua_pushinteger(L, (x)); \
    lua_rawset(L, -3);

# define lua_push_string_at_key(L,k,s) \
    lua_push_key(k); \
    lua_pushstring(L, s); \
    lua_rawset(L, -3);

# define lua_push_lstring_at_key(L,k,s,l) \
    lua_push_key(k); \
    lua_pushlstring(L, s, l); \
    lua_rawset(L, -3);

# define lua_push_boolean_at_key(L,k,b) \
    lua_push_key(k); \
    lua_pushboolean(L, b); \
    lua_rawset(L, -3);

# define lua_push_value_at_key(L,k,v) \
    lua_push_key(k); \
    lua_push_key(v); \
    lua_rawset(L, -3);

# define lua_push_svalue_at_key(L,k,v) \
    lua_push_key(k); \
    lua_push_key_by_index(v); \
    lua_rawset(L, -3);

# define lua_push_specification_at_key(L,k,x) \
    lua_push_key(k); \
    lmt_push_specification(L, x, 0); \
    lua_rawset(L, -3);

/*tex We put some here. These are not public (read: names can change). */

/*tex Used in |lmtnodelib|. */

# define NODE_METATABLE_INSTANCE   "node.instance"
# define NODE_PROPERTIES_DIRECT    "node.properties"
# define NODE_PROPERTIES_INDIRECT  "node.properties.indirect"
# define NODE_PROPERTIES_INSTANCE  "node.properties.instance"

/*tex Used in |lmttokenlib|. */

# define TOKEN_METATABLE_INSTANCE  "token.instance"
# define TOKEN_METATABLE_PACKAGE   "token.package"

/*tex Used in |lmtepdflib|. */

# define PDFE_METATABLE_INSTANCE   "pdfe.instance"
# define PDFE_METATABLE_DICTIONARY "pdfe.dictionary"
# define PDFE_METATABLE_ARRAY      "pdfe.array"
# define PDFE_METATABLE_STREAM     "pdfe.stream"
# define PDFE_METATABLE_REFERENCE  "pdfe.reference"

/*tex Used in |lmtmplib|. */

# define MP_METATABLE_INSTANCE     "mp.instance"
# define MP_METATABLE_FIGURE       "mp.figure"
# define MP_METATABLE_OBJECT       "mp.object"

/*tex Used in |lmtsparselib|. */

# define SPARSE_METATABLE_INSTANCE "sparse.instance"

/*tex Maybe used in |lmtstrlibext| */

# define STRING_BUFFER_INSTANCE "stringbuffer.instance"

/* Various */

# define VECTOR_METATABLE_INSTANCE  "vector"
# define MESH_METATABLE_INSTANCE    "mesh"
# define DECIMAL_METATABLE_INSTANCE "decimal"
# define COMPLEX_METATABLE_INSTANCE "complex"
# define POSIT_METATABLE_INSTANCE   "posit"

/*tex
    There are some more but for now we have no reason to alias them for performance reasons, so
    that got postponed. We then also need to move the defines here:
*/

/*
# define DIR_METATABLE             "file.directory"

# define LUA_BYTECODES_INDIRECT

# define TEX_METATABLE_TEX         "tex.tex"
# define TEX_NEST_INSTANCE         "tex.nest.instance"
# define TEX_*                     "tex.*"

# define LUA_FUNCTIONS             "lua.functions"
# define LUA_BYTECODES             "lua.bytecodes"
# define LUA_BYTECODES_INDIRECT    "lua.bytecodes.indirect"

# define LANGUAGE_METATABLE        "luatex.language"
# define LANGUAGE_FUNCTIONS        "luatex.language.wordhandlers"
*/

/*tex

    Currently we sometimes use numbers and sometimes strings in node properties. We can make that
    consistent by having a check on number and if not then assign a string. The strings are
    prehashed and we make a bunch of lua tables that have these values. We can preassign these at
    startup time.

*/

typedef struct value_info {
    union {
        int     id;
        int     type;
        int     value;
    };
    int         lua;
    const char *name;
} value_info;

typedef struct node_info {
    int           id;
    int           size;
    value_info   *subtypes;
    value_info   *fields;
    const char   *name;
    int           lua;
    int           visible;
    int           first;
    int           last;
} node_info;

typedef struct par_info {
    short cmd;
    short chr;
    int   category;
} par_info;

typedef struct command_item {
    int         id;
    int         lua;
    const char *name;
    int         kind;
    int         min;
    int         max;
    int         base;
    int         fixedvalue;
} command_item;

# define unknown_node    0xFFFF
# define unknown_subtype 0xFFFF
# define unknown_field   0xFFFF
# define unknown_value   0xFFFF

# define set_value_entry_nop(target,n)     target[n].lua = 0;                target[n].name  = NULL;       target[n].value = unknown_value;
# define set_value_entry_key(target,n,k)   target[n].lua = lua_key_index(k); target[n].name  = lua_key(k); target[n].value = n;
# define set_value_entry_lua(target,n,k)   target[n].lua = lua_key_index(k); target[n].name  = lua_key(k);
# define set_value_entry_val(target,n,v,k) target[n].lua = lua_key_index(k); target[n].name  = lua_key(k); target[n].value = v;

extern value_info *lmt_aux_allocate_value_info(size_t last);

typedef struct lmt_interface_info {
    /*tex These are mostly used in lmtnodelib. */
    value_info    *pack_type_values;
    value_info    *group_code_values;
    value_info    *par_context_values;
    value_info    *page_context_values;
 /* value_info    *append_line_context_values; */
    value_info    *append_adjust_context_values;
    value_info    *append_migrate_context_values;
    value_info    *alignment_context_values;
    value_info    *line_break_context_values;
    value_info    *build_context_values;
    value_info    *vsplit_context_values;
    value_info    *par_trigger_values;
    value_info    *par_mode_values;
    value_info    *math_style_name_values;
    value_info    *math_style_variant_values;
 /* value_info    *noad_option_values; */
 /* value_info    *glyph_option_values; */
    /*tex These are mostly used in lmttokenlib. */
    value_info    *lua_function_values;
    value_info    *direction_values;
    value_info    *node_fill_values;
    value_info    *page_contribute_values;
    value_info    *math_style_values;
    value_info    *math_parameter_values;
    value_info    *math_font_parameter_values;
    value_info    *math_indirect_values;
    value_info    *field_type_values;
    /*tex Here's one for nodes. */
    node_info     *node_data;
    par_info      *par_data;
    /*tex And this one is for tokens. */
    command_item  *command_names;
} lmt_interface_info ;

extern lmt_interface_info lmt_interface;

# define lmt_push_pack_type(L,n)              lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.pack_type_values             [n].lua)
# define lmt_push_group_code(L,n)             lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.group_code_values            [n].lua)
# define lmt_push_par_context(L,n)            lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.par_context_values           [n].lua)
# define lmt_push_page_context(L,n)           lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.page_context_values          [n].lua)
//define lmt_push_append_line_context(L,n)    lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.append_line_context_values   [n].lua)
# define lmt_push_append_adjust_context(L,n)  lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.append_adjust_context_values [n].lua)
# define lmt_push_append_migrate_context(L,n) lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.append_migrate_context_values[n].lua)
# define lmt_push_alignment_context(L,n)      lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.alignment_context_values     [n].lua)
# define lmt_push_par_trigger(L,n)            lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.par_trigger_values           [n].lua)
# define lmt_push_par_mode(L,n)               lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.par_mode_values              [n].lua)
# define lmt_push_math_style_name(L,n)        lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_style_name_values       [n].lua)
# define lmt_push_math_style_variant(L,n)     lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_style_variant_values    [n].lua)
# define lmt_push_math_noad_option(L,n)       lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_noad_option_values      [n].lua)
# define lmt_push_lua_function_values(L,n)    lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.lua_function_values          [n].lua)
# define lmt_push_direction(L,n)              lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.direction_values             [n].lua)
# define lmt_push_node_fill(L,n)              lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.node_fill_values             [n].lua)
# define lmt_push_page_contribute(L,n)        lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.page_contribute_values       [n].lua)
# define lmt_push_math_style(L,n)             lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_style_values            [n].lua)
# define lmt_push_math_parameter(L,n)         lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_parameter_values        [n].lua)
# define lmt_push_math_font_parameter(L,n)    lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_font_parameter_values   [n].lua)
# define lmt_push_math_indirect(L,n)          lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.math_indirect_values         [n].lua)
# define lmt_push_field_type(L,n)             lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_interface.field_type_values            [n].lua)

# define lmt_name_of_pack_type(n)              lmt_interface.pack_type_values             [n].name
# define lmt_name_of_group_code(n)             lmt_interface.group_code_values            [n].name
# define lmt_name_of_par_context(n)            lmt_interface.par_context_values           [n].name
# define lmt_name_of_page_context(n)           lmt_interface.page_context_values          [n].name
//define lmt_name_of_append_line_context(n)    lmt_interface.append_line_context_values   [n].name
# define lmt_name_of_append_adjust_context(n)  lmt_interface.append_adjust_context_values [n].name
# define lmt_name_of_append_migrate_context(n) lmt_interface.append_migrate_context_values[n].name
# define lmt_name_of_alignment_context(n)      lmt_interface.alignment_context_values     [n].name
# define lmt_name_of_trigger_begin(n)          lmt_interface.par_trigger_values           [n].name
# define lmt_name_of_par_mode(n)               lmt_interface.par_mode_values              [n].name
# define lmt_name_of_math_style_name(n)        lmt_interface.math_style_name_values       [n].name
# define lmt_name_of_math_style_variant(n)     lmt_interface.math_style_variant_values    [n].name
# define lmt_name_of_math_noad_option(n)       lmt_interface.math_noad_option_values      [n].name
# define lmt_name_of_lua_function_values(n)    lmt_interface.lua_function_values          [n].name
# define lmt_name_of_direction(n)              lmt_interface.direction_values             [n].name
# define lmt_name_of_node_fill(n)              lmt_interface.node_fill_values             [n].name
# define lmt_name_of_page_contribute(n)        lmt_interface.page_contribute_values       [n].name
# define lmt_name_of_math_style(n)             lmt_interface.math_style_values            [n].name
# define lmt_name_of_math_parameter(n)         lmt_interface.math_parameter_values        [n].name
# define lmt_name_of_math_font_parameter(n)    lmt_interface.math_font_parameter_values   [n].name
# define lmt_name_of_math_indirect(n)          lmt_interface.math_indirect_values         [n].name
# define lmt_name_of_field_type(n)             lmt_interface.field_type_values            [n].name

/*tex
    This list will be made smaller because not all values need the boost. Before we define the
    lot we undefine some possibly conflicting snippets. Actually, we don't really define the
    key's here but assemble more complex references to registry indices and variables.

    We could do all keys but some are used only for reporting options (values) so we can skip 
    these (see |lmttexlib.c| for examples of that. 
*/

# undef quad /* CYGWIN */

# define declare_shared_lua_keys(L) \
/* */\
make_lua_key(L, __index);\
make_lua_key(L, __newindex);\
make_lua_key(L, above);\
make_lua_key(L, abovedisplayshortskip);\
make_lua_key(L, abovedisplayskip);\
make_lua_key(L, accent);\
make_lua_key(L, AccentBaseDepth);\
make_lua_key(L, accentbasedepth);\
make_lua_key(L, AccentBaseHeight);\
make_lua_key(L, accentbaseheight);\
make_lua_key(L, AccentBottomOvershoot);\
make_lua_key(L, accentbottomovershoot);\
make_lua_key(L, AccentBottomShiftDown);\
make_lua_key(L, accentbottomshiftdown);\
make_lua_key(L, AccentExtendMargin);\
make_lua_key(L, accentextendmargin);\
make_lua_key(L, accentkern);\
make_lua_key(L, AccentSuperscriptDrop);\
make_lua_key(L, accentsuperscriptdrop);\
make_lua_key(L, AccentSuperscriptPercent);\
make_lua_key(L, accentsuperscriptpercent);\
make_lua_key(L, AccentTopOvershoot);\
make_lua_key(L, accenttopovershoot);\
make_lua_key(L, AccentTopShiftUp);\
make_lua_key(L, accenttopshiftup);\
make_lua_key(L, accentvariant);\
make_lua_key(L, active);\
make_lua_key(L, active_char);\
make_lua_key(L, adapted);\
make_lua_key(L, additional);\
make_lua_key(L, adjacentdemerits);\
make_lua_key(L, adjdemerits);\
make_lua_key(L, adjust);\
make_lua_key(L, adjustedhbox);\
make_lua_key(L, adjustspacing);\
make_lua_key(L, adjustspacingshrink);\
make_lua_key(L, adjustspacingstep);\
make_lua_key(L, adjustspacingstretch);\
make_lua_key(L, advance);\
make_lua_key(L, after_output);\
make_lua_key(L, after_something);\
make_lua_key(L, afterdisplay);\
make_lua_key(L, afterdisplaypenalty);\
make_lua_key(L, afteroutput);\
make_lua_key(L, aliased);\
make_lua_key(L, align);\
make_lua_key(L, alignhead);\
make_lua_key(L, alignment);\
make_lua_key(L, alignment_tab);\
make_lua_key(L, alignrecord);\
make_lua_key(L, alignset);\
make_lua_key(L, alignstack);\
make_lua_key(L, alsosimple);\
make_lua_key(L, analyzed);\
make_lua_key(L, anchor);\
make_lua_key(L, append);\
make_lua_key(L, argument);\
make_lua_key(L, arithmic);\
make_lua_key(L, atop);\
make_lua_key(L, association);\
make_lua_key(L, attr);\
make_lua_key(L, attribute);\
make_lua_key(L, attribute_list);\
make_lua_key(L, attributelist);\
make_lua_key(L, auto);\
make_lua_key(L, automatic);\
make_lua_key(L, auxiliary);\
make_lua_key(L, axis);\
make_lua_key(L, AxisHeight);\
make_lua_key(L, balance);\
make_lua_key(L, balancefinalpenalty);\
make_lua_key(L, balanceslot);\
make_lua_key(L, baselineskip);\
make_lua_key(L, beforedisplay);\
make_lua_key(L, beforedisplaypenalty);\
make_lua_key(L, begin_group);\
make_lua_key(L, begin_local);\
make_lua_key(L, begin_paragraph);\
make_lua_key(L, beginmath);\
make_lua_key(L, beginbrokenmath);\
make_lua_key(L, beginparagraph);\
make_lua_key(L, belowdisplayshortskip);\
make_lua_key(L, belowdisplayskip);\
make_lua_key(L, bend_tolerance);\
make_lua_key(L, bestinsert);\
make_lua_key(L, bestpagebreak);\
make_lua_key(L, bestsize);\
make_lua_key(L, binary);\
make_lua_key(L, boolean);\
make_lua_key(L, bothflexible);\
make_lua_key(L, bottom);\
make_lua_key(L, bottomaccent);\
make_lua_key(L, bottomaccentvariant);\
make_lua_key(L, bottomanchor);\
make_lua_key(L, bottomleft);\
make_lua_key(L, bottomlevel);\
make_lua_key(L, bottommargin);\
make_lua_key(L, bottomovershoot);\
make_lua_key(L, bottomright);\
make_lua_key(L, bottomskip);\
make_lua_key(L, boundary);\
make_lua_key(L, box);\
make_lua_key(L, box_property);\
make_lua_key(L, broken);\
make_lua_key(L, brokeninsert);\
make_lua_key(L, brokenpenalty);\
make_lua_key(L, brokenpenalties);\
make_lua_key(L, bytecode);\
make_lua_key(L, call);\
make_lua_key(L, callback);\
make_lua_key(L, cancel);\
make_lua_key(L, cardinal);\
make_lua_key(L, case_shift);\
make_lua_key(L, catalog);\
make_lua_key(L, Catalog);\
make_lua_key(L, catcode_table);\
make_lua_key(L, category);\
make_lua_key(L, cell);\
make_lua_key(L, center);\
make_lua_key(L, char);\
make_lua_key(L, char_given);\
make_lua_key(L, char_number);\
make_lua_key(L, character);\
make_lua_key(L, characters);\
make_lua_key(L, check);\
make_lua_key(L, checks);\
make_lua_key(L, choice);\
make_lua_key(L, class);\
make_lua_key(L, cleaders);\
make_lua_key(L, close);\
make_lua_key(L, clubpenalties);\
make_lua_key(L, clubpenalty);\
make_lua_key(L, cmd);\
make_lua_key(L, cmdname);\
make_lua_key(L, collapse);\
make_lua_key(L, collect);\
make_lua_key(L, combine_toks);\
make_lua_key(L, command);\
make_lua_key(L, comment);\
make_lua_key(L, compactmath);\
make_lua_key(L, compound);\
make_lua_key(L, compression);\
make_lua_key(L, condition);\
make_lua_key(L, conditional);\
make_lua_key(L, conditionalmathskip);\
make_lua_key(L, connectoroverlapmin);\
make_lua_key(L, constant);\
make_lua_key(L, constant_call);\
make_lua_key(L, container);\
make_lua_key(L, continue);\
make_lua_key(L, contributehead);\
make_lua_key(L, control);\
make_lua_key(L, convert);\
make_lua_key(L, correctionskip);\
make_lua_key(L, cost);\
make_lua_key(L, count);\
make_lua_key(L, cramped);\
make_lua_key(L, crampeddisplay);\
make_lua_key(L, crampedscript);\
make_lua_key(L, crampedscriptscript);\
make_lua_key(L, crampedtext);\
make_lua_key(L, cs_name);\
make_lua_key(L, csname);\
make_lua_key(L, current);\
make_lua_key(L, data);\
make_lua_key(L, dbox);\
make_lua_key(L, deep_frozen_dont_expand);\
make_lua_key(L, deep_frozen_end_template);\
make_lua_key(L, deep_frozen_keep_constant);\
make_lua_key(L, def);\
make_lua_key(L, deferred);\
make_lua_key(L, define_char_code);\
make_lua_key(L, define_family);\
make_lua_key(L, define_font);\
make_lua_key(L, define_lua_call);\
make_lua_key(L, degree);\
make_lua_key(L, degreevariant);\
make_lua_key(L, delete);\
make_lua_key(L, delimited);\
make_lua_key(L, DelimitedSubFormulaMinHeight);\
make_lua_key(L, delimiter);\
make_lua_key(L, delimiter_number);\
make_lua_key(L, delimiterextendmargin);\
make_lua_key(L, DelimiterExtendMargin);\
make_lua_key(L, delimiterover);\
make_lua_key(L, delimiterovervariant);\
make_lua_key(L, DelimiterDisplayPercent);\
make_lua_key(L, DelimiterPercent);\
make_lua_key(L, delimiterpercent);\
make_lua_key(L, DelimiterDisplayShortfall);\
make_lua_key(L, DelimiterShortfall);\
make_lua_key(L, delimitershortfall);\
make_lua_key(L, delimiterunder);\
make_lua_key(L, delimiterundervariant);\
make_lua_key(L, delta);\
make_lua_key(L, demerits);\
make_lua_key(L, denominator);\
make_lua_key(L, denominatorvariant);\
make_lua_key(L, depth);\
make_lua_key(L, depthbefore);\
make_lua_key(L, depthafter);\
make_lua_key(L, designsize);\
make_lua_key(L, dimension);\
make_lua_key(L, dimension_reference);\
make_lua_key(L, dir);\
make_lua_key(L, direct);\
make_lua_key(L, direction);\
make_lua_key(L, directory);\
make_lua_key(L, disc);\
make_lua_key(L, discpart);\
make_lua_key(L, discretionary);\
make_lua_key(L, discretionarypenalty);\
make_lua_key(L, display);\
make_lua_key(L, DisplayOperatorMinHeight);\
make_lua_key(L, displaywidowpenalties);\
make_lua_key(L, displaywidowpenalty);\
make_lua_key(L, doffset);\
make_lua_key(L, doublehyphendemerits);\
make_lua_key(L, doublesuperscript);\
make_lua_key(L, emergencyextrastretch);\
make_lua_key(L, emergencyleftskip);\
make_lua_key(L, emergencyrightskip);\
make_lua_key(L, emergencyshrink);\
make_lua_key(L, emergencystretch);\
make_lua_key(L, empty);\
make_lua_key(L, end);\
make_lua_key(L, endpartokens);\
make_lua_key(L, end_cs_name);\
make_lua_key(L, end_file);\
make_lua_key(L, end_group);\
make_lua_key(L, end_job);\
make_lua_key(L, end_line);\
make_lua_key(L, end_local);\
make_lua_key(L, end_match);\
make_lua_key(L, end_paragraph);\
make_lua_key(L, end_template);\
make_lua_key(L, endmath);\
make_lua_key(L, endbrokenmath);\
make_lua_key(L, equation);\
make_lua_key(L, equation_number);\
make_lua_key(L, equationnumber);\
make_lua_key(L, equationnumberpenalty);\
make_lua_key(L, escape);\
make_lua_key(L, etex);\
make_lua_key(L, exact);\
make_lua_key(L, exactly);\
make_lua_key(L, except);\
make_lua_key(L, exdepth);\
make_lua_key(L, exheight);\
make_lua_key(L, expand_after);\
make_lua_key(L, expandable);\
make_lua_key(L, expanded);\
make_lua_key(L, expansion);\
make_lua_key(L, exhyphenpenalty);\
make_lua_key(L, explicit);\
make_lua_key(L, explicit_space);\
make_lua_key(L, expression);\
make_lua_key(L, extender);\
make_lua_key(L, extensible);\
make_lua_key(L, extra);\
make_lua_key(L, extrahyphenpenalty);\
make_lua_key(L, extraattr);\
make_lua_key(L, extraspace);\
make_lua_key(L, extrasubprescriptshift);\
make_lua_key(L, extrasubprescriptspace);\
make_lua_key(L, extrasubscriptshift);\
make_lua_key(L, extrasubscriptspace);\
make_lua_key(L, extrasuperprescriptshift);\
make_lua_key(L, extrasuperprescriptspace);\
make_lua_key(L, extrasuperscriptshift);\
make_lua_key(L, extrasuperscriptspace);\
make_lua_key(L, fam);\
make_lua_key(L, fence);\
make_lua_key(L, fenced);\
make_lua_key(L, fi);\
make_lua_key(L, fil);\
make_lua_key(L, file);\
make_lua_key(L, fill);\
make_lua_key(L, filll);\
make_lua_key(L, finalhyphendemerits);\
make_lua_key(L, finalpenalty);\
make_lua_key(L, finishrow);\
make_lua_key(L, fireup);\
make_lua_key(L, first);\
make_lua_key(L, fitnessclasses);\
make_lua_key(L, fixedboth);\
make_lua_key(L, fixedbottom);\
make_lua_key(L, fixedsuperandsubscript);\
make_lua_key(L, fixedsuperorsubscript);\
make_lua_key(L, fixedtop);\
make_lua_key(L, flags);\
make_lua_key(L, flataccent);\
make_lua_key(L, FlattenedAccentBaseDepth);\
make_lua_key(L, flattenedaccentbasedepth);\
make_lua_key(L, FlattenedAccentBaseHeight);\
make_lua_key(L, flattenedaccentbaseheight);\
make_lua_key(L, FlattenedAccentBottomShiftDown);\
make_lua_key(L, flattenedaccentbottomshiftdown);\
make_lua_key(L, flattenedaccenttopshiftup);\
make_lua_key(L, FlattenedAccentTopShiftUp);\
make_lua_key(L, float);\
make_lua_key(L, font);\
make_lua_key(L, font_property);\
make_lua_key(L, fontkern);\
make_lua_key(L, fontspec);\
make_lua_key(L, force);\
make_lua_key(L, forcerulethickness);\
make_lua_key(L, fraction);\
make_lua_key(L, FractionDelimiterDisplayStyleSize);\
make_lua_key(L, FractionDelimiterSize);\
make_lua_key(L, fractiondelsize);\
make_lua_key(L, fractiondenomdown);\
make_lua_key(L, FractionDenominatorDisplayStyleGapMin);\
make_lua_key(L, FractionDenominatorDisplayStyleShiftDown);\
make_lua_key(L, FractionDenominatorGapMin);\
make_lua_key(L, FractionDenominatorShiftDown);\
make_lua_key(L, fractiondenomvgap);\
make_lua_key(L, FractionNumeratorDisplayStyleGapMin);\
make_lua_key(L, FractionNumeratorDisplayStyleShiftUp);\
make_lua_key(L, FractionNumeratorGapMin);\
make_lua_key(L, FractionNumeratorShiftUp);\
make_lua_key(L, fractionnumup);\
make_lua_key(L, fractionnumvgap);\
make_lua_key(L, fractionrule);\
make_lua_key(L, FractionRuleThickness);\
make_lua_key(L, fractionvariant);\
make_lua_key(L, frozen);\
make_lua_key(L, function);\
make_lua_key(L, geometry);\
make_lua_key(L, get_mark);\
make_lua_key(L, ghost);\
make_lua_key(L, gleaders);\
make_lua_key(L, global);\
make_lua_key(L, glue);\
make_lua_key(L, glueorder);\
make_lua_key(L, glueset);\
make_lua_key(L, gluesign);\
make_lua_key(L, gluespec);\
make_lua_key(L, glyph);\
make_lua_key(L, group);\
make_lua_key(L, h);\
make_lua_key(L, halign);\
make_lua_key(L, hangafter);\
make_lua_key(L, hangindent);\
make_lua_key(L, hbox);\
make_lua_key(L, hdelimiter);\
make_lua_key(L, head);\
make_lua_key(L, height);\
make_lua_key(L, hextensible);\
make_lua_key(L, hextensiblevariant);\
make_lua_key(L, hlist);\
make_lua_key(L, hfactor);\
make_lua_key(L, hmodepar);\
make_lua_key(L, hmove);\
make_lua_key(L, hoffset);\
make_lua_key(L, holdhead);\
make_lua_key(L, horizontal);\
make_lua_key(L, horizontalmathkern);\
make_lua_key(L, hrule);\
make_lua_key(L, hsize);\
make_lua_key(L, hskip);\
make_lua_key(L, hyphenate);\
make_lua_key(L, hyphenated);\
make_lua_key(L, hyphenation);\
make_lua_key(L, hyphenationmode);\
make_lua_key(L, hyphenchar);\
make_lua_key(L, hyphenpenalty);\
make_lua_key(L, id);\
make_lua_key(L, identifier);\
make_lua_key(L, if_test);\
make_lua_key(L, ifstack);\
make_lua_key(L, ignore);\
make_lua_key(L, ignore_something);\
make_lua_key(L, ignored);\
make_lua_key(L, image);\
make_lua_key(L, immediate);\
make_lua_key(L, immutable);\
make_lua_key(L, indent);\
make_lua_key(L, indentskip);\
make_lua_key(L, index);\
make_lua_key(L, Info);\
make_lua_key(L, info);\
make_lua_key(L, inherited);\
make_lua_key(L, initialize);\
make_lua_key(L, inner);\
make_lua_key(L, innerlocation);\
make_lua_key(L, innerxoffset);\
make_lua_key(L, inneryoffset);\
make_lua_key(L, input);\
make_lua_key(L, insert);\
make_lua_key(L, insertheights);\
make_lua_key(L, insertpenalties);\
make_lua_key(L, instance);\
make_lua_key(L, integer);\
make_lua_key(L, integer_reference);\
make_lua_key(L, interaction);\
make_lua_key(L, interlinepenalties);\
make_lua_key(L, interlinepenalty);\
make_lua_key(L, intermathskip);\
make_lua_key(L, internal_attribute);\
make_lua_key(L, internal_attribute_reference);\
make_lua_key(L, internal_box_reference);\
make_lua_key(L, internal_dimension);\
make_lua_key(L, internal_dimension_reference);\
make_lua_key(L, internal_glue);\
make_lua_key(L, internal_glue_reference);\
make_lua_key(L, internal_integer);\
make_lua_key(L, internal_integer_reference);\
make_lua_key(L, internal_muglue);\
make_lua_key(L, internal_muglue_reference);\
make_lua_key(L, internal_posit);\
make_lua_key(L, internal_posit_reference);\
make_lua_key(L, internal_toks);\
make_lua_key(L, internal_toks_reference);\
make_lua_key(L, internaldimension);\
make_lua_key(L, internalgluespec);\
make_lua_key(L, internalinteger);\
make_lua_key(L, internalmugluespec);\
make_lua_key(L, invalid_char);\
make_lua_key(L, italic);\
make_lua_key(L, italic_correction);\
make_lua_key(L, italiccorrection);\
make_lua_key(L, iterator_value);\
make_lua_key(L, keepbase);\
make_lua_key(L, kern);\
make_lua_key(L, kerns);\
make_lua_key(L, language);\
make_lua_key(L, largechar);\
make_lua_key(L, largefamily);\
make_lua_key(L, last);\
make_lua_key(L, lastinsert);\
make_lua_key(L, lastlinefit);\
make_lua_key(L, leader);\
make_lua_key(L, leaders);\
make_lua_key(L, leastpagecost);\
make_lua_key(L, left);\
make_lua_key(L, left_brace);\
make_lua_key(L, leftboundary);\
make_lua_key(L, leftbox);\
make_lua_key(L, leftboxwidth);\
make_lua_key(L, leftcorrectionkern);\
make_lua_key(L, lefthangskip);\
make_lua_key(L, leftmargin);\
make_lua_key(L, leftmarginkern);\
make_lua_key(L, leftmathslackkern);\
make_lua_key(L, leftprotrusion);\
make_lua_key(L, leftslack);\
make_lua_key(L, leftskip);\
make_lua_key(L, lefttoright);\
make_lua_key(L, lefttwindemerits);\
make_lua_key(L, legacy);\
make_lua_key(L, leftclass);\
make_lua_key(L, let);\
make_lua_key(L, letter);\
make_lua_key(L, level);\
make_lua_key(L, lhmin);\
make_lua_key(L, ligature);\
make_lua_key(L, ligatures);\
make_lua_key(L, limitabovebgap);\
make_lua_key(L, limitabovekern);\
make_lua_key(L, limitabovevgap);\
make_lua_key(L, limitbelowbgap);\
make_lua_key(L, limitbelowkern);\
make_lua_key(L, limitbelowvgap);\
make_lua_key(L, limits);\
make_lua_key(L, line);\
make_lua_key(L, linebreakchecks);\
make_lua_key(L, linebreakoptional);\
make_lua_key(L, linebreakpenalty);\
make_lua_key(L, linedepth);\
make_lua_key(L, lineheight);\
make_lua_key(L, linepenalty);\
make_lua_key(L, lineskip);\
make_lua_key(L, lineskiplimit);\
make_lua_key(L, list);\
make_lua_key(L, lmtxexpression);\
make_lua_key(L, local);\
make_lua_key(L, local_box);\
make_lua_key(L, localbox);\
make_lua_key(L, localbreak);\
make_lua_key(L, log);\
make_lua_key(L, logfile);\
make_lua_key(L, loopstate);\
make_lua_key(L, looseness);\
make_lua_key(L, LowerLimitBaselineDropMin);\
make_lua_key(L, LowerLimitGapMin);\
make_lua_key(L, lua);\
make_lua_key(L, lua_call);\
make_lua_key(L, lua_function_call);\
make_lua_key(L, lua_local_call);\
make_lua_key(L, lua_protected_call);\
make_lua_key(L, lua_semiprotected_call);\
make_lua_key(L, lua_value);\
make_lua_key(L, luametatex);\
make_lua_key(L, luatex);\
make_lua_key(L, macro);\
make_lua_key(L, mainclass);\
make_lua_key(L, make_box);\
make_lua_key(L, margin);\
make_lua_key(L, mark);\
make_lua_key(L, match);\
make_lua_key(L, math);\
make_lua_key(L, math_accent);\
make_lua_key(L, math_char_number);\
make_lua_key(L, math_choice);\
make_lua_key(L, math_component);\
make_lua_key(L, math_fence);\
make_lua_key(L, math_fraction);\
make_lua_key(L, math_modifier);\
make_lua_key(L, math_parameter);\
make_lua_key(L, math_radical);\
make_lua_key(L, math_script);\
make_lua_key(L, math_shift);\
make_lua_key(L, math_shift_cs);\
make_lua_key(L, math_style);\
make_lua_key(L, mathchar);\
make_lua_key(L, mathchoice);\
make_lua_key(L, mathcomponent);\
make_lua_key(L, MathConstants);\
make_lua_key(L, mathcontrol);\
make_lua_key(L, mathdir);\
make_lua_key(L, mathdisplay);\
make_lua_key(L, mathfence);\
make_lua_key(L, mathfraction);\
make_lua_key(L, mathinline);\
make_lua_key(L, mathkern);\
make_lua_key(L, mathkerns);\
make_lua_key(L, MathLeading);\
make_lua_key(L, mathequationnumber);\
make_lua_key(L, mathoperator);\
make_lua_key(L, mathpack);\
make_lua_key(L, mathpenaltyfactor);\
make_lua_key(L, mathpostpenalty);\
make_lua_key(L, mathprepenalty);\
make_lua_key(L, mathradical);\
make_lua_key(L, mathshapekern);\
make_lua_key(L, mathsimple);\
make_lua_key(L, mathskip);\
make_lua_key(L, mathspec);\
make_lua_key(L, mathstack);\
make_lua_key(L, mathstyle);\
make_lua_key(L, mathscale);\
make_lua_key(L, mathbegin);\
make_lua_key(L, mathend);\
make_lua_key(L, mathmode);\
make_lua_key(L, mathflatten);\
make_lua_key(L, mathmainstyle);\
make_lua_key(L, mathparentstyle);\
make_lua_key(L, mathsubformula);\
make_lua_key(L, mathtext);\
make_lua_key(L, mathtextchar);\
make_lua_key(L, maximum);\
make_lua_key(L, medmuskip);\
make_lua_key(L, message);\
make_lua_key(L, method);\
make_lua_key(L, middle);\
make_lua_key(L, middlebox);\
make_lua_key(L, MinConnectorOverlap);\
make_lua_key(L, mirror);\
make_lua_key(L, mkern);\
make_lua_key(L, mode);\
make_lua_key(L, modeline);\
make_lua_key(L, modifier);\
make_lua_key(L, move);\
make_lua_key(L, move_tolerance);\
make_lua_key(L, mrule);\
make_lua_key(L, mskip);\
make_lua_key(L, muglue);\
make_lua_key(L, mugluespec);\
make_lua_key(L, mutable);\
make_lua_key(L, mvl);\
make_lua_key(L, name);\
make_lua_key(L, nepalty);\
make_lua_key(L, nestedlist);\
make_lua_key(L, nestingfactor);\
make_lua_key(L, new);\
make_lua_key(L, next);\
make_lua_key(L, nil);\
make_lua_key(L, no);\
make_lua_key(L, no_expand);\
make_lua_key(L, noad);\
make_lua_key(L, noadstate);\
make_lua_key(L, noalign);\
make_lua_key(L, noaligned);\
make_lua_key(L, node);\
make_lua_key(L, nodelist);\
make_lua_key(L, noindent);\
make_lua_key(L, NoLimitSubFactor);\
make_lua_key(L, nolimitsubfactor);\
make_lua_key(L, nolimitsupfactor);\
make_lua_key(L, NoLimitSupFactor);\
make_lua_key(L, nomath);\
make_lua_key(L, none);\
make_lua_key(L, normal);\
make_lua_key(L, norule);\
make_lua_key(L, nucleus);\
make_lua_key(L, number);\
make_lua_key(L, numerator);\
make_lua_key(L, numeratorvariant);\
make_lua_key(L, off);\
make_lua_key(L, on);\
make_lua_key(L, open);\
make_lua_key(L, operator);\
make_lua_key(L, operatorsize);\
make_lua_key(L, optional);\
make_lua_key(L, options);\
make_lua_key(L, ordinary);\
make_lua_key(L, orientation);\
make_lua_key(L, original);\
make_lua_key(L, orphanlinefactors);\
make_lua_key(L, orphanpenalties);\
make_lua_key(L, orphanpenalty);\
make_lua_key(L, other_char);\
make_lua_key(L, outline);\
make_lua_key(L, output);\
make_lua_key(L, over);\
make_lua_key(L, OverbarExtraAscender);\
make_lua_key(L, overbarkern);\
make_lua_key(L, overbarrule);\
make_lua_key(L, OverbarRuleThickness);\
make_lua_key(L, OverbarVerticalGap);\
make_lua_key(L, overbarvgap);\
make_lua_key(L, overdelimiter);\
make_lua_key(L, overdelimiterbgap);\
make_lua_key(L, overdelimitervariant);\
make_lua_key(L, overdelimitervgap);\
make_lua_key(L, overlayaccent);\
make_lua_key(L, overlayaccentvariant);\
make_lua_key(L, overlinevariant);\
make_lua_key(L, overloaded);\
make_lua_key(L, package);\
make_lua_key(L, packing);\
make_lua_key(L, page);\
make_lua_key(L, page_property);\
make_lua_key(L, pagediscardshead);\
make_lua_key(L, pagehead);\
make_lua_key(L, pageinserthead);\
make_lua_key(L, pages);\
make_lua_key(L, Pages);\
make_lua_key(L, par);\
make_lua_key(L, parameter);\
make_lua_key(L, parameter_reference);\
make_lua_key(L, parameters);\
make_lua_key(L, parfillleftskip);\
make_lua_key(L, parfillrightskip);\
make_lua_key(L, parfillskip);\
make_lua_key(L, parindent);\
make_lua_key(L, parinitleftskip);\
make_lua_key(L, parinitrightskip);\
make_lua_key(L, parpasses);\
make_lua_key(L, parshape);\
make_lua_key(L, parskip);\
make_lua_key(L, parts);\
make_lua_key(L, partsitalic);\
make_lua_key(L, partsorientation);\
make_lua_key(L, passes);\
make_lua_key(L, passive);\
make_lua_key(L, pdfe);\
make_lua_key(L, penalty);\
make_lua_key(L, permanent);\
make_lua_key(L, phantom);\
make_lua_key(L, posit);\
make_lua_key(L, post);\
make_lua_key(L, post_linebreak);\
make_lua_key(L, postadjust);\
make_lua_key(L, postadjusthead);\
make_lua_key(L, postmigrate);\
make_lua_key(L, postmigratehead);\
make_lua_key(L, pre);\
make_lua_key(L, pre_align);\
make_lua_key(L, preadjust);\
make_lua_key(L, preadjusthead);\
make_lua_key(L, preamble);\
make_lua_key(L, prebox);\
make_lua_key(L, prefix);\
make_lua_key(L, premigrate);\
make_lua_key(L, premigratehead);\
make_lua_key(L, prepostsup);\
make_lua_key(L, prepostsub);\
make_lua_key(L, preroll);\
make_lua_key(L, presub);\
make_lua_key(L, presubscriptshiftdistance);\
make_lua_key(L, presup);\
make_lua_key(L, presuperscriptshiftdistance);\
make_lua_key(L, pretolerance);\
make_lua_key(L, prev);\
make_lua_key(L, prevdepth);\
make_lua_key(L, prevgraf);\
make_lua_key(L, prime);\
make_lua_key(L, PrimeBaselineDropMax);\
make_lua_key(L, primeraise);\
make_lua_key(L, primeraisecomposed);\
make_lua_key(L, PrimeRaiseComposedPercent);\
make_lua_key(L, PrimeRaisePercent);\
make_lua_key(L, primeshift);\
make_lua_key(L, primeshiftdrop);\
make_lua_key(L, primeshiftup);\
make_lua_key(L, PrimeShiftUp);\
make_lua_key(L, PrimeShiftUpCramped);\
make_lua_key(L, primespaceafter);\
make_lua_key(L, PrimeSpaceAfter);\
make_lua_key(L, primevariant);\
make_lua_key(L, primewidth);\
make_lua_key(L, PrimeWidthPercent);\
make_lua_key(L, primitive);\
make_lua_key(L, properties);\
make_lua_key(L, protected);\
make_lua_key(L, protected_call);\
make_lua_key(L, protrudechars);\
make_lua_key(L, protrusion);\
make_lua_key(L, ptr);\
make_lua_key(L, punctuation);\
make_lua_key(L, quad);\
make_lua_key(L, quit);\
make_lua_key(L, radical);\
make_lua_key(L, radicaldegreeafter);\
make_lua_key(L, radicaldegreebefore);\
make_lua_key(L, RadicalDegreeBottomRaisePercent);\
make_lua_key(L, radicaldegreeraise);\
make_lua_key(L, RadicalDisplayStyleVerticalGap);\
make_lua_key(L, radicalextensibleafter);\
make_lua_key(L, radicalextensiblebefore);\
make_lua_key(L, RadicalExtraAscender);\
make_lua_key(L, radicalkern);\
make_lua_key(L, RadicalKernAfterDegree);\
make_lua_key(L, RadicalKernAfterExtensible);\
make_lua_key(L, RadicalKernBeforeDegree);\
make_lua_key(L, RadicalKernBeforeExtensible);\
make_lua_key(L, radicalrule);\
make_lua_key(L, RadicalRuleThickness);\
make_lua_key(L, radicalvariant);\
make_lua_key(L, RadicalVerticalGap);\
make_lua_key(L, radicalvgap);\
make_lua_key(L, raise);\
make_lua_key(L, reader);\
make_lua_key(L, register);\
make_lua_key(L, register_attribute);\
make_lua_key(L, register_attribute_reference);\
make_lua_key(L, register_box);\
make_lua_key(L, register_box_reference);\
make_lua_key(L, register_dimension);\
make_lua_key(L, register_dimension_reference);\
make_lua_key(L, register_glue);\
make_lua_key(L, register_glue_reference);\
make_lua_key(L, register_integer);\
make_lua_key(L, register_integer_reference);\
make_lua_key(L, register_muglue);\
make_lua_key(L, register_muglue_reference);\
make_lua_key(L, register_posit);\
make_lua_key(L, register_posit_reference);\
make_lua_key(L, register_toks);\
make_lua_key(L, register_toks_reference);\
make_lua_key(L, registerdimension);\
make_lua_key(L, registergluespec);\
make_lua_key(L, registerinteger);\
make_lua_key(L, registermugluespec);\
make_lua_key(L, regular);\
make_lua_key(L, relation);\
make_lua_key(L, relax);\
make_lua_key(L, remove_item);\
make_lua_key(L, repeat);\
make_lua_key(L, rpnexpression);\
make_lua_key(L, replace);\
make_lua_key(L, report);\
make_lua_key(L, reserved);\
make_lua_key(L, reset);\
make_lua_key(L, rhmin);\
make_lua_key(L, right);\
make_lua_key(L, right_brace);\
make_lua_key(L, rightboundary);\
make_lua_key(L, rightbox);\
make_lua_key(L, rightboxwidth);\
make_lua_key(L, rightclass);\
make_lua_key(L, rightcorrectionkern);\
make_lua_key(L, righthangskip);\
make_lua_key(L, rightmargin);\
make_lua_key(L, rightmarginkern);\
make_lua_key(L, rightmathslackkern);\
make_lua_key(L, rightprotrusion);\
make_lua_key(L, rightslack);\
make_lua_key(L, rightskip);\
make_lua_key(L, righttoleft);\
make_lua_key(L, righttwindemerits);\
make_lua_key(L, root);\
make_lua_key(L, rooted);\
make_lua_key(L, rule);\
make_lua_key(L, rulebasedmathskip);\
make_lua_key(L, ruledepth);\
make_lua_key(L, ruleheight);\
make_lua_key(L, same);\
make_lua_key(L, saved);\
make_lua_key(L, scale);\
make_lua_key(L, script);\
make_lua_key(L, scriptkern);\
make_lua_key(L, scriptorder);\
make_lua_key(L, ScriptPercentScaleDown);\
make_lua_key(L, scripts);\
make_lua_key(L, scriptscale);\
make_lua_key(L, scriptstate);\
make_lua_key(L, scriptxscale);\
make_lua_key(L, scriptyscale);\
make_lua_key(L, scriptweight);\
make_lua_key(L, scriptscript);\
make_lua_key(L, ScriptScriptPercentScaleDown);\
make_lua_key(L, scriptscriptscale);\
make_lua_key(L, scriptscriptxscale);\
make_lua_key(L, scriptscriptyscale);\
make_lua_key(L, scriptscriptweight);\
make_lua_key(L, second);\
make_lua_key(L, semi_protected_call);\
make_lua_key(L, semiprotected);\
make_lua_key(L, semisimple);\
make_lua_key(L, set);\
make_lua_key(L, set_box);\
make_lua_key(L, set_font);\
make_lua_key(L, sffactor);\
make_lua_key(L, sfstretchfactor);\
make_lua_key(L, shape);\
make_lua_key(L, shapingpenaltiesmode);\
make_lua_key(L, shapingpenalty);\
make_lua_key(L, shift);\
make_lua_key(L, shorthand_def);\
make_lua_key(L, shrink);\
make_lua_key(L, shrinkorder);\
make_lua_key(L, sign);\
make_lua_key(L, simple);\
make_lua_key(L, single);\
make_lua_key(L, singlelinepenalty);\
make_lua_key(L, size);\
make_lua_key(L, skewchar);\
make_lua_key(L, skewed);\
make_lua_key(L, SkewedDelimiterTolerance);\
make_lua_key(L, skeweddelimitertolerance);\
make_lua_key(L, skewedfractionhgap);\
make_lua_key(L, SkewedFractionHorizontalGap);\
make_lua_key(L, SkewedFractionVerticalGap);\
make_lua_key(L, skewedfractionvgap);\
make_lua_key(L, skip);\
make_lua_key(L, slant);\
make_lua_key(L, small);\
make_lua_key(L, smallchar);\
make_lua_key(L, smaller);\
make_lua_key(L, smallfamily);\
make_lua_key(L, some_item);\
make_lua_key(L, source);\
make_lua_key(L, space);\
make_lua_key(L, SpaceAfterScript);\
make_lua_key(L, spaceafterscript);\
make_lua_key(L, spacebeforescript);\
make_lua_key(L, spacebetweenscript);\
make_lua_key(L, SpaceBeforeScript);\
make_lua_key(L, SpaceBetweenScript);\
make_lua_key(L, spacefactor);\
make_lua_key(L, spacefontkern);\
make_lua_key(L, spacer);\
make_lua_key(L, spaceshrink);\
make_lua_key(L, spaceskip);\
make_lua_key(L, spacestretch);\
make_lua_key(L, spacing);\
make_lua_key(L, span);\
make_lua_key(L, spec);\
make_lua_key(L, specification);\
make_lua_key(L, specificationspec);\
make_lua_key(L, specification_reference);\
make_lua_key(L, split);\
make_lua_key(L, split_insert);\
make_lua_key(L, splitbottom);\
make_lua_key(L, splitdiscardshead);\
make_lua_key(L, splitfirst);\
make_lua_key(L, splitkeep);\
make_lua_key(L, splitoff);\
make_lua_key(L, splittopskip);\
make_lua_key(L, stack);\
make_lua_key(L, StackBottomDisplayStyleShiftDown);\
make_lua_key(L, StackBottomShiftDown);\
make_lua_key(L, stackdenomdown);\
make_lua_key(L, StackDisplayStyleGapMin);\
make_lua_key(L, StackGapMin);\
make_lua_key(L, stacknumup);\
make_lua_key(L, StackTopDisplayStyleShiftUp);\
make_lua_key(L, StackTopShiftUp);\
make_lua_key(L, stackvariant);\
make_lua_key(L, stackvgap);\
make_lua_key(L, start);\
make_lua_key(L, state);\
make_lua_key(L, step);\
make_lua_key(L, stop);\
make_lua_key(L, stretch);\
make_lua_key(L, stretched);\
make_lua_key(L, stretchorder);\
make_lua_key(L, StretchStackBottomShiftDown);\
make_lua_key(L, StretchStackGapAboveMin);\
make_lua_key(L, StretchStackGapBelowMin);\
make_lua_key(L, StretchStackTopShiftUp);\
make_lua_key(L, string);\
make_lua_key(L, strut);\
make_lua_key(L, style);\
make_lua_key(L, sub);\
make_lua_key(L, subbox);\
make_lua_key(L, submlist);\
make_lua_key(L, subpre);\
make_lua_key(L, subscript);\
make_lua_key(L, subshift);\
make_lua_key(L, SubscriptBaselineDropMin);\
make_lua_key(L, subscriptshiftdistance);\
make_lua_key(L, SubscriptShiftDown);\
make_lua_key(L, SubscriptShiftDownWithSuperscript);\
make_lua_key(L, SubscriptTopMax);\
make_lua_key(L, SubscriptSnap);\
make_lua_key(L, subscriptsnap);\
make_lua_key(L, subscriptvariant);\
make_lua_key(L, subshiftdown);\
make_lua_key(L, subshiftdrop);\
make_lua_key(L, substitute);\
make_lua_key(L, SubSuperscriptGapMin);\
make_lua_key(L, subsupshiftdown);\
make_lua_key(L, subsupvgap);\
make_lua_key(L, subtopmax);\
make_lua_key(L, subtype);\
make_lua_key(L, sup);\
make_lua_key(L, supbottommin);\
make_lua_key(L, superscript);\
make_lua_key(L, SuperscriptBaselineDropMax);\
make_lua_key(L, SuperscriptBottomMaxWithSubscript);\
make_lua_key(L, SuperscriptBottomMin);\
make_lua_key(L, superscriptshiftdistance);\
make_lua_key(L, SuperscriptShiftUp);\
make_lua_key(L, SuperscriptShiftUpCramped);\
make_lua_key(L, SuperscriptSnap);\
make_lua_key(L, superscriptsnap);\
make_lua_key(L, superscriptvariant);\
make_lua_key(L, suppre);\
make_lua_key(L, supshift);\
make_lua_key(L, supshiftdrop);\
make_lua_key(L, supshiftup);\
make_lua_key(L, supsubbottommax);\
make_lua_key(L, surround);\
make_lua_key(L, syllable);\
make_lua_key(L, tabskip);\
make_lua_key(L, tag);\
make_lua_key(L, tail);\
make_lua_key(L, target);\
make_lua_key(L, temp);\
make_lua_key(L, temphead);\
make_lua_key(L, terminal);\
make_lua_key(L, terminal_and_logfile);\
make_lua_key(L, tex);\
make_lua_key(L, tex_nest);\
make_lua_key(L, text);\
make_lua_key(L, textcontrol);\
make_lua_key(L, textscale);\
make_lua_key(L, textxscale);\
make_lua_key(L, textyscale);\
make_lua_key(L, textweight);\
make_lua_key(L, the);\
make_lua_key(L, thickmuskip);\
make_lua_key(L, thickness);\
make_lua_key(L, thinmuskip);\
make_lua_key(L, toddlerpenalty);\
make_lua_key(L, toddlerpenalties);\
make_lua_key(L, tok);\
make_lua_key(L, token);\
make_lua_key(L, tokenlist);\
make_lua_key(L, tolerance);\
make_lua_key(L, tolerant);\
make_lua_key(L, tolerant_call);\
make_lua_key(L, tolerant_protected_call);\
make_lua_key(L, tolerant_semi_protected_call);\
make_lua_key(L, top);\
make_lua_key(L, topaccent);\
make_lua_key(L, topaccentvariant);\
make_lua_key(L, topanchor);\
make_lua_key(L, topleft);\
make_lua_key(L, topmargin);\
make_lua_key(L, topovershoot);\
make_lua_key(L, topright);\
make_lua_key(L, topskip);\
make_lua_key(L, total);\
make_lua_key(L, tracingbalancing);\
make_lua_key(L, tracingparagraphs);\
make_lua_key(L, tracingpasses);\
make_lua_key(L, tracingfitness);\
make_lua_key(L, trailer);\
make_lua_key(L, Trailer);\
make_lua_key(L, triggered);\
make_lua_key(L, type);\
make_lua_key(L, uchyph);\
make_lua_key(L, uleaders);\
make_lua_key(L, un_hbox);\
make_lua_key(L, un_vbox);\
make_lua_key(L, undefined_cs);\
make_lua_key(L, under);\
make_lua_key(L, UnderbarExtraDescender);\
make_lua_key(L, underbarkern);\
make_lua_key(L, underbarrule);\
make_lua_key(L, UnderbarRuleThickness);\
make_lua_key(L, UnderbarVerticalGap);\
make_lua_key(L, underbarvgap);\
make_lua_key(L, underdelimiter);\
make_lua_key(L, underdelimiterbgap);\
make_lua_key(L, underdelimitervariant);\
make_lua_key(L, underdelimitervgap);\
make_lua_key(L, underlinevariant);\
make_lua_key(L, unhbox);\
make_lua_key(L, unhyphenated);\
make_lua_key(L, unit_reference);\
make_lua_key(L, unknown);\
make_lua_key(L, unset);\
make_lua_key(L, untraced);\
make_lua_key(L, unvbox);\
make_lua_key(L, uppercase);\
make_lua_key(L, UpperLimitBaselineRiseMin);\
make_lua_key(L, UpperLimitGapMin);\
make_lua_key(L, user);\
make_lua_key(L, userkern);\
make_lua_key(L, userpenalty);\
make_lua_key(L, userskip);\
make_lua_key(L, v);\
make_lua_key(L, vadjust);\
make_lua_key(L, valign);\
make_lua_key(L, value);\
make_lua_key(L, variable);\
make_lua_key(L, variant);\
make_lua_key(L, vbox);\
make_lua_key(L, vcenter);\
make_lua_key(L, vdelimiter);\
make_lua_key(L, vertical);\
make_lua_key(L, verticalmathkern);\
make_lua_key(L, vextensible);\
make_lua_key(L, vextensiblevariant);\
make_lua_key(L, vfactor);\
make_lua_key(L, virtual);\
make_lua_key(L, vlist);\
make_lua_key(L, vmode);\
make_lua_key(L, vmodepar);\
make_lua_key(L, vmove);\
make_lua_key(L, void);\
make_lua_key(L, vrule);\
make_lua_key(L, vsize);\
make_lua_key(L, vskip);\
make_lua_key(L, vtop);\
make_lua_key(L, whatsit);\
make_lua_key(L, widowpenalties);\
make_lua_key(L, widowpenalty);\
make_lua_key(L, width);\
make_lua_key(L, weight);\
make_lua_key(L, woffset);\
make_lua_key(L, word);\
make_lua_key(L, wordpenalty);\
make_lua_key(L, wrapup);\
make_lua_key(L, xheight);\
make_lua_key(L, xleaders);\
make_lua_key(L, xoffset);\
make_lua_key(L, xray);\
make_lua_key(L, xscale);\
make_lua_key(L, xspaceskip);\
make_lua_key(L, yoffset);\
make_lua_key(L, yscale);\
make_lua_key(L, zerospaceskip);\
make_lua_key_alias(L, empty_string,             "");\
/* */ \
make_lua_key_alias(L, node_instance,            NODE_METATABLE_INSTANCE);\
make_lua_key_alias(L, node_properties,          NODE_PROPERTIES_DIRECT);\
make_lua_key_alias(L, node_properties_indirect, NODE_PROPERTIES_INDIRECT);\
/* */ \
make_lua_key_alias(L, token_instance,           TOKEN_METATABLE_INSTANCE);\
make_lua_key_alias(L, token_package,            TOKEN_METATABLE_PACKAGE);\
/* */ \
make_lua_key_alias(L, sparse_instance,          SPARSE_METATABLE_INSTANCE);\
/* */ \
make_lua_key_alias(L, pdfe_instance,            PDFE_METATABLE_INSTANCE);\
make_lua_key_alias(L, pdfe_dictionary_instance, PDFE_METATABLE_DICTIONARY);\
make_lua_key_alias(L, pdfe_array_instance,      PDFE_METATABLE_ARRAY);\
make_lua_key_alias(L, pdfe_stream_instance,     PDFE_METATABLE_STREAM);\
make_lua_key_alias(L, pdfe_reference_instance,  PDFE_METATABLE_REFERENCE);\
/* */ \
make_lua_key_alias(L, vector_instance,          VECTOR_METATABLE_INSTANCE);\
make_lua_key_alias(L, mesh_instance,            MESH_METATABLE_INSTANCE);\
make_lua_key_alias(L, decimal_instance,         DECIMAL_METATABLE_INSTANCE);\
make_lua_key_alias(L, complex_instance,         COMPLEX_METATABLE_INSTANCE);\
make_lua_key_alias(L, posit_instance,           POSIT_METATABLE_INSTANCE);\
/* */ \
make_lua_key_alias(L, file_handle_instance,     LUA_FILEHANDLE);\
/* */ \
make_lua_key_alias(L, string_buffer_instance,   STRING_BUFFER_INSTANCE);\
/* done */

# define declare_metapost_lua_keys(L) \
/* */\
/*          (L, close); */\
make_lua_key(L, bytemap);\
make_lua_key(L, color);\
make_lua_key(L, curl);\
make_lua_key(L, curled);\
make_lua_key(L, curved);\
make_lua_key(L, curvature);\
make_lua_key(L, cycle);\
make_lua_key(L, dash);\
make_lua_key(L, dashes);\
/*          (L, depth); */\
make_lua_key(L, direction_x);\
make_lua_key(L, direction_y);\
make_lua_key(L, elliptical);\
make_lua_key(L, end_cycle);\
make_lua_key(L, endpoint);\
make_lua_key(L, error);\
make_lua_key(L, error_line);\
/*          (L, explicit); */\
make_lua_key(L, fig);\
/*          (L, fill); */\
make_lua_key(L, find_file);\
/*          (L, font); */\
make_lua_key(L, given);\
make_lua_key(L, halt_on_error);\
make_lua_key(L, hash);\
/*          (L, height); */\
make_lua_key(L, htap);\
/*          (L, interaction);*/\
make_lua_key(L, internals);\
make_lua_key(L, job_name);\
make_lua_key(L, knots);\
make_lua_key(L, left_curl);\
make_lua_key(L, left_tension);\
make_lua_key(L, left_type);\
make_lua_key(L, left_x);\
make_lua_key(L, left_y);\
make_lua_key(L, linecap);\
make_lua_key(L, linejoin);\
make_lua_key(L, make_text);\
make_lua_key(L, math_mode);\
make_lua_key(L, memory);\
make_lua_key(L, miterlimit);\
make_lua_key(L, nodes);\
make_lua_key(L, offset);\
/*          (L, open); */\
make_lua_key(L, open_file);\
/*          (L, outline); */\
make_lua_key(L, pairs);\
make_lua_key(L, path);\
make_lua_key(L, pen);\
make_lua_key(L, postscript);\
make_lua_key(L, prescript);\
make_lua_key(L, primary);\
make_lua_key(L, print_line);\
make_lua_key(L, random_seed);\
make_lua_key(L, rectangle);\
/*          (L, reader); */\
make_lua_key(L, restore);\
make_lua_key(L, right_curl);\
make_lua_key(L, right_tension);\
make_lua_key(L, right_type);\
make_lua_key(L, right_x);\
make_lua_key(L, right_y);\
make_lua_key(L, run_error);\
make_lua_key(L, run_internal);\
make_lua_key(L, run_logger);\
make_lua_key(L, run_overload);\
make_lua_key(L, run_script);\
make_lua_key(L, run_status);\
make_lua_key(L, run_warning);\
make_lua_key(L, rx);\
make_lua_key(L, ry);\
make_lua_key(L, save);\
make_lua_key(L, secondary);\
make_lua_key(L, show_mode);\
make_lua_key(L, stacking);\
make_lua_key(L, start_bounds);\
make_lua_key(L, start_clip);\
make_lua_key(L, start_group);\
make_lua_key(L, status);\
make_lua_key(L, stop_bounds);\
make_lua_key(L, stop_clip);\
make_lua_key(L, stop_group);\
make_lua_key(L, strings);\
make_lua_key(L, sx);\
make_lua_key(L, sy);\
make_lua_key(L, symbols);\
make_lua_key(L, tense);\
make_lua_key(L, tertiary);\
/*          (L, text); */\
make_lua_key(L, text_mode);\
make_lua_key(L, tokens);\
make_lua_key(L, transform);\
make_lua_key(L, tx);\
make_lua_key(L, ty);\
/*          (L, type); */\
make_lua_key(L, utf8_mode);\
make_lua_key(L, warning);\
/*          (L, width); */\
make_lua_key(L, writer);\
make_lua_key(L, x_coord);\
make_lua_key(L, y_coord);\
/* */\
make_lua_key(L, vector);\
make_lua_key(L, mesh);\
/* */\
make_lua_key_alias(L, mplib_instance, MP_METATABLE_INSTANCE);\
make_lua_key_alias(L, mplib_figure,   MP_METATABLE_FIGURE);\
make_lua_key_alias(L, mplib_object,   MP_METATABLE_OBJECT);\
/* done */

/*tex
    We want them properly aligned so we put pointers and indices in blocks.
*/

typedef struct lmt_keys {
# undef make_lua_key
# undef make_lua_key_alias
# define make_lua_key       make_lua_key_ptr
# define make_lua_key_alias make_lua_key_ptr_alias
declare_shared_lua_keys(NULL)
declare_metapost_lua_keys(NULL)
# undef make_lua_key
# undef make_lua_key_alias
# define make_lua_key       make_lua_key_idx
# define make_lua_key_alias make_lua_key_idx_alias
declare_shared_lua_keys(NULL)
declare_metapost_lua_keys(NULL)
# undef make_lua_key
# undef make_lua_key_alias
} lmt_keys_info ;

extern lmt_keys_info lmt_keys;

# define make_lua_key       init_lua_key
# define make_lua_key_alias init_lua_key_alias

# define lmt_initialize_shared_keys    declare_shared_lua_keys
# define lmt_initialize_metapost_keys  declare_metapost_lua_keys

/*tex

    We use round from |math.h| because when in a macro we check for sign we (depending on
    optimization) can fetch numbers multiple times. I need to measure this a bit more as inlining
    looks a bit faster on for instance |experiment.tex| but of course the bin becomes (some 10K)
    larger.

*/

//define lmt_rounded(d)            (lua_Integer)  (round(d))
//define lmt_roundedfloat(f)       (lua_Integer)  (round((double) f))

# define lmt_rounded(d)            (lua_Integer)  (llround(d))
# define lmt_roundedfloat(f)       (lua_Integer)  (llround((double) f)) 

# define lmt_tolong(L,i)           (long)         lua_tointeger(L,i)
# define lmt_checklong(L,i)        (long)         luaL_checkinteger(L,i)
# define lmt_optlong(L,i,j)        (long)         luaL_optinteger(L,i,j)

# define lmt_todouble(L,i,d)       (double)       lua_tonumber(L,i)
# define lmt_optdouble(L,i,d)      (double)       luaL_optnumber(L,i,d)

# define lmt_tointeger(L,i)        (int)          lua_tointeger(L,i)
# define lmt_checkinteger(L,i)     (int)          luaL_checkinteger(L,i)
# define lmt_optinteger(L,i,j)     (int)          luaL_optinteger(L,i,j)

# define lmt_tounsigned(L,i)       (unsigned int) lua_tointeger(L,i)
# define lmt_checkinteger(L,i)     (int)          luaL_checkinteger(L,i)
# define lmt_optunsigned(L,i,j)    (unsigned int) luaL_optinteger(L,i,j)

# define lmt_tosizet(L,i)          (size_t)       lua_tointeger(L,i)
# define lmt_checksizet(L,i)       (size_t)       luaL_checkinteger(L,i)
# define lmt_optsizet(L,i,j)       (size_t)       luaL_optinteger(L,i,j)
                                                  
# define lmt_tohalfword(L,i)       (halfword)     lua_tointeger(L,i)
# define lmt_checkhalfword(L,i)    (halfword)     luaL_checkinteger(L,i)
# define lmt_opthalfword(L,i,j)    (halfword)     luaL_optinteger(L,i,j)
                                                  
# define lmt_tofullword(L,i)       (fullword)     lua_tointeger(L,i)
# define lmt_checkfullword(L,i)    (fullword)     luaL_checkinteger(L,i)
# define lmt_optfullword(L,i,j)    (fullword)     luaL_optinteger(L,i,j)
                                                  
# define lmt_toscaled(L,i)         (scaled)       lua_tointeger(L,i)
# define lmt_checkscaled(L,i)      (scaled)       luaL_checkinteger(L,i)
# define lmt_optscaled(L,i,j)      (scaled)       luaL_optinteger(L,i,j)
                                                  
# define lmt_toquarterword(L,i)    (quarterword)  lua_tointeger(L,i)
# define lmt_checkquarterword(L,i) (quarterword)  luaL_checkinteger(L,i)
# define lmt_optquarterword(L,i,j) (quarterword)  luaL_optinteger(L,i,j)
                                                  
# define lmt_tosingleword(L,i)     (singleword)   lua_tointeger(L,i)
# define lmt_checksingleword(L,i)  (singleword)   luaL_checkinteger(L,i)
# define lmt_optsingleword(L,i,j)  (singleword)   luaL_optinteger(L,i,j)

# undef lround
# include <math.h>

static inline int lmt_roundnumber(lua_State *L, int i)
{
    double n = lua_tonumber(L, i);
    return n == 0.0 ? 0 : lround(n);
}

static inline unsigned int lmt_uroundnumber(lua_State *L, int i)
{
    double n = lua_tonumber(L, i);
    return n == 0.0 ? 0 : (unsigned int) lround(n);
}

static inline int lmt_optroundnumber(lua_State *L, int i, int dflt)
{
    double n = luaL_optnumber(L, i, dflt);
    return n == 0.0 ? 0 : lround(n);
}

static inline int lmt_opturoundnumber(lua_State *L, int i, int dflt)
{
    double n = luaL_optnumber(L, i, dflt);
    return n == 0.0 ? 0 : (unsigned int) lround(n);
}

static inline double lmt_number_from_table(lua_State *L, int i, int j, lua_Number d)
{
    double n;
    lua_rawgeti(L, i, j);
    n = luaL_optnumber(L, -1, d);
    lua_pop(L, 1);
    return n;
}

extern void lmt_initialize_interface(void);

# define lmt_toroundnumber  lmt_roundnumber
# define lmt_touroundnumber lmt_uroundnumber

static inline void lua_set_string_by_key(lua_State *L, const char *a, const char *b)
{
    lua_pushstring(L, b ? b : "");
    lua_setfield(L, -2, a);
}

static inline void lua_set_string_by_index(lua_State *L, lua_Integer a, const char *b)
{
    lua_pushstring(L, b ? b : "");
    lua_rawseti(L, -2, a);
}

static inline void lua_set_integer_by_key(lua_State *L, const char *a, int b)
{
    lua_pushinteger(L, b);
    lua_setfield(L, -2, a);
}

static inline void lua_set_integer_by_index(lua_State *L, int a, int b)
{
    lua_pushinteger(L, b);
    lua_rawseti(L, -2, a);
}

static inline void lua_set_number_by_key(lua_State *L, const char *a, double b)
{
    lua_pushnumber(L, b);
    lua_setfield(L, -2, a);
}

static inline void lua_set_number_by_index(lua_State *L, int a, double b)
{
    lua_pushnumber(L, b);
    lua_rawseti(L, -2, a);
}

static inline void lua_set_cardinal_by_key(lua_State *L, const char *a, unsigned b)
{
    lua_pushinteger(L, b);
    lua_setfield(L, -2, a);
}

static inline void lua_set_cardinal_by_index(lua_State *L, int a, unsigned b)
{
    lua_pushinteger(L, b);
    lua_rawseti(L, -2, a);
}

static inline void lua_set_boolean_by_key(lua_State *L, const char *a, int b)
{
    lua_pushboolean(L, b);
    lua_setfield(L, -2, a);
}

static inline void lua_set_boolean_by_index(lua_State *L, int a, int b)
{
    lua_pushboolean(L, b);
    lua_rawseti(L, -2, a);
}

static inline void lmt_string_to_buffer(const char *str)
{
    luaL_addstring(lmt_lua_state.used_buffer, str);
}

static inline void lmt_char_to_buffer(char c)
{
    luaL_addchar(lmt_lua_state.used_buffer, c);
}

static inline void lmt_newline_to_buffer(void)
{
    luaL_addchar(lmt_lua_state.used_buffer, '\n');
}

/* moved here */

# define lua_pop_one(L) lua_pop(L, 1)

//define lua_pop_one(L) L->top.p--

# define set_numeric_field_by_index(target,name,dflt) \
    lua_push_key(name); \
    target = (lua_rawget(L, -2) == LUA_TNUMBER) ? lmt_roundnumber(L, -1) : dflt ; \
    lua_pop_one(L);

# define set_boolean_field_by_index(target,name,dflt) \
    lua_push_key(name); \
    target = (lua_rawget(L, -2) == LUA_TBOOLEAN) ? lua_toboolean(L, -1) : dflt ; \
    lua_pop_one(L);

# define set_string_field_by_index(target,name) \
    lua_push_key(name); \
    target = (lua_rawget(L, -2) == LUA_TSTRING) ? lua_tostring(L, -1) : NULL ; \
    lua_pop_one(L);

# define set_any_field_by_index(target,name) \
    lua_push_key(name); \
    target = (lua_rawget(L, -2) != LUA_TNIL); \
    lua_pop_one(L);

# endif
