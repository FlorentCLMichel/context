/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex

    In traditional \TEX\ inserts are implemented using a quadruple of box, dimen, count and skip
    registers. This means that the allocate macro |\newinsert| as well as the other allocators
    have to keep a range of registers free. In \CONTEXT\ (\MKII\ and \MKIV) for instance the
    indices 132 upto 254 are reserved for that.

    When pondering about improvements this implementation detail always puts some strains on
    the possible solutions and it is for that reason that an alternative code path is present,
    one that keep the relevant data in dedicated data structures. When that got implemented all
    accessors ended up here. Most were already abstracted anyway. For now it means that the old
    interface still works (and is default). By setting the |\insertmode| to 2 the alternative
    path is chosen. For practical reasons the first time an insert is used that value gets
    frozen; a mixed approach was too messy.

    Actually the new variant, which is tagged |class| instead of |index|, also better suits the
    extended box model. There is access to the basic three dimension but that's all. One can wrap
    in a box and mess with others but doing that with the boxes inserts makes no sense because
    the output routine expects simple boxes.

    A side effect is of course that we now have more primitives, starting with |\insert...| and
    also helpers at the \LUA\ end. A few more will follow and likely some enhancements will show
    up too.

    In this new mode we also store the |\floatingpenalty| and |\maxdepth| so these can now differ 
    per class. They were already stored in the node, but this way we don't need to set the shared
    variable every time we do an insert.

*/

insert_state_info lmt_insert_state = {
    .inserts     = NULL,
    .insert_data = {
        .minimum   = min_insert_size,
        .maximum   = max_insert_size,
        .size      = memory_data_unset,
        .step      = stp_insert_size,
        .allocated = 0,
        .itemsize  = sizeof(insert_record),
        .top       = 0,
        .ptr       = 0,
        .initial   = memory_data_unset,
        .offset    = 0,
        .extra     = 0, 
    },
    .mode        = unset_insert_mode,
    .storing     = 0,
};

typedef enum saved_insert_entries {
    saved_insert_index_entry    = 0, 
    saved_insert_data_entry     = 0, 
    saved_insert_callback_entry = 0, 
    saved_insert_n_of_records   = 1,
} saved_insert_entries;

# define saved_insert_index    saved_value_1(saved_insert_index_entry)
# define saved_insert_data     saved_value_2(saved_insert_data_entry)
# define saved_insert_callback saved_value_3(saved_insert_callback_entry)

static inline void saved_inserts_initialize(void)
{
    saved_type(0) = saved_record_0;
    saved_record(0) = insert_save_type;
}

void tex_show_insert_group(void)
{
    tex_print_str_esc("insert");
    tex_print_int(saved_insert_index);
}

int tex_show_insert_record(void)
{
    tex_print_str("insert ");
    switch (saved_type(0)) { 
       case saved_record_0:
            tex_print_format("index %i", saved_insert_index);       // saved_value_1(0)
            break;
       case saved_record_1:
            tex_print_format("data %i", saved_insert_data);         // saved_value_2(0)  
            break;
       case saved_record_2:
            tex_print_format("callback %i", saved_insert_callback); // saved_value_3(0)
            break;
        default: 
            return 0;
    }
    return 1;
}

void tex_initialize_inserts(void)
{
    insert_record *tmp = aux_allocate_clear_array(sizeof(insert_record), lmt_insert_state.insert_data.minimum, 1);
    if (tmp) {
        lmt_insert_state.inserts = tmp;
        lmt_insert_state.insert_data.allocated = lmt_insert_state.insert_data.minimum;
        lmt_insert_state.insert_data.top = lmt_insert_state.insert_data.minimum;
        lmt_insert_state.insert_data.ptr = 0;
    } else {
        tex_overflow_error("inserts", lmt_insert_state.insert_data.minimum);
    }
}

/*tex
    This one is not sparse but we don't have many inserts so we're okay. I need to check the 0/1
    offsets here.
*/

