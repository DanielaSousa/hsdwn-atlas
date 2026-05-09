import pickle
from heapq import heappush, heappop
from itertools import count
import networkx as nx

from networkx.algorithms.shortest_paths.weighted import _weight_function
from ryuController.Path_Calculator.tmp import read_weights, DATA, weights, norm

from ryuController.Path_Calculator.performance import Performance
perf = Performance('performance.csv')


def get_break_probability(energy, snr):
    assert energy <= 1 and snr <= 1, 'Values are not normalized!'
    # in case a link is not yet erased from olsr tables, but no packet was received in the meantime

    # Quanto maior melhor, maior valores de snr, menor sera a probabilidade de break!!
    if snr == 0:
        return 1

    min_val = 0.05
    max_val = 0.50
    # function
    break_prob = 1
    if energy < min_val:
        break_prob = 1
    elif energy > max_val:
        # The range of SNR may vary between 1dB and 30dB. The optimum SNR range is 18-30dB
        break_prob = 1 - snr
    else:
        #print('50/50')
        break_prob = 1 - (0.5 * energy + 0.5 * snr)

    return break_prob


def get_path_cost(G, w, v, u, attr):
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


def get_link_cost(G, w, nodeA, nodeB, edge, n_routes):
    """
    get the link weight for a specific link
    :param w [7]: the weights specific for this type of service
    :param nodeA: the edge source
    :param nodeB: the edge target
    :param G: the networkx graph
    :return: value
    """
    # TODO ver isto de acordo com a equacao
    assert len(w) == 7, 'Not enough weights!'
    assert all(0 <= i <= 1 for i in w), 'weights outside range [0,1]!'
    assert 0.95 <= sum(w) <= 1.05, 'Sum not equal to 1!'

    # edge = G.edges[nodeA, nodeB]
    # --- break probability ----
    # variables
    energy = norm(G.nodes[nodeB]['energy'], 0, 100)  # percentage [0, 100%]
    break_prob = get_break_probability(energy, edge['snr'])

    # --- other variables ----
    # there is no way to say how long the link can be up.
    link_stab = norm(edge['link_stability'], 0, 600)*(-1)
    # Therefore, 10 min is used, because no simulation time with more than 600seconds is utilized from now on!

    throughput = edge['throughput']*(-1)  # 1Gbs - (Mbps)
    delivery = edge['delivery']*(-1)
    delay = edge['delay']  # (seconds)

    n = G.number_of_nodes()
    maxEdges = (n*(n - 1) / 2)

    val = w[0] * (1+delivery) + w[1] * delay + w[2] * (1+throughput) + w[3] * (1+link_stab) + w[4] * break_prob + \
        w[5] * (1/maxEdges) + w[6] * n_routes  # [0,1]

    return val


