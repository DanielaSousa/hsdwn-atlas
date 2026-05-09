import networkx as nx
import pickle
from ..utils import *
from heapq import heappush, heappop
from itertools import count
from ryuController.paths import PathState, Routes
import logging
import traceback
logger2 = logging.getLogger(__name__)
logger2.setLevel(logging.WARNING)
#logging.basicConfig(level=logging.INFO)

def get_best_path(src, dst,theta,G, routes:Routes, exclude = [] ):
    assert isinstance(G,nx.classes.graph.Graph), 'Not a graph!'
    # normal dijkstra
    logger2.debug("get_best_path %s", str((src, dst)))

    try:
        #_dist, path = multi_variable_(G, source=src,routes=routes, target=dst, weight=theta, exclude=exclude)
        if False:
            _paths = list(nx.all_simple_paths(G, source=src, target=dst))
            if len(_paths)>1:
                logger2.info(" [PATHS] Multiple paths %s", _paths)
            else:
                logger2.warning(" [PATHS] One or less than one path! %s", _paths)
        _dist, path = multi_variable_v2(G, source=src, target=dst, r=routes, weight=theta,  exclude = exclude)
    except (nx.exception.NetworkXError, nx.NetworkXNoPath) as err:
        logger2.info(err)
        return -1, [src]
    except Exception as err:  # nx.NetworkXNoPath
        logger2.error(err)
        traceback.print_exc()
        exit(-1)
    return _dist, path

def get_RI_4_node(r:set, u:int, neigh:list)->int:
    '''
    Args:
        r: set with the nodes being used by other routes
        u: node
        neigh: list of neighbours of u
    Returns: number of interfering nodes
    '''
    assert isinstance(neigh, list), 'Argument of wrong type!'
    assert isinstance(u, int), 'Argument of wrong type!'
    assert isinstance(r, set), 'Argument of wrong type!'
    #print(neigh)
    sum = 0
    if len(r) == 0:
        return 0
    for i in neigh+[u]:
        if i in r:
            sum+=1

    return sum

def get_tech_cost(t):
    return t/10