int tex_valid_insert_id(halfword n)
{
    switch (lmt_insert_state.mode) {
        case index_insert_mode:
            return (n >= 0 && n <= max_box_register_index);
        case class_insert_mode:
            if (n <= 0) {
                tex_handle_error(
                    normal_error_type,
                    "In \\insertmode 2 you can't use zero as index.",
                    NULL
                );
            } else if (n <= lmt_insert_state.insert_data.ptr) {
                return 1;
            } else if (n < lmt_insert_state.insert_data.top) {
                lmt_insert_state.insert_data.ptr = n;
                return 1;
            } else if (n < lmt_insert_state.insert_data.maximum && lmt_insert_state.insert_data.top < lmt_insert_state.insert_data.maximum) {
                insert_record *tmp;
                int top = n + lmt_insert_state.insert_data.step;
                if (top > lmt_insert_state.insert_data.maximum) {
                    top = lmt_insert_state.insert_data.maximum;
                }
                tmp = aux_reallocate_array(lmt_insert_state.inserts, sizeof(insert_record), top, 1); // 1 slack
                if (tmp) {
                    size_t extra = ((size_t) top - lmt_insert_state.insert_data.top) * sizeof(insert_record);
                    memset(&tmp[lmt_insert_state.insert_data.top + 1], 0, extra);
                 // memset(&tmp[lmt_insert_state.insert_data.top], 0, extra);
                    lmt_insert_state.inserts = tmp;
                    lmt_insert_state.insert_data.allocated = top;
                    lmt_insert_state.insert_data.top = top;
                    lmt_insert_state.insert_data.ptr = n;
                    return 1;
                }
            }
            tex_overflow_error("inserts", lmt_insert_state.insert_data.maximum);
    }
    return 0;
}

scaled tex_get_insert_limit(halfword i)
{
    if (tex_valid_insert_id(i)) {
        return lmt_insert_state.mode == index_insert_mode ? insert_maxheight(i) : lmt_insert_state.inserts[i].limit;
    } else {
        return 0;
    }
}

halfword tex_get_insert_multiplier(halfword i)
{
    if (tex_valid_insert_id(i)) {
        return lmt_insert_state.mode == index_insert_mode ? insert_multiplier(i) : lmt_insert_state.inserts[i].multiplier;
    } else {
        return 0;
    }
}

halfword tex_get_insert_penalty(halfword i)
{
    if (tex_valid_insert_id(i)) {
        return lmt_insert_state.mode == index_insert_mode ? floating_penalty_par : lmt_insert_state.inserts[i].penalty;
    } else {
        return 0;
    }
}

halfword tex_get_insert_maxdepth(halfword i)
{
    if (tex_valid_insert_id(i)) {
        return lmt_insert_state.mode == index_insert_mode ? split_max_depth_par : lmt_insert_state.inserts[i].maxdepth;
    } else {
        return 0;
    }
}

halfword tex_get_insert_distance(halfword i)
{
    if (tex_valid_insert_id(i)) {
        return lmt_insert_state.mode == index_insert_mode ? insert_distance(i) : lmt_insert_state.inserts[i].distance;
    } else {
        return 0;
    }
}

static inline halfword tex_aux_insert_box(halfword i)
{
    if (tex_valid_insert_id(i)) {
        return lmt_insert_state.mode == index_insert_mode ? insert_content(i) : lmt_insert_state.inserts[i].content;
    } else {
        return null;
    }
}

scaled tex_get_insert_height(halfword i)
{
    halfword b = tex_aux_insert_box(i);
    return b ? box_height(b) : 0;
}

scaled tex_get_insert_depth(halfword i)
{
    halfword b = tex_aux_insert_box(i);
    return b ? box_depth(b) : 0;
}

scaled tex_get_insert_width(halfword i)
{
    halfword b = tex_aux_insert_box(i);
    return b ? box_width(b) : 0;
}

halfword tex_get_insert_direction(halfword i)
{
    halfword b = tex_aux_insert_box(i);
    return tex_get_direction_from_list(b ? box_list(b) : null);
}

halfword tex_get_insert_content(halfword i)
{
    return tex_aux_insert_box(i);
}

scaled tex_get_insert_line_height(halfword i) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        return lmt_insert_state.inserts[i].lineheight;
    } else { 
        return 0;
    }
}

scaled tex_get_insert_line_depth(halfword i) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        return lmt_insert_state.inserts[i].linedepth;
    } else { 
        return 0;
    }
}
scaled tex_get_insert_stretch(halfword i) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        return lmt_insert_state.inserts[i].stretch;
    } else { 
        return 0;
    }
}

scaled tex_get_insert_shrink(halfword i) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        return lmt_insert_state.inserts[i].shrink;
    } else { 
        return 0;
    }
}

halfword tex_get_insert_storage(halfword i)
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        return has_insert_option(i, insert_option_storing);
    } else {
        return 0;
    }
}

