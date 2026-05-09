# net view
weights = {}
# weights[0]=sys.argv[1::]

# types of services
DATA = 0
VIDEO = 2
VOICE = 1


def read_weights(type):
    ryu_config_file = "~/Desktop/PhD/RYU/ryu_config.txt"
    try:
        with open(ryu_config_file) as f:
            w = f.readline().strip().split(",")
            assert len(w) == 7, "Not enough weights!"
            # TODO turn weights to int
    except:
        print("Accepting default values: 0.142")
        w = [0.142, 0.142, 0.142, 0.142, 0.142, 0.142, 0.142]

    return w


def norm(value, in_min=0, in_max=100):
    return (value-in_min)/(in_max - in_min)


def get_link_value(G, w, nodeA, nodeB, edge, n_routes):
    """
    get the link weight for a specific link
    :param w [5]: the weights specific for this type of service
    :param nodeA: the edge source
    :param nodeB: the edge target
    :param G: the networkx graph
    :return: value
    """
    # TODO ver isto de acordo com a equaçao
    assert len(w) == 7, 'Not enough weights!'
    assert all(0 <= i <= 1 for i in w), 'weights outside range [0,1]!'
    assert sum(w) == 1, 'Sum not equal to 1!'

    # edge = G.edges[nodeA, nodeB]
    # --- break probability ----
    # variables
    energy = G.nodes[nodeB]['energy']  # percentage [0, 100%]
    min_val = 5
    snr = edge['snr']
    max_val = 50
    # function
    break_prob = 1
    if energy < min_val:
        break_prob = 1
    elif energy > max_val:
        # The range of SNR may vary between 1dB and 30dB. The optimum SNR range is 18–30dB
        break_prob = norm(snr, 1, 30)
    else:
        break_prob = 0.5*norm(energy) + 0.5*norm(snr, 1, 30)

    # --- other variables ----
    # there is no way to say how long the link can be up.
    link_stab = norm(edge['link_stability'], 0, 600)
    # Therefore, 10 min is used, because no simulation time with more than 600seconds is utilized from now on!

    throughput = norm(edge['throughput'], 0, 1000)  # 1Gbs - (Mbps)
    delivery = norm(edge['delivery'])
    delay = 1 - norm(edge['delay'], 0, 100)  # (seconds) - inverse range

    n = G.number_of_nodes()
    maxEdges = 1/(n(n - 1) / 2)

    val = w[0] * delivery + w[1] * delay + w[2] * throughput + w[3] * link_stab + w[4] * \
        break_prob + w[5] * maxEdges + w[6] * \
        n_routes  # [0,1] # TODO faltam aqui variaveis

    return val


if __name__ == "__main__":
    weights[DATA] = read_weights(DATA)
    G = None
    nodeA = 0
    nodeB = 1
    get_link_value(weights[DATA], nodeA, nodeB, G)
