from ryuController.paths import FlowKey
# ----------------

#
# from paths import FlowKey
import logging
import traceback
logger_utils = logging.getLogger(__name__)
logger_utils.setLevel(logging.DEBUG)


def reset_avbw_to_maxC(G_copy):
    """
    Return a deepcopy of G where each node's 'avbw' is set to its 'max_C'.

    Parameters:
        G : networkx.Graph (undirected) with node attributes 'avbw' and 'max_C'

    Returns:
        G: G with updated 'avbw'
    """

    for n, data in G_copy.nodes(data=True):
        if 'max_C' in data:
            data['avbw'] = data['max_C']
        else:
            data['avbw'] = data.get('avbw', 0)

    return G_copy

def update_node_avbw_for_flow(G, path, flow_bw):
    """
    Update each node's 'avbw' along the given path by subtracting the flow's bandwidth.

    Parameters:
        G       : networkx.Graph with node attributes 'avbw'
        path    : list of nodes in the flow (e.g., [1, 2, 3, 4])
        flow_bw : float, bandwidth used by the flow
    """
    for n in path:
        if 'avbw' in G.nodes[n]:
            G.nodes[n]['avbw'] = max(0, G.nodes[n]['avbw'] - flow_bw)
        else:
            # If avbw not set, initialize with 0 minus flow_bw (clamped at 0)
            G.nodes[n]['avbw'] = 0


def filter_and_sort_f_keys(paths_in_nodes, priority_threshold):
    """
    Filter and sort f_keys based on the following criteria:
    1. Remove f_keys with priority lower than priority_threshold.
    2. Sort by the length of lists in paths_in_nodes (descending order).
    3. Sort by priority (higher priority = lower numerical value).
    4. Sort by dataRate (higher is better).

    :param paths_in_nodes: Dictionary mapping f_keys to lists of nodes.
    :param priority_threshold: Integer threshold for filtering and sorting priorities.
    :return: A sorted list of f_keys.
    """
    # Filter out f_keys with priority lower than the threshold
    filtered_f_keys = [
        f_key for f_key in paths_in_nodes.keys()
        if f_key.priority >= priority_threshold
    ]

    # Sort the remaining f_keys based on the criteria
    # Sort based on new criteria
    return sorted(
        filtered_f_keys,
        key=lambda f_key: (
            -f_key.priority,  # Higher priority first
            -f_key.get_dataRate(),  # Higher data rate first
            -len(paths_in_nodes[f_key])  # More nodes first
        )
    )



def sort_by_priority(keys: FlowKey):
    """
    Sort by priority of each flow key in keys
    Args:
        keys: list of <FlowKey>
    Returns: sorted list of <FlowKey>
    """
    return sorted(keys, key=lambda x: x.priority)  # lower value = higher importance

def get_min_avbw(p:list, G):
    """ get min avbw for path p from p[0] to p[-2]"""
    min_value = 10**9 # 1Gbps
    for n in p[:-1]:
        if not G.nodes[n]['isOF']:
            continue
        else:
            logger_utils.debug("calculating min value : %d VS avbw on node %d : %s",min_value, n, str(G.nodes[n]['avbw']) )
            min_value = min(min_value,G.nodes[n]['avbw'] )

    return max(min_value, 0)