void tex_set_insert_limit(halfword i, scaled v)
{
    if (tex_valid_insert_id(i)) {
        switch (lmt_insert_state.mode) {
            case index_insert_mode: insert_maxheight(i) = v; break;
            case class_insert_mode: lmt_insert_state.inserts[i].limit = v; break;
        }
    }
}

void tex_set_insert_multiplier(halfword i, halfword v) 
{
    if (tex_valid_insert_id(i)) {
        switch (lmt_insert_state.mode) {
            case index_insert_mode: insert_multiplier(i) = v; break;
            case class_insert_mode: lmt_insert_state.inserts[i].multiplier = v; break;
        }
    }
}

void tex_set_insert_penalty(halfword i, halfword v) 
{
    if (tex_valid_insert_id(i) && lmt_insert_state.mode == class_insert_mode) {
        lmt_insert_state.inserts[i].options = set_insert_option(lmt_insert_state.inserts[i].options, insert_option_penalty);
        lmt_insert_state.inserts[i].penalty = v;
    }
}

void tex_set_insert_maxdepth(halfword i, halfword v) 
{
    if (tex_valid_insert_id(i) && lmt_insert_state.mode == class_insert_mode) {
        lmt_insert_state.inserts[i].options = set_insert_option(lmt_insert_state.inserts[i].options, insert_option_maxdepth);
        lmt_insert_state.inserts[i].maxdepth = v;
    }
}

void tex_set_insert_distance(halfword i, halfword v) 
{
    if (tex_valid_insert_id(i)) {
        int d = null;
        switch (lmt_insert_state.mode) {
            case index_insert_mode:
                d = insert_distance(i);
                insert_distance(i) = v;
                break;
            case class_insert_mode:
                d = lmt_insert_state.inserts[i].distance;
                lmt_insert_state.inserts[i].distance = v;
                break;
        }
        tex_flush_node(d);
    }
}

void tex_set_insert_height(halfword i, scaled v) 
{
    halfword b = tex_aux_insert_box(i);
    if (b) {
        box_height(b) = v;
    }
}

void tex_set_insert_depth(halfword i, scaled v) 
{
    halfword b = tex_aux_insert_box(i);
    if (b) {
        box_depth(b) = v;
    }
}

void tex_set_insert_width(halfword i, scaled v) 
{
    halfword b = tex_aux_insert_box(i);
    if (b) {
        box_width(b) = v;
    }
}

void tex_set_insert_content(halfword i, halfword v) 
{
    /* can have old pointer to list */
    switch (lmt_insert_state.mode) {
        case index_insert_mode: insert_content(i) = v; break;
        case class_insert_mode: if (tex_valid_insert_id(i)) { lmt_insert_state.inserts[i].content = v; } break;
    }
}

void tex_set_insert_line_height(halfword i, scaled v) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        lmt_insert_state.inserts[i].lineheight = v;
    }
}

void tex_set_insert_line_depth(halfword i, scaled v) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        lmt_insert_state.inserts[i].linedepth = v;
    }
}

void tex_set_insert_stretch(halfword i, scaled v) 
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        lmt_insert_state.inserts[i].stretch = v;
    }
}

void tex_set_insert_shrink(halfword i, scaled v) {
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        lmt_insert_state.inserts[i].shrink = v;
    }
}

void tex_set_insert_direction(halfword i, scaled v) 
{
    (void) i;
    (void) v;
    /*tex 
        This is a dummy, at least till I find a reason and logic for it. 
    */
}

void tex_set_insert_storage(halfword i, halfword v)
{
    if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        lmt_insert_state.inserts[i].options = v
      ? set_insert_option(lmt_insert_state.inserts[i].options, insert_option_storing)
      : unset_insert_option(lmt_insert_state.inserts[i].options, insert_option_storing);
    }
}

void tex_wipe_insert(halfword i) {
    if (lmt_insert_state.mode == class_insert_mode && i >= 0 && i <= lmt_insert_state.insert_data.ptr) {
//  if (lmt_insert_state.mode == class_insert_mode && tex_valid_insert_id(i)) {
        halfword b = lmt_insert_state.inserts[i].content;
        if (b) {
            tex_flush_node(b);
            lmt_insert_state.inserts[i].content = null;
        }
    }
}

