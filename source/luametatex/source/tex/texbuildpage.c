/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex

    When \TEX\ appends new material to its main vlist in vertical mode, it uses a method something
    like |vsplit| to decide where a page ends, except that the calculations are done \quote {on
    line} as new items come in. The main complication in this process is that insertions must be
    put into their boxes and removed from the vlist, in a more-or-less optimum manner.

    We shall use the term \quote {current page} for that part of the main vlist that is being
    considered as a candidate for being broken off and sent to the user's output routine. The
    current page starts at |node_next(page_head)|, and it ends at |page_tail|. We have |page_head =
    page_tail| if this list is empty.

    Utter chaos would reign if the user kept changing page specifications while a page is being
    constructed, so the page builder keeps the pertinent specifications frozen as soon as the page
    receives its first box or insertion. The global variable |page_contents| is |empty| when the
    current page contains only mark nodes and content-less whatsit nodes; it is |inserts_only|
    if the page contains only insertion nodes in addition to marks and whatsits. Glue nodes, kern
    nodes, and penalty nodes are discarded until a box or rule node appears, at which time
    |page_contents| changes to |box_there|. As soon as |page_contents| becomes non-|empty|, the
    current |vsize| and |max_depth| are squirreled away into |page_goal| and |page_max_depth|; the
    latter values will be used until the page has been forwarded to the user's output routine. The
    |\topskip| adjustment is made when |page_contents| changes to |box_there|.

    Although |page_goal| starts out equal to |vsize|, it is decreased by the scaled natural
    height-plus-depth of the insertions considered so far, and by the |\skip| corrections for
    those insertions. Therefore it represents the size into which the non-inserted material
    should fit, assuming that all insertions in the current page have been made.

    The global variables |best_page_break| and |least_page_cost| correspond respectively to the
    local variables |best_place| and |least_cost| in the |vert_break| routine that we have already
    studied; i.e., they record the location and value of the best place currently known for
    breaking the current page. The value of |page_goal| at the time of the best break is stored in
    |best_size|.

*/

/* 
    We don't do much overflow checking here, so we can wrap around. THis can actually be used in 
    some cases to collect more than normally possible and split off an amount that is okay. 

*/

page_builder_state_info lmt_page_builder_state = {
    .tail             = null,
    .contents         = 0,
    .max_depth        = 0,
    .best_break       = null,
    .least_cost       = 0,
    .best_size        = 0,
    .goal             = 0,
    .vsize            = 0,
    .total            = 0,
    .depth            = 0,
    .excess           = 0,
    .page_so_far      = { 0 },
    .insert_penalties = 0,
    .insert_heights   = 0,
    .last_glue        = max_halfword,
    .last_penalty     = 0,
    .last_kern        = 0,
    .last_node_type   = unknown_node_type,
    .last_node_subtype= unknown_node_subtype,
    .last_extra_used  = 0,
    .last_boundary    = 0,
    .output_active    = 0,
    .dead_cycles      = 0,
    .current_state    = 0
};

/*tex
    Because we have an extra order (fi) we need to intercept it in the case of the page builder
    although we could decide to support it here too.
*/

# define page_stretched(order) lmt_page_builder_state.page_so_far[page_stretch_state+order]

static void tex_aux_fire_up (halfword c);

/*tex

    The page builder has another data structure to keep track of insertions. This is a list of
    four-word nodes, starting and ending at |page_insert_head|. That is, the first element of the
    list is node |t$_1$ = node_next(page_insert_head)|; node $r_j$ is followed by |t$_{j+1}$ =
    node_next(t$_j$)|; and if there are |n| items we have |$_{n+1}$ >= page_insert_head|. The
    |subtype| field of each node in this list refers to an insertion number; for example, |\insert
    250| would correspond to a node whose |subtype| is |qi(250)| (the same as the |subtype| field
    of the relevant |insert_node|). These |subtype| fields are in increasing order, and |subtype
    (page_insert_head) = 65535|, so |page_insert_head| serves as a convenient sentinel at the end
    of the list. A record is present for each insertion number that appears in the current page.

    The |type| field in these nodes distinguishes two possibilities that might occur as we look
    ahead before deciding on the optimum page break. If |type(r) = inserting_node|, then |height(r)|
    contains the total of the height-plus-depth dimensions of the box and all its inserts seen so
    far. If |type(r) = split_up_node|, then no more insertions will be made into this box, because at
    least one previous insertion was too big to fit on the current page; |broken_ptr(r)| points to
    the node where that insertion will be split, if \TEX\ decides to split it, |broken_insert(r)|
    points to the insertion node that was tentatively split, and |height(r)| includes also the
    natural height plus depth of the part that would be split off.

    In both cases, |last_insert(r)| points to the last |insert_node| encountered for box
    |qo(subtype(r))| that would be at least partially inserted on the next page; and
    |best_insert(r)| points to the last such |insert_node| that should actually be inserted, to get
    the page with minimum badness among all page breaks considered so far. We have |best_insert
    (r) = null| if and only if no insertion for this box should be made to produce this optimum page.

    Pages are built by appending nodes to the current list in \TEX's vertical mode, which is at the
    outermost level of the semantic nest. This vlist is split into two parts; the \quote {current
    page} that we have been talking so much about already, and the |quote {contribution list} that
    receives new nodes as they are created. The current page contains everything that the page
    builder has accounted for in its data structures, as described above, while the contribution
    list contains other things that have been generated by other parts of \TEX\ but have not yet
    been seen by the page builder. The contribution list starts at |contribute_head|, and it
    ends at the current node in \TEX's vertical mode.

    When \TEX\ has appended new material in vertical mode, it calls the procedure |build_page|,
    which tries to catch up by moving nodes from the contribution list to the current page. This
    procedure will succeed in its goal of emptying the contribution list, unless a page break is
    discovered, i.e., unless the current page has grown to the point where the optimum next page
    break has been determined. In the latter case, the nodes after the optimum break will go back
    onto the contribution list, and control will effectively pass to the user's output routine.

    We make |type (page_head) = glue_node|, so that an initial glue node on the current page will
    not be considered a valid breakpoint. We keep this old tex trickery of cheating with node types
    but have to make sure that the size is valid to do so (and we have different sizes!).

*/

void tex_initialize_pagestate(void)
{
    lmt_page_builder_state.tail = page_head;
    lmt_page_builder_state.contents = contribute_nothing;
    lmt_page_builder_state.max_depth = 0;
    lmt_page_builder_state.best_break = null;
    lmt_page_builder_state.least_cost = 0;
    lmt_page_builder_state.best_size = 0;
    lmt_page_builder_state.goal = 0;
    lmt_page_builder_state.vsize = 0;
    lmt_page_builder_state.total = 0;
    lmt_page_builder_state.depth = 0;
    for (int i = page_initial_state; i <= page_shrink_state; i++) {
        lmt_page_builder_state.page_so_far[i] = 0;
    }
    lmt_page_builder_state.insert_penalties = 0;
    lmt_page_builder_state.insert_heights = 0;
    lmt_page_builder_state.last_glue = max_halfword;
    lmt_page_builder_state.last_penalty = 0;
    lmt_page_builder_state.last_kern = 0;
    lmt_page_builder_state.last_extra_used = 0;
    lmt_page_builder_state.last_boundary = 0;
    lmt_page_builder_state.last_node_type = unknown_node_type;
    lmt_page_builder_state.last_node_subtype = unknown_node_subtype;
    lmt_page_builder_state.output_active = 0;
    lmt_page_builder_state.dead_cycles = 0;
    lmt_page_builder_state.current_state = 0;
}

void tex_initialize_buildpage(void)
{
    node_type(page_insert_head) = split_node;
    node_subtype(page_insert_head) = insert_split_subtype;
    insert_index(page_insert_head) = 65535;          /*tex some signal */
    node_next(page_insert_head) = page_insert_head;
    node_type(page_head) = glue_node;                /*tex brr, a temp node has a different size than a glue node */
    node_subtype(page_head) = page_glue;             /*tex basically: unset */
}