def multi_variable_v2(G, source, r: Routes, weight=[0.0, 0.0, 1.0],  pred=None, cutoff=None,
                      target=None, exclude=[]):
    """
    Find shortest weighted paths and lengths from a source node.

    Compute the shortest path length between source and all other
    reachable nodes for a weighted graph.

    Uses modified Dijkstra's algorithm to compute shortest paths and lengths
    between a source and all other reachable nodes in a weighted graph.

    :param G: NetworkX graph
    :param source: Starting node for path
    :param target: Ending node for path
    :return:
    """

    #prepare flows
    r_set= set()
    for k,l in r.flows.items():
        for p in l:
            if p.state== PathState.BROKEN:
                break
            else:
                r_set.update(p.path)

    # print('source -> target', source, target, G.nodes(), G.edges())
    sources = [source]

    # weight = _weight_function(G, weight)
    def weight_func(v, u, atr):
        logger2.debug("PathCalc: Edges (%d, %d)", v, u)
        return get_edge_cost_v2(weight, atr)  # d = edges[u][v]

    paths = {source: [source] for source in sources}  # dictionary of paths

    G_succ = G._succ if G.is_directed() else G._adj

    push = heappush
    pop = heappop
    dist = {}  # dictionary of final distances
    seen = {}
    # CHANGE:--> fringe is heapq with (distance,c,node, variables)
    # use the count c to avoid comparing nodes (may not be able to)
    c = count()
    fringe = []
    if source not in G:
        raise nx.NodeNotFound(f"Source {source} not in G")
    if target not in G:
        raise nx.NodeNotFound(f"Source {target} not in G")
    if G.has_edge(source, target):
        logger2.debug("Direct Path!")
        return (0, paths[source] + [target])  # direct path | improvement #1

    seen[source] = 0
    init = {'link_stability': -1,'break_prob': 0, 'max_edges': 0, 'AvBw': -1, 'TechCost': 0, 'RI':0}
    push(fringe, (0, next(c), source, init))

    # print graph
    logger2.debug(" \n\t all graph")
    for nodes in G.nodes():
        logger2.debug("%s", str ((nodes, G.nodes[nodes])))
    for e in G.edges():
        logger2.debug("%s", str ((e, G.edges[e])))

    logger2.debug(" \n\t end all graph")

    maxTTL = 30
    while fringe:
        path_n = {}
        (d, _, v, var) = pop(fringe)
        if v in dist:
            continue  # already searched this node.
        dist[v] = d  # (distance, variables)
        path_n_1 = var
        if v == target:
            break
        for u, e in G_succ[v].items():
            if u in exclude:
                continue
            # ---- find cost ----
            logger2.debug("edge: %d -> %d", v, u)
            logger2.debug("attr u %s", str(G.nodes[u]))
            logger2.debug("attr v %s", str(G.nodes[v]))

            if G.nodes[u]['datapath_id'] == G.nodes[v]['datapath_id']: #is same sdn node
                path_n = path_n_1
            else:
                # ticks
                l_stab_e = min(e['link_stability'], 60) / 60  # maxTicks = 60 sec
                path_n['link_stability'] = max(path_n_1['link_stability'], l_stab_e * (-1))

                # P_break
                path_n['break_prob'] = max(path_n_1['break_prob'], get_break_probability(
                    norm(G.nodes[u]['energy'], 0, 100), e['snr']))

                path_n['max_edges'] = path_n_1['max_edges'] + 1 / maxTTL

                # -------------------------
                # print(G.nodes[u]['avbw'])
                assert G.nodes[u]['avbw'] >= 0, "This node %d has negative avbw of %d".format(u, G.nodes[u]['avbw'])
                path_n['AvBw'] = max(path_n_1['AvBw'] , G.nodes[u]['avbw']*(-1)/100000000 ) # TODO falta normalizar
                path_n['RI'] = path_n_1['RI'] + get_RI_4_node(r_set, u, list(G.neighbors(u)))/100 # TODO ver este valor
                path_n['TechCost'] = path_n_1['TechCost'] + get_tech_cost(G.nodes[u]['type'])/maxTTL # maxTTL hops # normalised by maxTTL

                logger2.info("%s", str((path_n['AvBw'] * (-1), path_n['RI'], path_n['TechCost'])))
            # ------------------------------
            vu_dist = weight_func(v, u, path_n)

            if cutoff is not None:
                if vu_dist > cutoff:
                    continue
            if u in dist:
                u_dist = dist[u]
                if vu_dist < u_dist:
                    pickle.dump((G, source, exclude , weight, pred, cutoff, target), open(
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

    # dist[target], paths[target]
    logger2.debug("Get Path results %s", str((dist, paths)))

    if target is None:
        return (dist, paths)
    try:
        return (dist[target], paths[target])
    except KeyError as err:
        raise nx.NetworkXNoPath(f"No path to {target}.") from err

# -----------------------------------------------------------------
def multi_variable_Dn(G, source, routes, weight=[0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0], pred=None, cutoff=None,
                      target=None, exclude=[]):
    """
    Find shortest weighted paths and lengths from a source node.

    Compute the shortest path length between source and all other
    reachable nodes for a weighted graph.

    Uses modified Dijkstra's algorithm to compute shortest paths and lengths
    between a source and all other reachable nodes in a weighted graph.

    :param G: NetworkX graph
    :param source: Starting node for path
    :param target: Ending node for path
    :return:
    """

    # find the sum of unique routes in the graph
    # {dpId: {flow_key: (next_mac, out_port) } }
    u_sum = set()
    for k in routes.keys():
        # print(routes[k])
        for key, r in routes[k].items():
            u_sum.add(key)

    sum_routes = len(u_sum)

    # ----------------------------------------

    # print('source -> target', source, target, G.nodes(), G.edges())
    sources = [source]

    # weight = _weight_function(G, weight)
    def weight_func(v, u, atr):
        return get_edge_cost(
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
            'break_prob': 0, 'max_edges': 0, 'n_routes': 0, 'unique_routes': set()}
    push(fringe, (0, next(c), source, init))

    n = G.number_of_nodes()
    maxTTL = 15
    # print("graph", n, G.number_of_edges())
    while fringe:
        path_n = {}
        (d, _, v, var) = pop(fringe)
        if v in dist:
            continue  # already searched this node.
        dist[v] = d  # (distance, variables)
        # print(v, "-->", var, d)
        # print('v', v, '->', d)
        path_n_1 = var
        if v == target:
            break
        # print('G_succ[v].items()', len(G_succ[v].items()), G_succ[v].items())
        for u, e in G_succ[v].items():
            # print(v, '-->', u, 'link cost:', e)
            if v in exclude:
                continue
            # ---- find cost ----

            # cost = weight_func(v, u, e)
            # if cost is None:
            # 	continue
            # vu_dist = dist[v] + cost

            path_n['delivery'] = max(path_n_1['delivery'], e['delivery'] * (-1))
            path_n['delay'] = path_n_1['delay'] + e['delay']
            path_n['throughput'] = max(path_n_1['throughput'], e['throughput'] * (-1))

            # ticks
            l_stab_e = min(e['link_stability'], 60) / 60  # maxTicks = 60 sec
            path_n['link_stability'] = max(path_n_1['link_stability'], l_stab_e * (-1))

            # P_break
            path_n['break_prob'] = max(path_n_1['break_prob'], get_break_probability(
                norm(G.nodes[u]['energy'], 0, 100), e['snr']))

            path_n['max_edges'] = path_n_1['max_edges'] + 1 / maxTTL

            # routes bug
            path_n['unique_routes'] = path_n_1['unique_routes']

            if routes.get(G.nodes[u]['datapath_id']) is not None:
                for key, r in routes.get(G.nodes[u]['datapath_id']).items():
                    path_n['unique_routes'].add(key)  # match field

            assert len(path_n['unique_routes']) <= sum_routes, ' affected routes higher than total routes!'
            if sum_routes == 0:
                path_n['n_routes'] = 0
            else:
                path_n['n_routes'] = len(path_n['unique_routes']) / sum_routes

            vu_dist = weight_func(v, u, path_n)
            # print(v, '->', u, ':', vu_dist, path_n )

            # print(v, '->', u, vu_dist)

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
            # print(paths)

    # The optional predecessor and path dictionaries can be accessed
    # by the caller via the pred and paths objects passed as arguments.
    # return dist
    # dist[target], paths[target]
    print("HELLO", paths[target])

    if target is None:
        return (dist, paths)
    try:
        # print(paths[target])
        return (dist[target], paths[target])
    except KeyError as err:
        raise nx.NetworkXNoPath(f"No path to {target}.") from err