def multi_variable_Dn(G, source, routes, weight=[0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0], pred=None, cutoff=None, target=None):
    """
    Find shortest weighted paths and lengths from a source node.

    Compute the shortest path length between source and all other
    reachable nodes for a weighted graph.

    Uses modified Dijkstra's algorithm to compute shortest paths and lengths
    between a source and all other reachable nodes in a weighted graph.

    :param G: NetworkX graph
    :param source: Starting node for path
    :param target: Ending node for path
    :return: [0.25, 0.0, 0.25, 0.0, 0.25, 0.25, 0.0]
    [0.142, 0.142, 0.142, 0.142, 0.142, 0.142, 0.142]
    [0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0]
    [0.50, 0.50, 0.0, 0.0, 0.0, 0.0, 0.0] - theta1
    [0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0] - theta2

    """

    # find the sum of unique routes in the graph
    # self.routes  ={dpId: {dst: (next_mac, out_port, match)}} --> match = (src,dst,service)
    u_sum = set()
    for k in routes.keys():
        #print(routes[k])
        for key, r in routes[k].items():
            u_sum.add(r[2])
    

    sum_routes = len(u_sum)

    # ----------------------------------------
    perf.start_sample()
    # print('source -> target', source, target, G.nodes(), G.edges())
    sources = [source]
    # weight = _weight_function(G, weight)
    def weight_func(v, u, atr): return get_path_cost(
        G, weight, v, u, atr)  # d = edges[u][v]
    paths = {source: [source] for source in sources}  # dictionary of paths

    # dist = _dijkstra_multisource(
    # 	G, sources, weight, paths=paths, cutoff=cutoff, target=target
    # )
    # _dijkstra_multisource (G, sources, weight, pred=None, paths=None, cutoff=None, target=None)
    G_succ = G._succ if G.is_directed() else G._adj

    push = heappush
    pop = heappop
    dist = {}  # dictionary of final distances
    seen = {}
    # CHANGE:--> fringe is heapq with (distance,c,node, variables)
    # use the count c to avoid comparing nodes (may not be able to)
    c = count()
    fringe = []
    # for source in sources:
    if source not in G:
        raise nx.NodeNotFound(f"Source {source} not in G")
    if target not in G:
        raise nx.NodeNotFound(f"Source {target} not in G")
    if G.has_edge(source, target):
        return (None, paths[source] + [target])  # direct path | improvement #1

    seen[source] = 0
    init = {'delivery': -1, 'delay': 0, 'throughput': -1, 'link_stability': -1,
                            'break_prob': 0, 'max_edges': 0, 'n_routes': 0, 'unique_routes':set() }
    push(fringe, (0, next(c), source, init))

    n = G.number_of_nodes()
    maxTTL = 15
    #print("graph", n, G.number_of_edges())
    while fringe:
        path_n = {}
        (d, _, v, var) = pop(fringe)
        if v in dist:
            continue  # already searched this node.
        dist[v] = d  # (distance, variables)
        #print(v, "-->", var, d)
        # print('v', v, '->', d)
        path_n_1 = var
        if v == target:
            break
        # print('G_succ[v].items()', len(G_succ[v].items()), G_succ[v].items())
        for u, e in G_succ[v].items():
            # print(v, '-->', u, 'link cost:', e)
            # ---- find cost ----

            # cost = weight_func(v, u, e)
            # if cost is None:
            # 	continue
            # vu_dist = dist[v] + cost

            path_n['delivery'] = max(path_n_1['delivery'], e['delivery']*(-1))
            path_n['delay'] = path_n_1['delay'] + e['delay']
            path_n['throughput'] = max( path_n_1['throughput'], e['throughput']*(-1))
            
            # ticks
            l_stab_e = min(e['link_stability'], 60) / 60 # maxTicks = 60 sec
            path_n['link_stability'] = max(path_n_1['link_stability'], l_stab_e*(-1))
            
            # P_break
            path_n['break_prob'] = max(path_n_1['break_prob'], get_break_probability(
                norm(G.nodes[u]['energy'], 0, 100), e['snr']))
            
            path_n['max_edges'] = path_n_1['max_edges'] + 1/maxTTL

            # routes bug
            path_n['unique_routes'] = path_n_1['unique_routes']
            
            if routes.get(G.nodes[u]['datapath_id']) is not None:
                for key, r in routes.get(G.nodes[u]['datapath_id']).items():
                    path_n['unique_routes'].add( r[2] ) # match field

            
            assert len(path_n['unique_routes']) <= sum_routes, ' affected routes higher than total routes!'
            if sum_routes == 0:
                path_n['n_routes']  = 0
            else:
                path_n['n_routes'] = len(path_n['unique_routes'])/ sum_routes

            vu_dist = weight_func(v, u, path_n)
            # print(v, '->', u, ':', vu_dist, path_n )

            #print(v, '->', u, vu_dist)


            if cutoff is not None:
                if vu_dist > cutoff:
                    continue
            if u in dist:
                u_dist = dist[u]
                if vu_dist < u_dist:
                    # print(v, '->', u, path_n_1, path_n, u_dist, vu_dist)

                    pickle.dump((G, source, routes, weight, pred, cutoff, target), open(
                        'graph-debug.pickle', 'wb'))
                    raise ValueError(
                        "Contradictory paths found:", "negative weights?")
                elif pred is not None and vu_dist == u_dist:
                    pred[u].append(v)
            elif u not in seen or vu_dist < seen[u]:
                seen[u] = vu_dist

                push(fringe, (vu_dist, next(c), u, path_n.copy()))
                if paths is not None:
                    paths[u] = paths[v] + [u]
                if pred is not None:
                    pred[u] = [v]
            elif vu_dist == seen[u]:
                if pred is not None:
                    pred[u].append(v)
            #print(paths)

    # The optional predecessor and path dictionaries can be accessed
    # by the caller via the pred and paths objects passed as arguments.
    # return dist
    # dist[target], paths[target]
    print("HELLO", paths[target])
    perf.end_sample(len(G.nodes()))
    if target is None:
        return (dist, paths)
    try:
        # print(paths[target])
        return (dist[target], paths[target])
    except KeyError as err:
        raise nx.NetworkXNoPath(f"No path to {target}.") from err