/*tex

    An array |page_so_far| records the heights and depths of everything on the current page. This
    array contains six |scaled| numbers, like the similar arrays already considered in |line_break|
    and |vert_break|; and it also contains |page_goal| and |page_depth|, since these values are all
    accessible to the user via |set_page_dimension| commands. The value of |page_so_far[1]| is also
    called |page_total|. The stretch and shrink components of the |\skip| corrections for each
    insertion are included in |page_so_far|, but the natural space components of these corrections
    are not, since they have been subtracted from |page_goal|.

    The variable |page_depth| records the depth of the current page; it has been adjusted so that it
    is at most |page_max_depth|. The variable |last_glue| points to the glue specification of the
    most recent node contributed from the contribution list, if this was a glue node; otherwise
    |last_glue = max_halfword|. (If the contribution list is nonempty, however, the value of
    |last_glue| is not necessarily accurate.) The variables |last_penalty|, |last_kern|, and
    |last_node_type| are similar. And finally, |insert_penalties| holds the sum of the penalties
    associated with all split and floating insertions.

    Here is a procedure that is called when the |page_contents| is changing from |empty| to
    |inserts_only| or |box_there|.

*/

void tex_additional_page_skip(void)
{
    if (page_total > 0 && additional_page_skip_par) {
        page_stretch += glue_stretch(additional_page_skip_par);
        page_shrink += glue_shrink(additional_page_skip_par);
        page_total += glue_amount(additional_page_skip_par);
        update_tex_additional_page_skip(null);
    }
}

static void tex_aux_save_best_page_specs(void)
{
    page_last_depth = page_depth;
    page_last_height = page_total;
    page_last_stretch = page_stretch;
    page_last_fistretch = page_fistretch;
    page_last_filstretch = page_filstretch;
    page_last_fillstretch = page_fillstretch;
    page_last_filllstretch = page_filllstretch;
    page_last_shrink = page_shrink;
}

static void tex_aux_freeze_page_specs(int s)
{
    lmt_page_builder_state.contents = s;
    lmt_page_builder_state.max_depth = max_depth_par;
    lmt_page_builder_state.least_cost = awful_bad;
 /* page_builder_state.insert_heights = 0; */ /* up to the user */
    for (int i = page_initial_state; i <= page_shrink_state; i++) {
        lmt_page_builder_state.page_so_far[i] = 0;
        lmt_page_builder_state.page_last_so_far[i] = 0;
    }
    page_goal = vsize_par;
    page_vsize = vsize_par;
    page_depth = 0;
    page_total = 0;
    page_excess = 0;
    page_except = 0;
    page_last_depth = 0;
    page_last_height = 0;
    if (initial_page_skip_par) {
        page_stretch = glue_stretch(initial_page_skip_par);
        page_shrink = glue_shrink(initial_page_skip_par);
        page_total = glue_amount(initial_page_skip_par);
    }
    if (additional_page_skip_par) {
        page_stretch += glue_stretch(additional_page_skip_par);
        page_shrink += glue_shrink(additional_page_skip_par);
        page_total += glue_amount(additional_page_skip_par);
        update_tex_additional_page_skip(null);
    }
    if (tracing_pages_par > 0) {
        tex_begin_diagnostic();
        tex_print_format(
            "[page: frozen state, goal %p, maxdepth %p, contribution %s, insertheights %p]",
            page_goal,
            lmt_page_builder_state.max_depth,
            lmt_interface.page_contribute_values[s].name,
            lmt_page_builder_state.insert_heights
        );
        tex_end_diagnostic();
    }
}

static void update_page_goal(halfword index, scaled total, scaled delta)
{
    page_goal -= delta;
    lmt_page_builder_state.insert_heights += total;
    if (lmt_page_builder_state.insert_heights > max_dimension) {
        lmt_page_builder_state.insert_heights = max_dimension;
    }
    if (tracing_inserts_par > 0) {
        tex_begin_diagnostic();
        tex_print_format(
            "[page: update page goal for insert, index %i, total %p, insertheights %p, vsize %p, delta %p, goal %p]",
            index, total, lmt_page_builder_state.insert_heights, page_vsize, delta, page_goal
        );
        tex_end_diagnostic();
    }
}

/*tex

    The global variable |output_active| is true during the time the user's output routine is
    driving \TEX. The page builder is ready to start a fresh page if we initialize the following
    state variables. (However, the page insertion list is initialized elsewhere.)

*/

static void tex_aux_start_new_page(void)
{
    lmt_page_builder_state.contents = contribute_nothing;
    lmt_page_builder_state.tail = page_head;
    node_next(page_head) = null;
    lmt_page_builder_state.last_glue = max_halfword;
    lmt_page_builder_state.last_penalty = 0;
    lmt_page_builder_state.last_kern = 0;
    lmt_page_builder_state.last_boundary = 0;
    lmt_page_builder_state.last_node_type = unknown_node_type;
    lmt_page_builder_state.last_node_subtype = unknown_node_subtype;
    page_depth = 0;
    lmt_page_builder_state.max_depth = 0;
}

/*tex

    At certain times box |\outputbox| is supposed to be void (i.e., |null|), or an insertion box is
    supposed to be ready to accept a vertical list. If not, an error message is printed, and the
    following subroutine flushes the unwanted contents, reporting them to the user.

*/

static halfword tex_aux_delete_box_content(int n, const char *what, int where)
{
    if (tracing_pages_par > 0) {
        tex_begin_diagnostic();
        tex_print_format("[page: deleting %s box, case %i]", what, where);
        tex_show_box(n);
        tex_end_diagnostic();
    }
    tex_flush_node_list(n);
    return null;
}

/*tex

    The following procedure guarantees that an insert box is not an |\hbox|. A user can actually
    mess with this box, unless we decide to come up with a dedicated data structure for it.

*/

static int tex_aux_valid_insert_content(halfword content)
{
    if (content && node_type(content) == hlist_node) {
        /*tex It's not always a box so we need to adapt this message some day. */
        tex_handle_error(
            normal_error_type,
            "Insertions can only be added to a vbox",
            "Tut tut: You're trying to \\insert into a \\box register that now contains an\n"
            "\\hbox. Proceed, and I'll discard its present contents."
        );
        return 0;
    } else {
        return 1;
    }
}

/*tex

    \TEX\ is not always in vertical mode at the time |build_page| is called; the current mode
    reflects what \TEX\ should return to, after the contribution list has been emptied. A call on
    |build_page| should be immediately followed by |goto big_switch|, which is \TEX's central
    control point.

    Append contributions to the current page.

*/

static void tex_aux_display_page_break_cost(int context, halfword badness, halfword penalty, halfword cost, int moveon, int fireup)
{
    tex_begin_diagnostic();
    tex_print_format("[page: break at %s, total %P, goal %p, badness %B, penalty %i, cost %B%s, moveon %s, fireup %s]",
        lmt_interface.page_context_values[context].name, 
        page_total, page_stretch, page_fistretch, page_filstretch, page_fillstretch, page_filllstretch, page_shrink,
        page_goal, badness, penalty, cost, cost < lmt_page_builder_state.least_cost ? "#" : "",
        moveon ? "yes" : "no", fireup ? "yes" : "no"
    );
    tex_end_diagnostic();
}

static void tex_aux_display_insertion_split_cost(halfword index, scaled height, halfword penalty)
{
    /*tex Display the insertion split cost. */
    tex_begin_diagnostic();
    tex_print_format("[page: split insert %i: height %p, depth %p, penalty %i]",
        index, height, lmt_packaging_state.best_height_plus_depth, penalty
    );
    tex_end_diagnostic();
}

