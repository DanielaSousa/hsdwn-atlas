import logging
logger = logging.getLogger()
#logger.setLevel(logging.WARNING)
maxTTL = 15


MAX_COST = 3
simClok = 0.0 # seconds
def addTic(t = 0.000001): # TODO c++
    global simClok
    simClok+=t

def get_P_Load(v, G, paths):
    total_load = 0
    load = {}
    for key in paths.keys():
        p = paths[key][0].path
        if v in p:
            index = p.index(v)
            Dr = key.og_bw
            # rcv
            if index != 0:
                if (p[index-1],v) not in load.keys():
                    load[(p[index - 1], v)] = Dr
                else:
                    load [(p[index-1],v)] +=Dr
            #sent
            if index != len(p)-1 :
                if (v, p[index + 1]) not in load.keys():
                    load[(v, p[index + 1])] = Dr
                else:
                    load[(v, p[index + 1])] += Dr
    for (u,v), ele in load.keys():
        total_load += max(ele, G[u][v]['throughput'])

    return total_load



def get_path_cost_v2(p, G, weight, routes, techCost):
    """
    Get the cost of a path
    Args:
        p: path
        G: graph
        weight: service weights
        routes: routes
        techCost: dictionary of costs

    Returns: cost
    """
    assert len(p) >= 2, "path does not exists"

    path_n_1 = {'link_stability': -1,'break_prob': 0, 'max_edges': 0, 'P_Load': 1, 'TechCost': 1}
    path_n = {}

    for i in range(0, len(p)-1):
        e = G.edges[p[i], p[i+1]]
        u = p[i+1]

        # ticks
        l_stab_e = min(e['link_stability'], 60) / 60  # maxTicks = 60 sec
        path_n['link_stability'] = max(path_n_1['link_stability'], l_stab_e * (-1))

        # P_break
        path_n['break_prob'] = max(path_n_1['break_prob'], get_break_probability(
            norm(G.nodes[u]['energy'], 0, 100), e['snr']))

        path_n['max_edges'] = path_n_1['max_edges'] + 1 / maxTTL

        # TODO path states
        path_n['P_Load'] = 0  # max
        path_n['TechCost'] = 0  # mean

        path_n_1 = path_n

    cost = get_edge_cost_v2(G, weight, p[0], p[-1], path_n, routes)  # G, w, v, u, attr
    return cost


def norm(value, in_min=0, in_max=100):
    return (value-in_min)/(in_max - in_min)

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

def get_edge_cost_v2( w, attr):
    assert len(w) >= 3, 'Not enough weights!'
    # logger.debug('W: %s \n attr %s', str(w), str(attr))
    assert all(0 <= i <= 1 for i in w), 'weights outside range [0,1]!'
    assert 0.95 <= sum(w) <= 1.05, 'Sum not equal to 1!'



    assert attr['max_edges'] <= 1 , ' max_edges not complient to [0,1]'
    assert attr['break_prob'] <= 1 , 'break_prob not complient to [0,1]'
    assert attr['link_stability'] <= 0 , 'link_stability not complient to [0,1]'
    assert attr['TechCost'] <= 1, 'TechCost not complient to [0,1]'

    P_state = w[0] * (1 + attr['link_stability'])+ \
                    w[1] * attr['break_prob'] + \
                        w[2] *  attr['max_edges']

    # ---------- P_Load --------
    gamma = 0.5
    P_load = gamma*attr['RI'] + (1.0-gamma)*(1.0+attr['AvBw'] )
    #assert attr['P_Load'] <= 1, 'not complient to [0,1]'


    #logger.debug("Load: %f , Tech: %f , State: %f", P_load, attr['TechCost'], P_state )
    assert P_state is not None, 'P_state is None'
    assert P_load is not None, 'P_load is None'
    assert attr['TechCost'] is not None, 'TechCost is None'
    return P_state + P_load + attr['TechCost']


# --------------------------------------
# ---------------------------------------

def get_path_cost(p, G, weight):
    """

    Args:
        p: path
        G: graph
        routes: {dpId: {flow_key: [(next_mac, out_port)] } } --> flow_key = (src,dst,service)
        weight: service weights

    Returns: cost
    """
    assert len(p) >= 2, "path does not exists"

    path_n_1 = {'delivery': -1, 'delay': 0, 'throughput': -1, 'link_stability': -1,
                'break_prob': 0, 'max_edges': 0, 'n_routes': 0,  'unique_routes':set()}
    path_n = {}
    u_sum = set()
    for k in routes.keys():
        # print(routes[k])
        for key, r in routes[k].items(): # key= dst, r = (next_mac, out_port, match)
            u_sum.add(key)  # flow_keys

    sum_routes = sum([x[2] for x in u_sum]) # sum(DataRate) PS:Service value == DataRate

    for i in range(0, len(p)-1):
        e = G.edges[p[i], p[i+1]]
        u = p[i+1]
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

        path_n['unique_routes'] = path_n_1['unique_routes']
        if routes.get(G.nodes[u]['datapath_id']) is not None:
            for key, r in routes.get(G.nodes[u]['datapath_id']).items():
                path_n['unique_routes'].add(key)  # flow_key

        assert len(path_n['unique_routes']) <= len(u_sum), ' affected routes higher than total routes!'
        if sum_routes == 0:
            path_n['n_routes'] = 0
        else:
            path_n['n_routes'] = sum([x[2] for x in path_n['unique_routes']]) / sum_routes # Number of routes and their DataRate

        path_n_1 = path_n

    cost = get_edge_cost(G, weight, p[0], p[-1], path_n)  # G, w, v, u, attr

    return cost

def get_edge_cost(G, w, v, u, attr):
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