def multi_variable_djikstra(G, source, routes, weight=[0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0], pred=None, cutoff=None, target=None):
    """

    :param G:
    :param source:
    :param weight: list of weights for each variable of the matrix
    :param pred:
    :param cutoff:
    :param target:
    :return:
    """
    perf.start_sample()
    FIXED_COST = 10
    sources = [source]
    # weight = _weight_function(G, weight)
    def weight_func(v, u, d): return get_link_cost(
        G, weight, v, u, d, len(routes.get(u, [])))  # d = edges[u][v]
    paths = {source: [source] for source in sources}  # dictionary of paths

    # dist = _dijkstra_multisource(
    # 	G, sources, weight, paths=paths, cutoff=cutoff, target=target
    # )
    # _dijkstra_multisource (G, sources, weight, pred=None, paths=None, cutoff=None, target=None)
    G_succ = G._succ if G.is_directed() else G._adj

    push = heappush
    pop = heappop
    dist = {}  # dictionary of final distances
    seen = {}
    # fringe is heapq with 3-tuples (distance,c,node)
    # use the count c to avoid comparing nodes (may not be able to)
    c = count()
    fringe = []
    # for source in sources:
    if source not in G:
        raise nx.NodeNotFound(f"Source {source} not in G")
    seen[source] = 0
    push(fringe, (0, next(c), source))
    while fringe:
        (d, _, v) = pop(fringe)
        if v in dist:
            continue  # already searched this node.
        dist[v] = d
        #print('v', v, '->', d)
        if v == target:
            break
        for u, e in G_succ[v].items():
            cost = weight_func(v, u, e)
            # print('v->u', u, '->', cost)
            if cost is None:
                continue
            vu_dist = dist[v] + cost + FIXED_COST
            if cutoff is not None:
                if vu_dist > cutoff:
                    continue
            if u in dist:
                u_dist = dist[u]
                # print(vu_dist, u_dist,vu_dist < u_dist )
                if vu_dist < u_dist:
                    # print(vu_dist, u_dist)
                    #print(v, '->', u, path_n_1, path_n)
                    raise ValueError(
                        "Contradictory paths found:", "negative weights?")
                elif pred is not None and vu_dist == u_dist:
                    pred[u].append(v)
            elif u not in seen or vu_dist < seen[u]:
                seen[u] = vu_dist
                push(fringe, (vu_dist, next(c), u))
                if paths is not None:
                    paths[u] = paths[v] + [u]
                if pred is not None:
                    pred[u] = [v]
            elif vu_dist == seen[u]:
                if pred is not None:
                    pred[u].append(v)

    # The optional predecessor and path dictionaries can be accessed
    # by the caller via the pred and paths objects passed as arguments.
    # return dist
    # dist[target], paths[target]
    perf.end_sample(len(G.nodes()))
    if target is None:
        return (dist, paths)
    try:
        return (dist[target], paths[target])
    except KeyError as err:
        raise nx.NetworkXNoPath(f"No path to {target}.") from err