static halfword tex_aux_page_badness(scaled goal)
{
    if (page_total + page_except < goal) {
        if (page_fistretch || page_filstretch || page_fillstretch || page_filllstretch) {
            return 0;
        } else {
            return tex_badness(goal - page_total, page_stretch);
        }
    } else if (page_total + page_except - goal > page_shrink) {
        return awful_bad;
    } else {
        return tex_badness(page_total + page_except - goal, page_shrink);
    }
}

static inline halfword tex_aux_page_costs(halfword badness, halfword penalty)
{
    if (lmt_page_builder_state.insert_penalties >= infinite_penalty) {
        return awful_bad;
    } else if (badness >= awful_bad) {
        return badness; /* trigger fireup */
    } else if (penalty <= eject_penalty) {
        return penalty; /* trigger fireup */
    } else if (badness < infinite_bad) {
        return badness + penalty + lmt_page_builder_state.insert_penalties;
    } else {
        return deplorable;
    }
}

static halfword tex_aux_insert_topskip(halfword height, int contribution)
{
    if (lmt_page_builder_state.contents != contribute_nothing) {
        lmt_page_builder_state.contents = contribution;
    } else {
        tex_aux_freeze_page_specs(contribution);
    }
    {
        halfword glue = tex_new_param_glue_node(tex_glue_is_zero(initial_top_skip_par) ? top_skip_code : initial_top_skip_code, top_skip_glue);
        if (glue_amount(glue) > height) {
            glue_amount(glue) -= height;
        } else {
            glue_amount(glue) = 0;
        }
        return glue;
    }
}

/*tex
    Append an insertion to the current page and |goto contribute|. The insertion number (index) is
    registered in the subtype (not any more for a while).
*/

static void tex_aux_append_insert(halfword current)
{
    halfword index = insert_index(current); /* initially 65K */
    halfword location = page_insert_head;
    halfword multiplier = tex_get_insert_multiplier(index);
    halfword content = tex_get_insert_content(index);
    scaled limit = tex_get_insert_limit(index);
    int slot = 1;
    if (lmt_page_builder_state.contents == contribute_nothing) {
        tex_aux_freeze_page_specs(contribute_insert);
    }
    while (index >= insert_index(node_next(location))) {
        location = node_next(location);
        slot += 1 ;
    }
    if (insert_index(location) != index) {
        /*tex

            Create a page insertion node with |subtype(r) = qi(n)|, and include the glue correction
            for box |n| in the current page state. We take note of the value of |\skip| |n| and the
            height plus depth of |\box| |n| only when the first |\insert n| node is encountered for
            a new page. A user who changes the contents of |\box| |n| after that first |\insert n|
            had better be either extremely careful or extremely lucky, or both.

            We need to handle this too:

            [content]
            [max(space shared,space n)]
            [class n]
            .........
            [space m]
            [class m]

            For now a callback can deal with this but maybe we need to have a more advanced
            mechanism for this (and more control over inserts in general).

        */
        halfword splitnode = tex_new_node(split_node, normal_split_subtype);
        scaled advance = 0;
        halfword distance = lmt_get_insert_distance(index, slot); /*tex Callback: we get a copy! */
        split_insert_index(splitnode) = index;
        tex_try_couple_nodes(splitnode, node_next(location));
        tex_couple_nodes(location, splitnode);
        location = splitnode;
        if (! tex_aux_valid_insert_content(content)) {
            content = tex_aux_delete_box_content(content, "insert", 1);
            tex_set_insert_content(index, content);
        };
        if (content) {
            box_height(location) = box_total(content);
        } else {
            box_height(location) = 0;
        }
        split_best_insert(location) = null;
        if (multiplier == scaling_factor) {
            advance = box_height(location);
        } else {
            advance = tex_x_over_n(box_height(location), scaling_factor) * multiplier;
        }
        advance += glue_amount(distance);
        update_page_goal(index, 0, advance); /*tex Here gets no height added! */
        page_stretched(glue_stretch_order(distance)) += glue_stretch(distance);
        page_shrink += glue_shrink(distance);
        if (glue_shrink_order(distance) != normal_glue_order && glue_shrink(distance)) {
            tex_handle_error(
                normal_error_type,
                "Infinite glue shrinkage inserted from \\skip%i",
                index,
                "The correction glue for page breaking with insertions must have finite\n"
                "shrinkability. But you may proceed, since the offensive shrinkability has been\n"
                "made finite."
            );
        }
        tex_flush_node(distance);
    }
    /*tex I really need to check this logic with the original \LUATEX\ code. */
    if (node_type(location) == split_node && node_subtype(location) == insert_split_subtype) {
        lmt_page_builder_state.insert_penalties += insert_float_cost(current);
    } else {
        scaled delta = page_goal - page_total - page_depth + page_shrink;
        scaled needed = insert_total_height(current);
        split_last_insert(location) = current;
        /*tex This much room is left if we shrink the maximum. */
        if (multiplier != scaling_factor) {
            /*tex This much room is needed. */
            needed = tex_x_over_n(needed, scaling_factor) * multiplier;
        }
        if ((needed <= 0 || needed <= delta) && (insert_total_height(current) + box_height(location) <= limit)) {
            update_page_goal(index, insert_total_height(current), needed);
            box_height(location) += insert_total_height(current);
        } else {
            /*tex

                Find the best way to split the insertion, and change |subtype(r)| to
                |split_up_inserting_code|.

                Here is the code that will split a long footnote between pages, in an emergency.
                The current situation deserves to be recapitulated: Node |p| is an insertion
                into box |n|; the insertion will not fit, in its entirety, either because it
                would make the total contents of box |n| greater than |\dimen| |n|, or because
                it would make the incremental amount of growth |h| greater than the available
                space |delta|, or both. (This amount |h| has been weighted by the insertion
                scaling factor, i.e., by |\count| |n| over 1000.) Now we will choose the best
                way to break the vlist of the insertion, using the same criteria as in the
                |\vsplit| operation.

            */
            scaled height;
            halfword breaknode, penalty;
            if (multiplier <= 0) {
                height = max_dimension;
            } else {
                height = page_goal - page_total - page_depth;
                if (multiplier != scaling_factor) {
                    height = tex_x_over_n(height, multiplier) * scaling_factor;
                }
            }
            if (height > limit - box_height(location)) {
                height = limit - box_height(location);
            }
            breaknode = tex_vert_break(insert_list(current), height, insert_max_depth(current), 0, 0);
            box_height(location) += lmt_packaging_state.best_height_plus_depth;
            penalty = breaknode ? (node_type(breaknode) == penalty_node ? penalty_amount(breaknode) : 0) : eject_penalty;
            if (tracing_pages_par > 0) {
                tex_aux_display_insertion_split_cost(index, height, penalty);
            }
            if (multiplier != scaling_factor) {
                lmt_packaging_state.best_height_plus_depth = tex_x_over_n(lmt_packaging_state.best_height_plus_depth, scaling_factor) * multiplier;
            }
            update_page_goal(index, lmt_packaging_state.best_height_plus_depth, lmt_packaging_state.best_height_plus_depth);
            node_subtype(location) = insert_split_subtype;
            split_broken(location) = breaknode;
            split_broken_insert(location) = current;
            lmt_page_builder_state.insert_penalties += penalty;
        }
    }
}

static inline int tex_aux_get_penalty_option(halfword current)
{
    while (1) {
        current = node_prev(current);
        if (current && current != contribute_head) {
            switch (node_type(current)) {
                case glue_node:
                    break;
                case penalty_node:
                    if (penalty_amount(current) >= infinite_penalty) {
                        if (tex_has_penalty_option(current, penalty_option_widowed)) {
                            if (tracing_pages_par > 1) {
                                tex_begin_diagnostic();
                                tex_print_format("[page: widowed]");
                                tex_end_diagnostic();
                            }
                            return penalty_option_widowed;
                        } else if (tex_has_penalty_option(current, penalty_option_clubbed)) {
                            if (tracing_pages_par > 1) {
                                tex_begin_diagnostic();
                                tex_print_format("[page: clubbed]");
                                tex_end_diagnostic();
                            }
                            return penalty_option_clubbed;
                        }
                    }
                    return 0;
                default:
                    return 0;
            }
        } else {
            return 0;
        }
    }
 // return 0;
}