halfword lmt_get_insert_distance(halfword i, int slot)
{
    int callback_id = lmt_callback_defined(insert_distance_callback);
    if (callback_id != 0) {
        halfword replacement = null;
        lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dd->N", i, slot, &replacement);
        if (replacement) {
            return replacement;
        } else {
            halfword distance = null;
            switch (lmt_insert_state.mode) {
                case index_insert_mode:
                    distance = insert_distance(i);
                    break;
                case class_insert_mode:
                    if (tex_valid_insert_id(i)) {
                        distance = lmt_insert_state.inserts[i].distance;
                    }
                    break;
            }
            if (distance) {
                return tex_copy_node(distance);
            }
        }
    }
    return tex_new_glue_spec_node(null);
}

halfword tex_get_insert_progress(halfword i)
{
    if (tex_valid_insert_id(i)) {
        halfword p = page_insert_head;
        while (p && i >= insert_index(node_next(p))) {
            p = node_next(p);
            if (p == page_insert_head) {
                break;
            }
        }
        return insert_index(p) == i ? insert_total_height(p) : 0;
    } else {
        return 0;
    }
}

/*tex The |class_insert| zero serves as a garbage bin. */

halfword tex_scan_insert_index(void)
{
    halfword index = 0;
    switch (lmt_insert_state.mode) {
        case unset_insert_mode:
            lmt_insert_state.mode = index_insert_mode;
            // fall-through
        case index_insert_mode:
            index = tex_scan_box_register_number();
            if (index == output_box_par) {
                tex_handle_error(
                    normal_error_type,
                    "You can't \\insert%i",
                    output_box_par,
                    "I'm changing to \\insert0; box \\outputbox is special."
                );
                index = 0;
            }
            break;
        case class_insert_mode:
            index = tex_scan_integer(0, NULL, NULL);
            if (! tex_valid_insert_id(index)) {
                index = 0;
            }
            break;
    }
    return index;
}

void tex_set_insert_mode(halfword mode)
{
    if (lmt_insert_state.mode == unset_insert_mode && (mode == index_insert_mode || mode == class_insert_mode)) {
        lmt_insert_state.mode = mode;
    } else if (mode != lmt_insert_state.mode) {
        tex_handle_error(
            normal_error_type,
            "Bad \\insertmode (%i)",
            mode,
            "This mode can be set once and has value 1 or 2. It will be automatically\n"
            "set when \\insert is used."
        );
    }
}

int tex_insert_is_void(halfword i)
{
    halfword b = tex_aux_insert_box(i);
    return (! b) || box_list(b) == null; /*tex So also an empty box test! */
}

/* playground */

int tex_insert_stored(void)
{
    return lmt_insert_state.head != null;
}

void tex_insert_restore(halfword n)
{
    if (lmt_insert_state.tail) {
        tex_couple_nodes(lmt_insert_state.tail, n);
    } else {
        lmt_insert_state.head = n;
    }
    lmt_insert_state.tail = n;
}

void tex_insert_store(halfword i, halfword n)
{
    if (tex_get_insert_storage(i)) {
        tex_insert_restore(n);
    }
}

/* not sparse (yet) ... makes no sense (unless we make the list pointers) */

void tex_dump_insert_data(dumpstream f) {
    dump_int(f, lmt_insert_state.mode);
    dump_int(f, lmt_insert_state.insert_data.ptr);
    dump_int(f, lmt_insert_state.insert_data.top);
    dump_things(f, lmt_insert_state.inserts[0], lmt_insert_state.insert_data.ptr);
}

void tex_undump_insert_data(dumpstream f) {
    insert_record *tmp;
    undump_int(f, lmt_insert_state.mode);
    undump_int(f, lmt_insert_state.insert_data.ptr);
    undump_int(f, lmt_insert_state.insert_data.top);
    tmp = aux_allocate_clear_array(sizeof(insert_record), lmt_insert_state.insert_data.top, 1);
    if (tmp) {
        lmt_insert_state.inserts = tmp;
        lmt_insert_state.insert_data.allocated = lmt_insert_state.insert_data.top;
        undump_things(f, lmt_insert_state.inserts[0], lmt_insert_state.insert_data.ptr);
    } else {
        tex_overflow_error("inserts", lmt_insert_state.insert_data.top);
    }
}

/*tex
    Inserts, not the easiest mechanism and a candicate for more opening up.
*/