def djikstra(G, source, weight="weight", pred=None, cutoff=None, target=None):
    """
    original djisktra algorithm from networkX
    :param G:
    :param source:
    :param weight: string or function
            If this is a string, then edge weights will be accessed via the
    edge attribute with this key (that is, the weight of the edge
    joining `u` to `v` will be ``G.edges[u, v][weight]``). If no
    such edge attribute exists, the weight of the edge is assumed to
    be one.
    :param pred: None
    :param cutoff: integer or float, optional
    :param target: node label, optional
    :return:

    Notes
-----
Edge weight attributes must be numerical.
Distances are calculated as sums of weighted edges traversed.

The weight function can be used to hide edges by returning None.
So ``weight = lambda u, v, d: 1 if d['color']=="red" else None``
will find the shortest red path.

Based on the Python cookbook recipe (119466) at
https://code.activestate.com/recipes/119466/

This algorithm is not guaranteed to work if edge weights
are negative or are floating point numbers
(overflows and roundoff errors can cause problems).
    """
    perf.start_sample()
    sources = [source]
    weight = _weight_function(G, weight)
    paths = {source: [source] for source in sources}  # dictionary of paths

    # dist = _dijkstra_multisource(
    # 	G, sources, weight, paths=paths, cutoff=cutoff, target=target
    # )
    # _dijkstra_multisource (G, sources, weight, pred=None, paths=None, cutoff=None, target=None)
    G_succ = G._succ if G.is_directed() else G._adj

    push = heappush
    pop = heappop
    dist = {}  # dictionary of final distances
    seen = {}
    # fringe is heapq with 3-tuples (distance,c,node)
    # use the count c to avoid comparing nodes (may not be able to)
    c = count()
    fringe = []
    # for source in sources:
    if source not in G:
        raise nx.NodeNotFound(f"Source {source} not in G")
    seen[source] = 0
    push(fringe, (0, next(c), source))
    while fringe:
        (d, _, v) = pop(fringe)
        if v in dist:
            continue  # already searched this node.
        dist[v] = d
        if v == target:
            break
        for u, e in G_succ[v].items():
            cost = weight(v, u, e)
            if cost is None:
                continue
            vu_dist = dist[v] + cost
            if cutoff is not None:
                if vu_dist > cutoff:
                    continue
            if u in dist:
                u_dist = dist[u]
                if vu_dist < u_dist:
                    raise ValueError(
                        "Contradictory paths found:", "negative weights?")
                elif pred is not None and vu_dist == u_dist:
                    pred[u].append(v)
            elif u not in seen or vu_dist < seen[u]:
                seen[u] = vu_dist
                push(fringe, (vu_dist, next(c), u))
                if paths is not None:
                    paths[u] = paths[v] + [u]
                if pred is not None:
                    pred[u] = [v]
            elif vu_dist == seen[u]:
                if pred is not None:
                    pred[u].append(v)

    # The optional predecessor and path dictionaries can be accessed
    # by the caller via the pred and paths objects passed as arguments.
    # return dist
    # dist[target], paths[target]
    perf.end_sample(len(G.nodes()))
    if target is None:
        return (dist, paths)
    try:
        return (dist[target], paths[target])
    except KeyError as err:
        raise nx.NetworkXNoPath(f"No path to {target}.") from err


if __name__ == "__main__":
    weights[DATA] = read_weights(DATA)
    G = nx.Graph()
    # node 1 -- node 2 -- node 3
    # |
    # 		 -- node 4 --
    attr = {'datapath_id': 0, 'port_no': 1, 'hw_addr': 1, 'ipv4_addr': '10.1.1.1',
            'vertex_id': 0, 'isValid': False, 'isOF': False, 'energy': 1}
    G.add_node(0, **attr)
    attr = {'datapath_id': 1, 'port_no': 1, 'hw_addr': 1, 'ipv4_addr': '10.1.1.2',
            'vertex_id': 1, 'isValid': False, 'isOF': False, 'energy': 1}
    G.add_node(1, **attr)
    attr = {'datapath_id': 2, 'port_no': 1, 'hw_addr': 1, 'ipv4_addr': '10.1.1.3',
            'vertex_id': 2, 'isValid': False, 'isOF': False, 'energy': 1}
    G.add_node(2, **attr)
    attr = {'datapath_id': 3, 'port_no': 1, 'hw_addr': 1, 'ipv4_addr': '10.1.1.4',
            'vertex_id': 3, 'isValid': False, 'isOF': False, 'energy': 1}
    G.add_node(3, **attr)
    G.add_edge(0, 1, **{'delivery': 0.10, 'delay': 0.8,
               'throughput': 0.5, 'link_stability': 5, 'snr': 0.6})
    G.add_edge(1, 2, **{'delivery': 0.30, 'delay': 0.5,
               'throughput': 0.5, 'link_stability': 5, 'snr': 0.6})

    G.add_edge(0, 3, **{'delivery': 1, 'delay': 0.1,
               'throughput': 1, 'link_stability': 600, 'snr': 0.6})
    G.add_edge(3, 2, **{'delivery': 0.50, 'delay': 0.8,
               'throughput': 0.2, 'link_stability': 6, 'snr': 0.6})
    G.add_edge(3, 1, **{'delivery': 0.50, 'delay': 0.8,
               'throughput': 0.2, 'link_stability': 6, 'snr': 0.6})

    routes = {0: {2: (None)}, 1: {2: (None)}}
    # _dist, path = djikstra(G, 0, target=2)
    # _dist, path = multi_variable_djikstra(G, 0, routes,weights[DATA], target=2)
    _dist, path = multi_variable_Dn(G, 0, routes, weights[DATA], target=2)
    print([G.nodes[n]['ipv4_addr'] for n in path])