static inline int tex_aux_get_last_penalty(halfword current)
{
    return node_type(current) == penalty_node
        ? penalty_options(current) & (penalty_option_widow | penalty_option_club | penalty_option_broken | penalty_option_shaping)
        : 0;
}

static void tex_aux_initialize_show_build_node(int callback_id)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "d->", initialize_show_build_context);
}

static void tex_aux_step_show_build_node(int callback_id, halfword current)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dNdd->", step_show_build_context,
        current,
        page_goal,
        page_total
    );
}

static void tex_aux_check_show_build_node(int callback_id, halfword current, int badness, int costs, int penalty, int *moveon, int *fireup)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dNbbddd->bb", check_show_build_context,
        current,
        *moveon,
        *fireup,
        badness,
        costs,
        penalty,
        moveon,
        fireup
    );
}

static void tex_aux_skip_show_build_node(int callback_id, halfword current)
{
    (void) current;
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dN->", skip_show_build_context);
}

static void tex_aux_move_show_build_node(int callback_id, halfword current)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dNddddb->", move_show_build_context,
        current,
        page_last_height,
        page_last_depth,
        page_last_stretch,
        page_last_shrink,
        (page_last_fistretch || page_last_filstretch || page_last_fillstretch || page_last_filllstretch) ? 1 : 0
    );
}

static void tex_aux_fireup_show_build_node(int callback_id, halfword current)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dN->", fireup_show_build_context,
        current
    );
}

static void tex_aux_wrapup_show_build_node(int callback_id)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "d->", wrapup_show_build_context);
}

static void tex_aux_show_loner_penalty(int callback_id, halfword options, scaled penalty)
{
    lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "dd->", options, penalty);
}

static int tex_aux_topskip_restart(halfword current, int where, scaled height, scaled depth, scaled exdepth, int discard, int tracing)
{
    if (lmt_page_builder_state.contents < contribute_box) {
        /*tex
            Initialize the current page, insert the |\topskip| glue ahead of |p|, and |goto
            continue|.
        */
        if (discard) {
            return 2;
        } else {
            halfword gluenode = tex_aux_insert_topskip(height, where);
            tex_attach_attribute_list_copy(gluenode, current);
            tex_couple_nodes(gluenode, current);
            tex_couple_nodes(contribute_head, gluenode);
            if (tracing > 1) {
                tex_begin_diagnostic();
                tex_print_format("[page: initialize, topskip at %s]", where == contribute_box ? "box" : "rule");
                tex_end_diagnostic();
            }
        }
        return 1;
    } else {
        /*tex Move a box to the current page, then |goto contribute|. */
        page_total += page_depth + height;
        page_depth = depth;
        page_except -= height;
        page_except -= depth;
        if (exdepth > page_except) {
            page_except = exdepth;
        }
        if (page_except < 0) {
            page_except = 0;
        }
        return 0;
    }
}

static int tex_aux_migrating_restart(halfword current, int tracing)
{
    if (auto_migrating_mode_permitted(auto_migration_mode_par, auto_migrate_post)) {
        halfword head = box_post_migrated(current);
        if (head) {
            halfword tail = tex_tail_of_node_list(head);
            if (tracing > 1 || tracing_adjusts_par > 1) {
                tex_begin_diagnostic();
                tex_print_format("[adjust: post, mvl]");
                tex_print_node_list(head, "post", show_box_depth_par, show_box_breadth_par);
                tex_end_diagnostic();
            }
            if (node_next(current)) {
                tex_couple_nodes(tail, node_next(current));
            } else {
                contribute_tail = tail;
            }
            tex_couple_nodes(current, head);
            box_post_migrated(current) = null;
        }
    }
    if (auto_migrating_mode_permitted(auto_migration_mode_par, auto_migrate_pre)) {
        halfword head = box_pre_migrated(current);
        if (head) {
            halfword tail = tex_tail_of_node_list(head);
            if (tracing > 1 || tracing_adjusts_par > 1) {
                tex_begin_diagnostic();
                tex_print_format("[adjust: pre, mvl]");
                tex_print_node_list(head, "pre", show_box_depth_par, show_box_breadth_par);
                tex_end_diagnostic();
            }
            tex_couple_nodes(tail, current);
            tex_couple_nodes(contribute_head, current);
            // if (contribute_head == contribute_tail) {
            //     contribute_tail = tail;
            // }
            box_pre_migrated(current) = null;
            return 1;
        }
    }
    return 0;
}

/*tex
    We just triggered the page builder for which we needed a contribution. We fake a zero penalty
    so that all gets processed. The main rationale is that we get a better indication of what we
    do. Of course a callback can remove this node  so that it is never seen. Triggering from the
    callback is not doable.
*/

static halfword tex_aux_process_boundary(halfword current)
{
    halfword penaltynode = tex_new_node(penalty_node, user_penalty_subtype);
    /* todo: copy attributes */
    tex_page_boundary_message("processed as penalty", 0);
    tex_try_couple_nodes(node_prev(current), penaltynode);
    tex_try_couple_nodes(penaltynode, node_next(current));
    tex_flush_node(current);
    penalty_amount(penaltynode) = boundary_data(current);
    current = penaltynode;
    node_next(contribute_head) = current;
    return current;
}

static void tex_aux_reconsider_goal(halfword current, halfword *badness, halfword *costs, halfword *penalty, int tracing)
{
    (void) current;
    if (*badness >= awful_bad && page_extra_goal_par) {
        switch (tex_aux_get_penalty_option(lmt_page_builder_state.tail)) {
            case penalty_option_widowed:
                if (page_total <= (page_goal + page_extra_goal_par)) {
                    halfword extrabadness = tex_aux_page_badness(page_goal + page_extra_goal_par);
                    halfword extracosts = tex_aux_page_costs(extrabadness, *penalty);
                    if (tracing > 0) {
                        tex_begin_diagnostic();
                        tex_print_format(
                            "[page: extra check, total %P, goal %p, extragoal %p, badness %B, costs %i, extrabadness %B, extracosts %i]",
                            page_total, page_stretch, page_filstretch, page_filstretch, page_fillstretch, page_filllstretch, page_shrink,
                            page_goal, page_extra_goal_par,
                            *badness, *costs, extrabadness, extracosts
                        );
                        tex_end_diagnostic();
                    }
                    /* we need to append etc. so an extra loop cycle */
                    *badness = extrabadness;
                    *costs = extracosts;
                    /* */
                    lmt_page_builder_state.last_extra_used = 1;
                    if (tracing > 1) {
                        tex_begin_diagnostic();
                        tex_print_format("[page: widowed]");
                        tex_end_diagnostic();
                    }
                }
                break;
            case penalty_option_clubbed:
                if (page_total >= (page_goal - page_extra_goal_par)) {
                    /* we force a flush and no extra loop cycle */
                    *penalty = eject_penalty;
                    /* */
                    if (tracing > 1) {
                        tex_begin_diagnostic();
                        tex_print_format("[page: clubbed]");
                        tex_end_diagnostic();
                    }
                }
                break;
        }
    }
}

static void tex_aux_contribute_glue(halfword current)
{
    page_stretched(glue_stretch_order(current)) += glue_stretch(current);
    page_shrink += glue_shrink(current);
    if (glue_shrink_order(current) != normal_glue_order && glue_shrink(current)) {
        tex_handle_error(
            normal_error_type,
            "Infinite glue shrinkage found on current page",
            "The page about to be output contains some infinitely shrinkable glue, e.g.,\n"
            "'\\vss' or '\\vskip 0pt minus 1fil'. Such glue doesn't belong there; but you can\n"
            "safely proceed, since the offensive shrinkability has been made finite."
        );
        tex_reset_glue_to_zero(current);
        glue_shrink_order(current) = normal_glue_order;
    }
}

