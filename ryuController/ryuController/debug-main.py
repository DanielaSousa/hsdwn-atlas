import pickle
import networkx as nx


# out of order
def get_path_cost(w, attr):
    assert len(w) == 7, 'Not enough weights!'
    assert all(0 <= i <= 1 for i in w), 'weights outside range [0,1]!'
    assert 0.95 <= sum(w) <= 1.05, 'Sum not equal to 1!'

    assert attr['delivery'] <= 0
    assert attr['throughput'] <= 0
    assert attr['link_stability'] <= 0

    assert all(0 <= abs(i) <= 1 for key, i in attr.items() if key != 'unique_routes'), 'attributes outside range [0,1]!'

    return w[0] * (1+attr['delivery']) + \
        w[1] * attr['delay'] + \
            w[2] * (1+attr['throughput']) + \
                w[3] * (1 + attr['link_stability'])+ \
                    w[4] * attr['break_prob'] + \
                        w[5] *  attr['max_edges'] + \
                            w[6] * attr['n_routes']


weights = [0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0]  # Hops


def get_link_cost(p, G):
    # cost = 0
    # for i in range(0, len(p)-1):
    #     attr = G.edges[p[i], p[i+1]]
    #     print(attr)
    #     cost += get_path_cost(weights, attr)

    # return cost
    # TODO
    return len(p)-1  # hops


def get_edge_rule(p, G):
    src = p[0]
    me = G.nodes[src]  # path[0]
    next = G.nodes[p[1]]
    port_out = me['port_no']  # default
    # print(path)

    # sw_id, netx_hw_addr, port_out
    #print(src, '->', me['datapath_id'])

    return (me['datapath_id'], next['hw_addr'], port_out)


if __name__ == "__main__":
    # read pickle
    (G, src, dst) = pickle.load(
        open("/home/phd/Documents/PhD/ns-3-dev/graph-debug.pickle", "rb"))
    for e in G.nodes():
        print(e,  G.nodes[e]['ipv4_addr'])
    #print(G.edges())


    # disjoint percentage in the path : 0,20 of nodes can be the same
    # k best paths (max)

    paths = list(nx.all_simple_paths(G, 1, 6))
    print(paths)
    paths = paths[:-1]  # just to test

    paths.sort(key=lambda e: get_link_cost(e, G))
    print(paths)

    # eval = [get_link_cost(p, G) for p in paths]
    # print(eval)
    # print(eval.index(min(eval)), min(eval))
    # p0_index = eval.index(min(eval))
    # p0 = paths[p0_index]
    # # remove p0 from list
    # del paths[p0_index]
    # del eval[p0_index]
    p0 = paths[0]
    paths = paths[1:]

    # find p0, then the rest has to be disjoint to this one
    # get disjoint paths

    # theorem : order path by cost, and then chose the first k paths that correspond to the max jointed percentage

    k = 3
    # k = 1  # p0
    final = [p0]
    # while (k):
    #     eval = []
    #     # for p in paths:
    #     #     eval += [len(set(p0[1:-1]) and set(p[1:-1])) /
    #     #              float(len(set(p0[1:-1]) or set(p[1:-1]))) * 100]
    #     # index = eval.index(min(eval))
    #     # final += [paths[p0_index]]
    #     p = paths[0]
    #     eval = len(set(p0[1:-1]) and set(p[1:-1])) / \
    #         float(len(set(p0[1:-1]) or set(p[1:-1]))) * 100

    #     if (eval <= 100):
    #         final += [p]
    #     paths = paths[1:]
    #     k -= 1
    # print(final)

    disj = []
    disj_perc = 0.7

    # disjoint edges
    l0 = []
    p = p0
    for i in range(0, len(p)-1):
        l0 += [(p[i], p[i+1])]
    print(l0)
    disj += l0

    for p in paths:
        count = 0
        l0 = []
        for i in range(0, len(p)-1):
            l0 += [(p[i], p[i+1])]
            if ((p[i], p[i+1]) in disj):

                count += 1
        disj += l0
        eval = count/min(len(disj), len(p)-1)
        print("EVAL", eval)
        if (eval <= disj_perc):
            final += [p]
            k -= 1
        if (k == 0):
            break

    print(final)
    assert len(final <= k)
    paths = final
    if paths == []:
        print("empty paths")
        print("")

    f_str = ""
    # serialize to string
    for p in final:
        if len(p) < 2:
            print("NO path found!")
            print(None, None)

        for i in range(0, len(p)-1):
            (sw_id, netx_hw_addr, port_out) = get_edge_rule(p[i:], G)
            f_str += "{} {} {},".format(sw_id, netx_hw_addr, port_out)
        # f_str = f_str[:-1]  # remove last comma
        f_str += "|"

    print(f_str)
