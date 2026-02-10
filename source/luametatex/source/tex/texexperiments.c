// if (lmt_page_builder_state.insert_pruning && lastinsert) {
//     tex_insert_prune_page(3);
//     lmt_page_builder_state.insert_pruning = 0;
// }

// } else if (subtype == insert_boundary) {
//     if (boundary_reserved(current) == 4) {
//         lmt_page_builder_state.insert_pruning = 1;
//     }
// }

int tex_insert_prune_page(halfword option)
{
    int count = 0;
    if (page_head != page_tail && lmt_insert_state.mode == class_insert_mode) {
        halfword current = node_next(page_head);
        if (tracing_inserts_par > 0) {
            tex_begin_diagnostic();
        }
     // tex_show_node_list(current,10000,10000);
        while (current) {
            switch (node_type(current)) {
                case insert_node:
                    count++;
                    if (tracing_inserts_par > 0) {
                        tex_print_format("%l[insert: pruning, index %i, count %i]", insert_index(current), count);
                    }
                    current = node_next(current);
                    break;
                default:
                    if (tracing_inserts_par > 0) {
                        tex_print_format("%l[insert: pruning, quitting]");
                        tex_end_diagnostic();
                    }
                    return 0;
            }
        }
        if (option == 3) {
            current = node_next(page_head);
            while (current) {
                halfword index = insert_index(current);
                halfword next = node_next(current);
                node_prev(current) = null;
                node_next(current) = null;
                if (tex_valid_insert_id(index)) {
                    halfword cache = lmt_insert_state.inserts[index].cache;
                    if (cache) {
                        tex_couple_nodes(tex_tail_of_node_list(cache), current);
                    } else {
                        lmt_insert_state.inserts[index].cache = current;
                    }
                    lmt_insert_state.inserts[index].state++;
                } else {
                    tex_flush_node(current);
                }
                current = next;
            }
            node_next(page_head) = null;
            page_tail = page_head;
            /* */
            for (int index = 0; index <= lmt_insert_state.insert_data.top; index++) {
                if (lmt_insert_state.inserts[index].state) {
                    halfword cache = lmt_insert_state.inserts[index].cache;
                    if (cache) {
                        if (tracing_inserts_par > 0) {
                            tex_print_format("%l[insert: sorting, index %i, collected %i]",
                                index, lmt_insert_state.inserts[index].state
                            );
                        }
                        if (node_next(page_head)) {
                            tex_couple_nodes(page_tail, cache);
                        } else {
                            tex_couple_nodes(page_head, cache);
                        }
                        page_tail = tex_tail_of_node_list(cache);
                        lmt_insert_state.inserts[index].cache = null;
                    }
                    lmt_insert_state.inserts[index].state = 0;
                }
            }
        } else if (option == 1 || option == 2) {
            current = node_next(page_head);
            while (current) {
                halfword index = insert_index(current);
                halfword next = node_next(current);
                node_prev(current) = null;
                node_next(current) = null;
                if (tex_valid_insert_id(index)) {
                    halfword content = lmt_insert_state.inserts[index].content;
                    halfword list = null;
                    if (! content) {
                        content = tex_new_node(vlist_node, 0);
                        tex_attach_current_attribute_list(content);
                        lmt_insert_state.inserts[index].content = content;
                    }
                    list = box_list(content);
                    /* tail = ... */
                    if (option == 2) {
                        halfword main = current;
                        current = insert_list(current);
                        insert_list(main) = null;
                        tex_flush_node(main);
                        if (list) {
                            halfword glue = tex_new_glue_node(zero_glue, user_skip_glue);
                            tex_attach_current_attribute_list(glue);
                            tex_couple_nodes(tex_tail_of_node_list(list), glue);
                        }
                    }
                    if (list) {
                        tex_couple_nodes(tex_tail_of_node_list(list), current);
                    } else {
                        box_list(content) = current;
                    }
                    lmt_insert_state.inserts[index].state++;
                } else {
                    tex_flush_node(current);
                }
                current = next;
            }
            /* */
            node_next(page_head) = null;
            page_tail = page_head;
            lmt_page_builder_state.insert_penalties = 0;
            lmt_page_builder_state.insert_heights = 0;
            /* */
            for (int index = 0; index <= lmt_insert_state.insert_data.top; index++) {
                if (lmt_insert_state.inserts[index].state) {
                    halfword content = lmt_insert_state.inserts[index].content;
                    if (content) {
                        scaledwhd whd = tex_natural_vsizes(box_list(content), null, 0.0, 0, 0, 1);
                        box_width(content) = whd.wd;
                        box_height(content) = whd.ht;
                        box_depth(content) = whd.dp;
                        if (tracing_inserts_par > 0) {
                            tex_print_format("%l[insert: pruning, index %i, collected %i, width %p, height %p, depth %p]",
                                index, lmt_insert_state.inserts[index].state, whd.wd, whd.ht, whd.dp
                            );
                        }
                     // tex_show_node_list(content,10000,10000);
                    }
                    lmt_insert_state.inserts[index].state = 0;
                }
            }
        }
        if (tracing_inserts_par > 0) {
            tex_print_format("%l[insert: pruning, total %i]", count);
            tex_end_diagnostic();
        }
    }
    return count;
}