/*tex
    Maybe: migrate everything beforehand is somewhat nicer when we use the builder for multi-column
    balancing etc.
*/

static inline halfword tex_aux_used_penalty(halfword p)
{
    if (double_penalty_mode_par && tex_has_penalty_option(p, penalty_option_double)) {
        tex_add_penalty_option(p, penalty_option_double_used);
        return penalty_tnuoma(p);
    } else {
        tex_remove_penalty_option(p, penalty_option_double_used);
        return penalty_amount(p);
    }
}

static void tex_process_mvl(halfword context, halfword boundary)
{
    if (node_next(contribute_head) && ! lmt_page_builder_state.output_active) {
 // if (! lmt_page_builder_state.output_active) {
        lmt_buildpage_callback(context, boundary);
    }
    if (node_next(contribute_head) && ! lmt_page_builder_state.output_active) {
        /*tex The (upcoming) penalty to be added to the badness: */
        halfword penalty = 0;
        int callback_id = lmt_callback_defined(show_build_callback);
        int tracing = tracing_pages_par;
        if (callback_id) {
            tex_aux_initialize_show_build_node(callback_id);
        }
        do {
            /*tex
                We update the values of |\lastskip|, |\lastpenalty|, |\lastkern| and
                |\lastboundary| as we go.
            */
            halfword current = node_next(contribute_head);
            halfword type = node_type(current);
            quarterword subtype = node_subtype(current);
            if (callback_id) {
                tex_aux_step_show_build_node(callback_id, current);
            }
            if (lmt_page_builder_state.last_glue != max_halfword) {
                tex_flush_node(lmt_page_builder_state.last_glue);
                lmt_page_builder_state.last_glue = max_halfword;
            }
            lmt_page_builder_state.last_penalty = 0;
            lmt_page_builder_state.last_kern = 0;
            lmt_page_builder_state.last_boundary = 0;
            lmt_page_builder_state.last_node_type = type;
            lmt_page_builder_state.last_node_subtype = subtype;
            lmt_page_builder_state.last_extra_used = 0;
            /*tex

                Move node |p| to the current page; if it is time for a page break, put the nodes
                following the break back onto the contribution list, and |return| to the users
                output routine if there is one.

                The code here is an example of a many-way switch into routines that merge together
                in different places. Some people call this unstructured programming, but the author
                doesn't see much wrong with it, as long as the various labels have a well-understood
                meaning.

                If the current page is empty and node |p| is to be deleted, |goto done1|; otherwise
                use node |p| to update the state of the current page; if this node is an insertion,
                |goto contribute|; otherwise if this node is not a legal breakpoint,
                |goto contribute| or |update_heights|; otherwise set |pi| to the penalty associated
                with this breakpoint.

                The title of this section is already so long, it seems best to avoid making it more
                accurate but still longer, by mentioning the fact that a kern node at the end of
                the contribution list will not be contributed until we know its successor.

            */
            switch (type) {
                case hlist_node:
                case vlist_node:
                    if (tex_aux_migrating_restart(current, tracing)) {
                        continue;
                    } else {
                        switch (tex_aux_topskip_restart(current, contribute_box, box_height(current), box_depth(current), box_exdepth(current), (box_options(current) & box_option_discardable), tracing)) {
                            case 1:
                                continue;
                            case 2:
                                /* todo: remove */
                                box_height(current) = 0;
                                box_depth(current) = 0;
                                continue;
                            default:
                                goto CONTRIBUTE;
                        }
                    }
                case rule_node:
                    switch (tex_aux_topskip_restart(current, contribute_rule, rule_height(current), rule_depth(current), 0, (rule_options(current) & rule_option_discardable), tracing)) {
                        case 1:
                            continue;
                        case 2:
                            /* todo: remove */
                            rule_height(current) = 0;
                            rule_depth(current) = 0;
                            continue;
                        default:
                            goto CONTRIBUTE;
                    }
                case boundary_node:
                    if (subtype == page_boundary) {
                        lmt_page_builder_state.last_boundary = boundary_data(current);
                    }
                    if (lmt_page_builder_state.contents < contribute_box) {
                        goto DISCARD;
                    } else if (subtype == page_boundary) {
                        current = tex_aux_process_boundary(current);
                        penalty = 0;
                        break;
                    } else {
                        goto DISCARD;
                    }
                case whatsit_node:
                    goto CONTRIBUTE;
                case glue_node:
                    lmt_page_builder_state.last_glue = tex_new_glue_node(current, subtype);
                    if (lmt_page_builder_state.contents < contribute_box) {
                        goto DISCARD;
                    } else if (precedes_break(lmt_page_builder_state.tail)) {
                        penalty = 0;
                        break;
                    } else {
                        goto UPDATEHEIGHTS;
                    }
                case kern_node:
                    lmt_page_builder_state.last_kern = kern_amount(current);
                    if (lmt_page_builder_state.contents < contribute_box) {
                        goto DISCARD;
                    } else if (! node_next(current)) {
                        return;
                    } else if (node_type(node_next(current)) == glue_node) {
                        penalty = 0;
                        break;
                    } else {
                        goto UPDATEHEIGHTS;
                    }
                case penalty_node:
                    lmt_page_builder_state.last_penalty = penalty_amount(current);
                    if (lmt_page_builder_state.contents < contribute_box) {
                        goto DISCARD;
                    } else {
                        penalty = tex_aux_used_penalty(current);
                        break;
                    }
                case mark_node:
                    goto CONTRIBUTE;
                case insert_node:
                    tex_aux_append_insert(current);
                    goto CONTRIBUTE;
                default:
                    tex_handle_error(
                        // normal_error_type,
                        succumb_error_type,
                        "Invalid %N node in pagebuilder",
                        current,
                        NULL
                    );
                    goto DISCARD;
                    // tex_formatted_error("pagebuilder", "invalid %N node in vertical mode", current);
                    break;
            }
            /*tex
                Check if node |p| is a new champion breakpoint; then if it is time for a page break,
                prepare for output, and either fire up the users output routine and |return| or
                ship out the page and |goto done|.
            */
            if (tracing > 1) {
                tex_begin_diagnostic();
                tex_print_format("[page: compute: %N, %d, penalty %i]", current, current, penalty);
                tex_end_diagnostic();
            }
            if (penalty < infinite_penalty) {
                /*tex
                    Compute the badness, |b|, of the current page, using |awful_bad| if the box is
                    too full. The |c| variable holds the costs.
                */
                halfword badness = tex_aux_page_badness(page_goal);
                halfword costs = tex_aux_page_costs(badness, penalty);
                lmt_page_builder_state.last_extra_used = 0;
                if (tracing > 1) {
                    tex_begin_diagnostic();
                    tex_print_format("[page: calculate, %N, %d, total %P, goal %p, badness %B, costs %i]",
                        current, current,
                        page_total, page_stretch, page_fistretch, page_filstretch, page_fillstretch, page_filllstretch, page_shrink,
                        page_goal,
                        badness, costs
                    );
                    tex_end_diagnostic();
                }
                tex_aux_reconsider_goal(current, &badness, &costs, &penalty, tracing);
                {
                    int moveon = costs <= lmt_page_builder_state.least_cost;
                    int fireup = costs == awful_bad || penalty <= eject_penalty;
                    if (callback_id) {
                        tex_aux_check_show_build_node(callback_id, current, badness, lmt_page_builder_state.last_penalty, costs, &moveon, &fireup);
                    }
                    if (tracing > 0) {
                        tex_aux_display_page_break_cost(context, badness, penalty, costs, moveon, fireup);
                    }
                    if (moveon) {
                        halfword insert = node_next(page_insert_head);
                        lmt_page_builder_state.best_break = current;
                        lmt_page_builder_state.best_size = page_goal;
                        lmt_page_builder_state.insert_penalties = 0;
                        lmt_page_builder_state.least_cost = costs;
                        while (insert != page_insert_head) {
                            split_best_insert(insert) = split_last_insert(insert);
                            insert = node_next(insert);
                        }
                        tex_aux_save_best_page_specs();
                        if (callback_id) {
                            tex_aux_move_show_build_node(callback_id, current);
                        }
                    }
                    if (fireup) {
                        if (tracing > 1) {
                            tex_begin_diagnostic();
                            tex_print_format("[page: fireup: %N, %d]", current, current);
                            tex_end_diagnostic();
                        }
                        if (callback_id) {
                            tex_aux_fireup_show_build_node(callback_id, current);
                        }
                        /*tex Output the current page at the best place. */
                        tex_aux_fire_up(current);
                        if (lmt_page_builder_state.output_active) {
                            /*tex User's output routine will act. */
                            if (callback_id) {
                                tex_aux_wrapup_show_build_node(callback_id);
                            }
                            return;
                        } else {
                            /*tex The page has been shipped out by default output routine. */
                            continue;
                        }
                    }
                }
            } else {
                if (callback_id) {
                    tex_aux_skip_show_build_node(callback_id, current);
                }
            }
            UPDATEHEIGHTS:
            /*tex
                Go here to record glue in the |active_height| table. Update the current page
                measurements with respect to the glue or kern specified by node~|p|.
            */
            if (tracing > 1) {
                tex_begin_diagnostic();
                tex_print_format("[page: update, %N, %d]", current, current);
                tex_end_diagnostic();
            }
            switch(node_type(current)) {
                case kern_node:
                    if (page_except) {
                        page_except -= kern_amount(current);
                        if (page_except < 0) {
                            page_except = 0;
                        }
                    }
                    page_total += page_depth + kern_amount(current);
                    page_depth = 0;
                    goto APPEND;
                case glue_node:
                    if (page_except) {
                        glue_stretch(current) = 0; /* why not also shrink etc */
                        page_except -= glue_amount(current);
                        if (page_except < 0) {
                            page_except = 0;
                        }
                    }
                    tex_aux_contribute_glue(current);
                    page_total += page_depth + glue_amount(current);
                    page_depth = 0;
                    goto APPEND;
            }
            CONTRIBUTE:
            /*tex
                Go here to link a node into the current page. Make sure that |page_max_depth| is
                not exceeded.
            */
            if (tracing > 1) {
                tex_begin_diagnostic();
                tex_print_format("[page: contribute, %N, %d]", current, current);
                tex_end_diagnostic();
            }
            if (page_depth > lmt_page_builder_state.max_depth) {
                page_total += page_depth - lmt_page_builder_state.max_depth;
                page_depth = lmt_page_builder_state.max_depth;
            }
            APPEND:
            if (tracing > 1) {
                tex_begin_diagnostic();
                tex_print_format("[page: append, %N, %d]", current, current);
                tex_end_diagnostic();
            }
            /*tex Link node |p| into the current page and |goto done|. We assume a positive depth. */
            tex_couple_nodes(lmt_page_builder_state.tail, current);
            lmt_page_builder_state.tail = current;
            tex_try_couple_nodes(contribute_head, node_next(current));
            node_next(current) = null;
            continue; // or: break;
            DISCARD:
            if (tracing > 1) {
                tex_begin_diagnostic();
                tex_print_format("[page: discard, %N, %d]", current, current);
                tex_end_diagnostic();
            }
            /*tex Recycle node |p|. */
            tex_try_couple_nodes(contribute_head, node_next(current));
            node_next(current) = null;
            if (saving_vdiscards_par > 0) {
                if (lmt_packaging_state.page_discards_head) {
                    tex_couple_nodes(lmt_packaging_state.page_discards_tail, current);
                } else {
                    lmt_packaging_state.page_discards_head = current;
                }
                lmt_packaging_state.page_discards_tail = current;
            } else {
                tex_flush_node_list(current);
            }
        } while (node_next(contribute_head));
        /*tex Make the contribution list empty by setting its tail to |contribute_head|. */
        contribute_tail = contribute_head;
    }
}