void tex_run_insert(void)
{
    int brace = 0;
    halfword callback = 0;
    halfword data = 0;
    halfword index = -1;
    while (1) {
        switch (tex_scan_character("cdiCDI", 1, 1, 1)) {
            case 0:
                goto DONE;
            case 'c': case 'C':
                if (tex_scan_mandate_keyword("callback", 1)) {
                    callback = tex_scan_integer(0, NULL, NULL);
                }
                break;
            case 'd': case 'D':
                /* identifier */
                if (tex_scan_mandate_keyword("data", 1)) {
                    data = tex_scan_integer(0, NULL, NULL);
                }
                break;
            case 'i': case 'I':
                if (tex_scan_mandate_keyword("index", 1)) {
                    index = tex_scan_insert_index();
                }
                break;
            case '{':
                brace = 1;
                goto DONE;
            default:
                goto DONE;
        }
    }
  DONE:
    if (index < 0) {
        index = tex_scan_insert_index();
    }
    saved_inserts_initialize();
    saved_insert_index = index;
    saved_insert_data = data;
    saved_insert_callback = callback;
    lmt_save_state.save_stack_data.ptr += saved_insert_n_of_records;
    tex_new_save_level(insert_group);
    if (! brace) {
        tex_scan_left_brace();
    }
    tex_normal_paragraph(insert_par_context);
    tex_push_nest();
    cur_list.mode = internal_vmode;
    cur_list.prev_depth = ignore_depth_criterion_par;
}

void tex_finish_insert_group(void)
{
    if (! tex_wrapped_up_paragraph(insert_par_context, 0)) {
        halfword p, q; /*tex for short-term use */
        scaled d;      /*tex holds |split_max_depth| in |insert_group| */
        halfword f;    /*tex holds |floating_penalty| in |insert_group| */
        tex_end_paragraph(insert_group, insert_par_context);
        q = tex_new_glue_node(split_top_skip_par, top_skip_code);
        d = split_max_depth_par;
        f = floating_penalty_par;
        tex_unsave();
        lmt_save_state.save_stack_data.ptr -= saved_insert_n_of_records;
     // p = tex_vpack(node_next(cur_list.head), 0, packing_additional, max_dimension, direction_unknown);
     // /* we don't do this: */
     // /* p = tex_filtered_vpack(node_next(cur_list.head), 0, packing_additional, max_dimension, insert_group, direction_unknown, 0, 0); */
     // /* because it can induce loops. */
     // tex_pop_nest();
        p = node_next(cur_list.head);
        tex_pop_nest();
        /*tex
            An |\insert| is just a list. We package it because we want to know the height but then 
            discard the wrapper |vlist| node. So the |insert_list| is not packaged.
        */
        p = tex_vpack(p, 0, packing_additional, max_dimension, direction_unknown, holding_none_option, NULL);
        {
            halfword index = saved_insert_index;
            halfword data = saved_insert_data;
            halfword callback = saved_insert_callback;
            halfword insert = tex_new_node(insert_node, 0);
            halfword maxdepth = tex_get_insert_maxdepth(index);
            halfword floating = tex_get_insert_penalty(index);
            if (tex_get_insert_storage(index)) {
                tex_insert_store(index, insert);
            } else {
                tex_tail_append(insert);
            }
            insert_index(insert) = index;
            insert_identifier(insert) = data;
            insert_callback(insert) = callback;
            insert_total_height(insert) = box_total(p);
            insert_list(insert) = box_list(p);
            insert_split_top(insert) = q;
            insert_max_depth(insert) = has_insert_option(index, insert_option_maxdepth) ? d : maxdepth;
            insert_float_cost(insert) = has_insert_option(index, insert_option_penalty) ? f : floating;
            insert_line_height(insert) = tex_get_insert_line_height(index);
            insert_line_depth(insert) = tex_get_insert_line_depth(index);
            insert_stretch(insert) = tex_get_insert_stretch(index);
            insert_shrink(insert) = tex_get_insert_shrink(index);
            box_list(p) = null;
            tex_flush_node(p);
            if (tracing_inserts_par > 0) {
                tex_begin_diagnostic();
                tex_print_levels();
                tex_print_format("[insert: setting, index %i, height %p, depth %p, penalty %i, topskip %Q]",
                    index, insert_total_height(insert), insert_max_depth(insert), insert_float_cost(insert), q, pt_unit);
                if (tracing_inserts_par > 1) {
                    tex_print_node_list(insert_list(insert), "insert", show_box_depth_par, show_box_breadth_par);
                }
                tex_end_diagnostic();
            }
        }
        /* we never do the callback ... maybe move it outside */
        if (lmt_nest_state.nest_data.ptr == 0) {
            tex_build_page(insert_page_context, 0);
        }
    }
}