void tex_build_page(halfword context, halfword boundary)
{
    if (! tex_appended_mvl(context, boundary)) {
        tex_process_mvl(context, boundary);
    }
}

/*tex

    When the page builder has looked at as much material as could appear before the next page break,
    it makes its decision. The break that gave minimum badness will be used to put a completed page
    into box |\outputbox|, with insertions appended to their other boxes.

    We also set the values of |top_mark|, |first_mark|, and |bot_mark|. The program uses the fact
    that |bot_mark(x) <> null| implies |first_mark(x) <> null|; it also knows that |bot_mark(x) =
    null| implies |top_mark(x) = first_mark(x) = null|.

    The |fire_up| subroutine prepares to output the current page at the best place; then it fires
    up the user's output routine, if there is one, or it simply ships out the page. There is one
    parameter, |c|, which represents the node that was being contributed to the page when the
    decision to force an output was made.

*/

static void tex_aux_fire_up(halfword c)
{
    /*tex nodes being examined and/or changed */
    halfword current, previous, lastinsert;
    /*tex Set the value of |output_penalty|. */
    if (node_type(lmt_page_builder_state.best_break) == penalty_node) {
        update_tex_output_penalty(penalty_amount(lmt_page_builder_state.best_break));
        if (tracing_loners_par) {
            int callback_id = lmt_callback_defined(show_loners_callback);
            // this one should return the str
            halfword n = tex_aux_get_last_penalty(lmt_page_builder_state.best_break);
            if (n) {
                halfword penalty = tex_aux_used_penalty(lmt_page_builder_state.best_break);
                if (callback_id) {
                    tex_aux_show_loner_penalty(callback_id, penalty_options(lmt_page_builder_state.best_break), penalty);
                } else {
                    unsigned char state[5] = { '.', '.', '.', '.', '\0' };
                    if (tex_has_penalty_option(lmt_page_builder_state.best_break, penalty_option_widow  )) { state[0] = 'W'; }
                    if (tex_has_penalty_option(lmt_page_builder_state.best_break, penalty_option_club   )) { state[1] = 'C'; }
                    if (tex_has_penalty_option(lmt_page_builder_state.best_break, penalty_option_broken )) { state[2] = 'B'; }
                    if (tex_has_penalty_option(lmt_page_builder_state.best_break, penalty_option_shaping)) { state[3] = 'S'; }
                    tex_begin_diagnostic();
                    tex_print_format("[page: loner: %s penalty %i]", state, penalty);
                    tex_end_diagnostic();
                }
            }
        }
        penalty_amount(lmt_page_builder_state.best_break) = infinite_penalty;
        penalty_tnuoma(lmt_page_builder_state.best_break) = infinite_penalty;
    } else {
        update_tex_output_penalty(infinite_penalty);
    }
    tex_update_top_marks();
    /*tex
        Put the optimal current page into box |output_box|, update |first_mark| and |bot_mark|,
        append insertions to their boxes, and put the remaining nodes back on the contribution
        list.

        As the page is finally being prepared for output, pointer |p| runs through the vlist, with
        |prev_p| trailing behind; pointer |q| is the tail of a list of insertions that are being
        held over for a subsequent page.
    */
    if (c == lmt_page_builder_state.best_break) {
        /*tex |c| not yet linked in */
        lmt_page_builder_state.best_break = null;
    }
    /*tex Ensure that box |output_box| is empty before output. */
    if (box_register(output_box_par)) {
        if (! ((no_output_box_error_par > 2) || (no_output_box_error_par & 1))) {
            tex_handle_error(
                normal_error_type,
                "\\box%i is not void",
                output_box_par,
                "You shouldn't use \\box\\outputbox except in \\output routines. Proceed, and I'll\n"
                "discard its present contents."
            );
        }
        box_register(output_box_par) = tex_aux_delete_box_content(box_register(output_box_par), "output", 1);
    }
    /*
    {
        int callback_id = lmt_callback_defined(fire_up_output_callback);
        if (callback_id != 0) {
            halfword insert = node_next(page_insert_head);
            lmt_run_callback(lmt_lua_state.lua_instance, callback_id, "->");
        }
    }
    */
    /*tex This will count the number of insertions held over. */
    {
        halfword save_split_top_skip = split_top_skip_par;
        lmt_page_builder_state.insert_penalties = 0;
        if (holding_inserts_par <= 0) {
            /*tex
                Prepare all the boxes involved in insertions to act as queues. If many insertions are
                supposed to go into the same box, we want to know the position of the last node in that
                box, so that we don't need to waste time when linking further information into it. The
                |last_insert| fields of the page insertion nodes are therefore used for this purpose
                during the packaging phase.

                This is tricky: |last_insert| directly points to a \quote {address} in the node list,
                that is: the row where |list_ptr| sits. The |raw_list_ptr| macro is just an offset to
                the base index of the node. Then |node_next| will start out there and follow the list.
                So, |last_insert| kind of points to a subnode (as in disc nodes) of size 1.

                    last_insert => [shift][list]

                which fakes:

                    last_insert => [type|subtype][next] => [real node with next]

                and with shift being zero this (when it would be queried) will be seen as a hlist node
                of type zero with subtype zero, but ... it is not really such a node which means that
                other properties are not valid! Normally this is ok, because \TEX\ only follows this
                list and never looks at the parent. But, when accessing from \LUA\ this is asking for
                troubles. However, as all happens in the page builder, we don't really expose this and
                if we would (somehow, e.g. via a callback) then for sure we would need to make sure
                that the node |last_insert(r)| points to is made into a new kind of node: one with
                size 1 and type |fake_node| or so, just to be sure (so that at the \LUA\ end no
                properties can be asked).

                Of course I can be wrong here and changing the approach would involve patching some
                code that I don't want to touch. I need a test case for \quote {following the chain}.
            */
            halfword insert = node_next(page_insert_head);
            while (insert != page_insert_head) {
                if (split_best_insert(insert)) {
                    halfword index = insert_index(insert);
                    halfword content = tex_get_insert_content(index);
                    if (! tex_aux_valid_insert_content(content)) {
                        content = tex_aux_delete_box_content(content, "insert", 2);
                    }
                    if (! content) {
                        /*tex
                            So we package the content in a box. Originally this is a hlist which
                            is somewhat strange because we're operating in vmode. The box is still
                            empty!
                        */
                        content = tex_new_null_box_node(vlist_node, insert_result_list);
                        tex_set_insert_content(index, content);
                    }
                    /*tex
                        We locate the place where we can add. We have an (unpackaged) list here so we
                        need to go to the end. Here we have this sort of hackery |box(n) + 5 == row of
                        list ptr, a fake node of size 1| trick.
                    */
                    split_last_insert(insert) = tex_tail_of_node_list(insert_first_box(content));
                }
                insert = node_next(insert);
            }
        }
        previous = page_head;
        current = node_next(previous);
        lastinsert = hold_head;
        node_next(lastinsert) = null;
        while (current != lmt_page_builder_state.best_break) {
            switch (node_type(current)) {
                case insert_node:
                    if (holding_inserts_par <= 0) {
                        /*tex
                            Either insert the material specified by node |p| into the appropriate box, or
                            hold it for the next page; also delete node |p| from the current page.

                            We will set |best_insert := null| and package the box corresponding to
                            insertion node |r|, just after making the final insertion into that box. If
                            this final insertion is |split_up_node|, the remainder after splitting and
                            pruning (if any) will be carried over to the next page.
                        */
                        /*tex should the present insertion be held over? */
                        int wait = 0;
                        halfword insert = node_next(page_insert_head);
                        while (insert_index(insert) != insert_index(current)) {
                            insert = node_next(insert);
                        }
                        if (split_best_insert(insert)) {
                            halfword split = split_last_insert(insert);
                            tex_try_couple_nodes(split, insert_list(current));
                            if (split_best_insert(insert) == current) {
                                /*tex
                                    Wrap up the box specified by node |r|, splitting node |p| if called
                                    for and set |wait| if node |p| holds a remainder after splitting.
                                */
                                if (node_type(insert) == split_node && node_subtype(insert) == insert_split_subtype && (split_broken_insert(insert) == current) && split_broken(insert)) {
                                    while (node_next(split) != split_broken(insert)) {
                                        split = node_next(split);
                                    }
                                    node_next(split) = null;
                                    split_top_skip_par = insert_split_top(current); /*tex The old value is saved and restored. */
                                    insert_list(current) = tex_prune_page_top(split_broken(insert), 0);
                                    if (insert_list(current)) {
                                        /*tex
                                            We only determine the total height of the list stored in
                                            the insert node.
                                         */
                                        halfword list = insert_list(current);
                                        halfword result = tex_vpack(list, 0, packing_additional, max_dimension, direction_unknown, holding_none_option, NULL);
                                        insert_total_height(current) = box_total(result);
                                        box_list(result) = null;
                                        tex_flush_node(result);
                                        wait = 1;
                                    }
                                }
                                split_best_insert(insert) = null;
                                {
                                    /*tex
                                        We need this juggling in order to also set the old school box
                                        when we're in traditional mode.
                                    */
                                    halfword index = insert_index(insert);
                                    halfword content = tex_get_insert_content(index);
                                    halfword list = box_list(content);
                                    halfword result = tex_vpack(list, 0, packing_additional, max_dimension, dir_lefttoright, holding_none_option, NULL);
                                    tex_set_insert_content(index, result);
                                    box_list(content) = null;
                                    tex_flush_node(content);
                                }
                            } else {
                                split_last_insert(insert) = tex_tail_of_node_list(split);
                            }
                        } else {
                            wait = 1;
                        }
                        /*tex
                            Either append the insertion node |p| after node |q|, and remove it from the
                            current page, or delete |node(p)|.
                        */
                        tex_try_couple_nodes(previous, node_next(current));
                        node_next(current) = null;
                        if (wait) {
                            tex_couple_nodes(lastinsert, current);
                            lastinsert = current;
                            ++lmt_page_builder_state.insert_penalties; /* todo: use a proper variable name instead */
                        } else {
                            insert_list(current) = null;
                            tex_flush_node(current);
                        }
                        current = previous;
                    }
                    break;
                case mark_node:
                    tex_update_first_and_bot_mark(current);
                    break;
            }
            previous = current;
            current = node_next(current);
        }
        split_top_skip_par = save_split_top_skip;
    }
    /*tex
        Break the current page at node |p|, put it in box~|output_box|, and put the remaining nodes
        on the contribution list.

        When the following code is executed, the current page runs from node |page_head| to node
        |prev_p|, and the nodes from |p| to |page_tail| are to be placed back at the front of
        the contribution list. Furthermore the held-over insertions appear in a list from
        |hold_head| to |q|; we will put them into the current page list for safekeeping while the
        user's output routine is active. We might have |q = hold_head|; and |p = null| if and only
        if |prev_p = page_tail|. Error messages are suppressed within |vpackage|, since the box
        might appear to be overfull or underfull simply because the stretch and shrink from the
        |\skip| registers for inserts are not actually present in the box.
    */
    if (current) {
        if (! node_next(contribute_head)) {
            contribute_tail = lmt_page_builder_state.tail;
        }
        tex_couple_nodes(lmt_page_builder_state.tail, node_next(contribute_head));
        tex_couple_nodes(contribute_head, current);
        node_next(previous) = null;
    }
    /*tex When we pack the box we inhibit error messages. */
    {
        halfword save_vbadness = vbadness_par;
        halfword save_vfuzz = vfuzz_par;
        vbadness_par = infinite_bad;
        vfuzz_par = max_dimension;
        tex_show_marks();
     // if (1) {
            box_register(output_box_par) = tex_filtered_vpack(node_next(page_head), lmt_page_builder_state.best_size, packing_exactly, lmt_page_builder_state.max_depth, output_group, dir_lefttoright, 0, 0, 0, holding_none_option, &lmt_page_builder_state.excess);
     // } else {
     //     /* maybe an option one day */
     //     box_register(output_box_par) = tex_filtered_vpack(node_next(page_head), 0, packing_additional, lmt_page_builder_state.max_depth, output_group, dir_lefttoright, 0, 0, 0, holding_none_option));
     // }
        vbadness_par = save_vbadness;
        vfuzz_par = save_vfuzz;
    }
    if (lmt_page_builder_state.last_glue != max_halfword) {
        tex_flush_node(lmt_page_builder_state.last_glue);
        lmt_page_builder_state.last_glue = max_halfword;
    }
    /*tex Start a new current page. This sets |last_glue := max_halfword|, well we already did that. */
    tex_aux_start_new_page();
    /*tex So depth is now forgotten .. hm. */
    if (lastinsert != hold_head) {
        node_next(page_head) = node_next(hold_head);
        lmt_page_builder_state.tail = lastinsert;
    }
    /*tex Delete the page-insertion nodes. */
    {
        halfword r = node_next(page_insert_head);
        while (r != page_insert_head) {
            lastinsert = node_next(r);
            tex_flush_node(r);
            r = lastinsert;
        }
    }
    node_next(page_insert_head) = page_insert_head;
    tex_update_first_marks();
    if (output_routine_par) {
        if (lmt_page_builder_state.dead_cycles >= max_dead_cycles_par) {
            /*tex Explain that too many dead cycles have occurred in a row. */
            tex_handle_error(
                normal_error_type,
                "Output loop --- %i consecutive dead cycles",
                lmt_page_builder_state.dead_cycles,
                "I've concluded that your \\output is awry; it never does a \\shipout, so I'm\n"
                "shipping \\box\\outputbox out myself. Next time increase \\maxdeadcycles if you\n"
                "want me to be more patient!"
            );
        } else {
            /*tex Fire up the users output routine and |return|. */
            lmt_page_builder_state.output_active = 1;
            ++lmt_page_builder_state.dead_cycles;
            tex_push_nest();
            cur_list.mode = internal_vmode;
            cur_list.prev_depth = ignore_depth_criterion_par;
            cur_list.mode_line = -lmt_input_state.input_line;
            tex_begin_token_list(output_routine_par, output_text);
            tex_new_save_level(output_group);
            tex_normal_paragraph(output_par_context);
            tex_scan_left_brace();
            return;
        }
    }
    /*tex
        Perform the default output routine. The list of held-over insertions, running from
        |page_head| to |page_tail|, must be moved to the contribution list when the user has
        specified no output routine.
    */

    /* todo: double link */

    if (node_next(page_head)) {
        if (node_next(contribute_head)) {
            node_next(lmt_page_builder_state.tail) = node_next(contribute_head);
        }
        else {
            contribute_tail = lmt_page_builder_state.tail;
        }
        node_next(contribute_head) = node_next(page_head);
        node_next(page_head) = null;
        lmt_page_builder_state.tail = page_head;
    }
    if (lmt_packaging_state.page_discards_head) {
        tex_flush_node_list(lmt_packaging_state.page_discards_head);
        lmt_packaging_state.page_discards_head = null;
    }
    if (box_register(output_box_par)) {
        tex_flush_node_list(box_register(output_box_par));
        box_register(output_box_par) = null;
    }
}

/*tex

    When the user's output routine finishes, it has constructed a vlist in internal vertical mode,
    and \TEX\ will do the following:

*/

void tex_resume_after_output(void)
{
    if (lmt_input_state.cur_input.loc || ((lmt_input_state.cur_input.token_type != output_text) && (lmt_input_state.cur_input.token_type != backed_up_text))) {
        /*tex Let's just be fatal here. */
        tex_fatal_error("Unbalanced output routine");
     // /*tex Recover from an unbalanced output routine */
     // tex_handle_error(
     //     normal_error_type,
     //     "Unbalanced output routine",
     //     "Your sneaky output routine has problematic {'s and/or }'s. I can't handle that\n"
     //     "very well; good luck."
     // );
     // /*tex Loops forever if reading from a file, since |null = min_halfword <= 0|. */
     // do {
     //     tex_get_token();
     // } while (lmt_input_state.cur_input.loc);
    }
    /*tex Conserve stack space in case more outputs are triggered. */
    tex_end_token_list();
    tex_end_paragraph(bottom_level_group, output_par_context); /*tex No |wrapped_up_paragraph| here. */
    tex_unsave();
    lmt_page_builder_state.output_active = 0;
    lmt_page_builder_state.insert_penalties = 0;
    /*tex Ensure that box |output_box| is empty after output. */
    if (box_register(output_box_par)) {
        if (! ((no_output_box_error_par > 2) || (no_output_box_error_par & 2))) {
            tex_handle_error(
                normal_error_type,
                "Output routine didn't use all of \\box%i", output_box_par,
                "Your \\output commands should empty \\box\\outputbox, e.g., by saying\n"
                "'\\shipout\\box\\outputbox'. Proceed; I'll discard its present contents."
            );
        }
        box_register(output_box_par) = tex_aux_delete_box_content(box_register(output_box_par), "output", 1);
    }
    if (lmt_insert_state.storing == insert_storage_delay && tex_insert_stored()) {
        if (tracing_inserts_par > 0) {
            tex_print_levels();
            tex_print_str(lmt_insert_state.head ? "<delaying inserts>" : "<no inserts to delay>");
            if (lmt_insert_state.head && tracing_inserts_par > 1) {
                tex_show_node_list(lmt_insert_state.head, max_integer, max_integer);
            }
        }
        tex_try_couple_nodes(lmt_page_builder_state.tail, lmt_insert_state.head);
        lmt_page_builder_state.tail = lmt_insert_state.tail;
        lmt_insert_state.head = null;
        lmt_insert_state.tail = null;
    }
    if (cur_list.tail != cur_list.head) {
        /*tex Current list goes after heldover insertions. */
        tex_try_couple_nodes(lmt_page_builder_state.tail, node_next(cur_list.head));
        lmt_page_builder_state.tail = cur_list.tail;
    }
    if (node_next(page_head)) {
        /* Both go before held over contributions. */
        if (! node_next(contribute_head)) {
            contribute_tail = lmt_page_builder_state.tail;
        }
        tex_try_couple_nodes(lmt_page_builder_state.tail, node_next(contribute_head));
        tex_try_couple_nodes(contribute_head, node_next(page_head));
        node_next(page_head) = null;
        lmt_page_builder_state.tail = page_head;
    }
    if (lmt_insert_state.storing == insert_storage_inject) {
        halfword h = node_next(contribute_head);
        while (h) {
            halfword n = node_next(h);
            if (node_type(h) == insert_node) {
                tex_try_couple_nodes(node_prev(h), n);
                tex_insert_restore(h);
            }
            h = n;
        }
        if (tracing_inserts_par > 0) {
            tex_print_levels();
            tex_print_str(lmt_insert_state.head ? "<storing inserts>" : "<no inserts to store>");
            if (lmt_insert_state.head && tracing_inserts_par > 1) {
                tex_show_node_list(lmt_insert_state.head, max_integer, max_integer);
            }
        }
    }
    lmt_insert_state.storing = insert_storage_ignore;
    tex_flush_node_list(lmt_packaging_state.page_discards_head);
    lmt_packaging_state.page_discards_head = null;
    tex_pop_nest();
    tex_build_page(after_output_page_context, 0);
}