/*tex 
    This one is meant for the elements (slots) in balanced lists but as we can have them packaged
    we try to handle this. 
*/

int tex_identify_inserts(halfword b, halfword cbk)
{
    halfword value = 0;
    while (b) { 
        switch (node_type(b)) {
            case hlist_node:
            case vlist_node:
                switch (node_subtype(b)) {
                    case hbox_list:
                    case container_list:
                    case unknown_list:
                        break;
                    case balance_slot_list:
                        goto DONE;
                    default: 
                        return value;
                }
                break;
            default: 
                return value; 
        }
        b = box_list(b);
    }
  DONE:
    if (b && node_type(b) == vlist_node) {
        halfword current = box_list(b);
        while (current) { 
            if (node_type(current) == insert_node) {
                int callback = lmt_callback_defined(balance_insert_callback);
                if (callback) {
                    ++lmt_balance_state.n_of_callbacks;
                    lmt_run_callback(lmt_lua_state.lua_instance, callback, "Nddd->",
                        current, 
                        cbk,
                        insert_index(current),
                        insert_identifier(current)
                    );
                }
                value |= has_inserts;
                if (insert_list(current)) {
                    value |= has_inserts_with_content;
                }
                if (insert_total_height(current) > 0 && tex_get_insert_multiplier(insert_index(current)) > 0) {
                    value |= has_inserts_with_height;
                }
            }
            current = node_next(current);
        }
    }
    return value;
}

scaled tex_insert_height(halfword node)
{
    /*tex A redundant check but we do it anyway. */
    if (node && node_type(node) == insert_node) {
        halfword multiplier = tex_get_insert_multiplier(insert_index(node));
        halfword needed = insert_total_height(node);
        if (multiplier > 0 && needed > 0) {
            return tex_x_over_n(needed, scaling_factor) * multiplier;
        }
    }
    return 0;
}

# define set_bit(bits,n) bits[n/8] |= (1 << (index % 8))
# define get_bit(bits,n) (1 & (bits[index/8] >> (n % 8)))

void tex_insert_reset_distances(void)
{
    for (int index = 0; index <= lmt_insert_state.insert_data.top; index++) {
        if (lmt_insert_state.inserts[index].before) { 
            tex_flush_node(lmt_insert_state.inserts[index].before);
        } 
        lmt_insert_state.inserts[index].before = null;
        if (lmt_insert_state.inserts[index].inbetween) { 
            tex_flush_node(lmt_insert_state.inserts[index].inbetween);
        } 
        lmt_insert_state.inserts[index].inbetween = null;
    }
}

scaled tex_insert_distances(halfword first, halfword last, scaled *stretch, scaled *shrink)
{
    char bits[(max_n_of_inserts/8)+1] = { 0 }; /* brrr */
    int isfirst = 1;
    scaled amount = 0;
    halfword c = first; 
    while (c != last) {
        if (node_type(c) == insert_node && insert_total_height(c) > 0 && tex_get_insert_multiplier(insert_index(c)) > 0) { 
            halfword distance = null;
            halfword index = insert_index(c);
            if (isfirst) { 
                if (lmt_insert_state.inserts[index].before) {
                    distance = lmt_insert_state.inserts[index].before;
                } else { 
                    distance = lmt_get_insert_distance(index, 1); /* first */
                    lmt_insert_state.inserts[index].before = distance;
                }
                isfirst = 0;
                set_bit(bits,index);
            } else if (insert_index(c) == index && ! get_bit(bits, index)) {
                if (lmt_insert_state.inserts[index].inbetween) {
                    distance = lmt_insert_state.inserts[index].inbetween;
                } else {
                    distance = lmt_get_insert_distance(index, 2); /* inbetween */
                    lmt_insert_state.inserts[index].inbetween = distance;
                }
                set_bit(bits,index);
            }
            if (distance) { 
                amount += glue_amount(distance);
                if (stretch) {
                    *stretch += glue_stretch(distance); /* ignore order, at least we can't handle that now */
                }
                if (shrink) {
                    *shrink += glue_shrink(distance);   /* no order, no infinite warning either */
                }
            }
        }
        c = node_next(c);
    }
    return amount;
}
