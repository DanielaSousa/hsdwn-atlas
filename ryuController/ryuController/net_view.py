
import networkx as nx

DEV_PATH = 1
IPv4 = 2
MAC = 4

import logging
logger = logging.getLogger(__name__)

class Bandwidth:
    def __init__(self, dpid_a, dpid_b):
        self.a_idx = min(dpid_a, dpid_b)
        self.b_idx = max(dpid_a, dpid_b)
        self.a_avbw = -1
        self.b_avbw = -1

    def add_avbw(self, dpid, avbw):
        assert dpid== self.a_idx or dpid==self.b_idx, "DpID does not match this edge bw!"
        if dpid== self.a_idx :
            self.a_avbw = avbw
        else:
            self.b_avbw = avbw

    def get_avbw(self):
        if self.a_avbw == -1 and self.b_avbw == -1:
            raise Exception("Both values are not update!")
        if self.a_avbw == -1:
            return self.b_avbw
        if self.b_avbw == -1:
            return self.a_avbw
        # average
        return (self.a_avbw + self.b_avbw)/2

    def __repr__(self):
        return "({}, {}): bits/s".format(self.a_avbw, self.b_avbw, self.get_avbw())

    def __str__(self):
        return "({}, {}): bits/s".format(self.a_avbw, self.b_avbw, self.get_avbw())


class Net:
    """
    GRAPH --> (node (a.k.a. vertex_id), attr)
    attr = {'datapath_id': None, 'port_no': None, 'hw_addr': None, 'ipv4_addr': None,
                    'vertex_id': None, 'isValid': False, 'isOF': False}
    maps (a.k.a. dict):
    --> (dpid, port_no) ->vertex_id
    --> (ip) -> vertex_index
    --> (mac) -> vertex_index

    In the graph, each interface/port_no is a node/vertex.
    In the same devices, the link between vertexes has a weight of zero. It can change if the change in technologies requires a operation that delays the transmission

    """
    def __init__(self, theta):
        self.G = nx.Graph() # TODO DiGraph()
        self.n_vertex = 0
        # self.mac2index = {} # std::map<Mac48Address, int node_index> mac2index;
        self.mac_to_index = {}
        self.ip_to_index = {}
        self.uuid_to_index = {}  # (dpid, port_no) ->vertex_id
        logger.info("Start NetView")

        pass

    def add_node(self, dpid, port_no, ip, mac):
        """
         vertex_id is between[1, inf]
        :param dpid:
        :param port_no:
        :param ip:
        :param mac:
        :return: (None | vertex_id) -->(failure | success)
        """
        vertex_id = self.n_vertex+1
        attr = {'datapath_id': None, 'port_no': None, 'hw_addr': None, 'ipv4_addr': None, 'energy': 50,
                'vertex_id': vertex_id, 'isValid': False, 'isOF': False , 'avbw': 1000, 'max_C': 1000, 'type':None}
        if (dpid and port_no) is not None:
            attr['datapath_id'] = dpid
            attr['port_no'] = port_no
            self.uuid_to_index[(dpid, port_no)] = vertex_id

        if ip:
            attr['ipv4_addr'] = ip
            self.ip_to_index[ip] = vertex_id
            # attr['isValid'] = True
        if mac:
            attr['hw_addr'] = mac
            self.mac_to_index[mac] = vertex_id
            # attr['isValid'] = True
        if (ip or mac or dpid or port_no) is None:
            print("ERROR : not valid info!")
            return None
        if self.check_valid_attr(attr):
            attr['isValid'] = True

        # print(vertex_id, attr)
        self.G.add_node(vertex_id, **attr)  # (add_node does not return )

        print("New Entry", vertex_id, dpid, port_no, ip, mac, flush=True)
        print("\n\t dpId:", dpid, "Graph index:", vertex_id, "ip:", ip, "\n" , flush=True)
        self.n_vertex = vertex_id  # update node count
        return self.n_vertex

    def update_node(self, dpid, port_no, ip, mac, index=None):
        """
        update and/or add node
        :param dpid:
        :param port_no:
        :param ip:
        :param mac:
        :param index:
        :return: vertex_index
        """
        # see if exists

        # if index:
        #     tmp_id = index
        #     # TODO check if index is really the proper index for this node
        if index is None:
            index = self.exists(dpid, port_no, ip, mac)
            # print('exists',index,dpid, port_no, ip, mac )

        if index is None:  # add new node
            index = self.add_node(dpid, port_no, ip, mac)
            # print(index,dpid, port_no, ip, mac )
        else:
            # print("Update Entry", index, dpid, port_no, ip, mac)
            pass

        if (self.G.nodes[index]['isValid'] and dpid is None and port_no is None):
            # print("IS VALID ", self.G.nodes[index])
            return index  # no field to update

        # update node info now
        if ip and self.G.nodes[index]['ipv4_addr'] is None:
            self.G.nodes[index]['ipv4_addr'] = ip
            # self.G.nodes[tmp_id]['isValid'] = True
            self.ip_to_index[ip] = index
        if mac and self.G.nodes[index]['hw_addr'] is None:
            self.G.nodes[index]['hw_addr'] = mac
            # self.G.nodes[tmp_id]['isValid'] = True
            self.mac_to_index[mac] = index
        if dpid and port_no:
            self.G.nodes[index]['datapath_id'] = dpid
            self.G.nodes[index]['port_no'] = port_no
            self.G.nodes[index]['isOF'] = True
            self.uuid_to_index[(dpid, port_no)] = index

        if self.check_valid_attr(self.G.nodes[index]):
            self.G.nodes[index]['isValid'] = True
        # print("UPDATED", self.G.nodes[index])
        return index

    def add_edge(self, src, dst, info):
        """
        add a edge between 2 nodes
        :param src: node index
        :param dst: node index
        :param info: dr, delay, through, link_stab, snr
        :return:
        """

        if len(info) > 3:
            (dr, delay, through, link_stab, snr) = info
        else:  # Backwards compatibility
            (dr, link_stab, snr) = info
            delay = 0
            through = 0

        # self.G.add_edge(src, dst, weight=info)
        attr = {'delivery': dr, 'delay': delay, 'throughput': through,
                'link_stability': link_stab, 'snr': snr}
        self.G.add_edge(src, dst, **attr)

    def remove_edge(self):
        pass

    def clear_graph(self):
        pass

    def remove_node(self):
        pass

    def exists(self, dpid, port_no, ip, mac):
        """
        check if node exists in graph
        :param dpid:
        :param port_no:
        :param ip:
        :param mac:
        :return: (index [1, inf] | None ) --> (success | not found)
        """
        assert ((dpid or port_no or ip or mac) is not None)
        tmp_id = None
        # print("EXISTS --> ", dpid, port_no, ip, mac)
        if dpid and port_no and (tmp_id is None):  # ou alguma mensagem packet_in
            tmp_id = self.uuid_to_index.get((dpid, port_no), None)
        elif dpid and (tmp_id is None):  # get the first port_n for this datapathID
            for (key, val) in self.uuid_to_index.keys():
                if key == dpid:
                    tmp_id = self.uuid_to_index[(key, val)]
                    break
        if ip and (tmp_id is None):  # pode ter sido adicionado quando recebeu o experimenter
            tmp_id = self.ip_to_index.get(ip, None)
            # print(self.ip_to_index)
        # ou recebeu msg em que o nó anteior nao era conhecido e ainda nao fez update pelo experimenter
        if mac and (tmp_id is None):
            tmp_id = self.mac_to_index.get(mac, None)

        return tmp_id

    def get_node_attr(self, node_index):
        return self.G.nodes[node_index]

    def get_edge_attr(self, nodeA, nodeB):
        return self.G.edges[nodeA, nodeB]
        #   self.G[nodeA][nodeB]

    def clear_neigh_edges_from_node(self, dpid):
        """
        It deletes all edges connected to this node, except if the neigh. node is the same device
        :param dpid:
        :return:
        """
        ebunch = self.get_neigh_edges_from(dpid)
        self.G.remove_edges_from(ebunch)

    def get_neigh_edges_from(self, dpid):
        """
        get all edges connected to the device dpid
        :param dpid:
        :return:
        """
        ebunch = []
        for (key, val) in self.uuid_to_index.keys():
            if key == dpid:
                for (i, j) in self.G.edges(self.uuid_to_index[(key, val)]):
                    if ((i, j) not in ebunch) and (not self.is_same_dev(i, j)):
                        ebunch.append((i, j))

        return ebunch

    def check_valid_attr(self, a):
        # if (a['datapath_id'] and a['port_no'] and a['hw_addr'] and a['ipv4_addr']) is None:
        if (a['hw_addr'] and a['ipv4_addr']) is None:
            return False
        return True

    def is_same_dev(self, dev_a, dev_b):
        """
        check if is same device
        :param dev_a: node_id
        :param dev_b: node_index in graph
        :return: bool
        """
        assert dev_a in self.G.nodes, "ERROR dev_a {} not in graph!".format(
            dev_a)
        assert dev_b in self.G.nodes, "ERROR dev_b {} not in graph!".format(
            dev_b)
        if self.G.nodes[dev_a]['isOF'] == False or self.G.nodes[dev_b]['isOF'] == False:
            # This should always give false
            return self.G.nodes[dev_a]['ipv4_addr'] == self.G.nodes[dev_b]['ipv4_addr']

        return self.G.nodes[dev_a]['datapath_id'] == self.G.nodes[dev_b]['datapath_id']

    def add_edge_same_dev(self, dpid):
        """
        for the same device, add edge between the port_n
        :param dpid:
        :return:
        """
        # -- TODO self.add_edge is not correct

        # indexes = []
        # for (key, val) in self.uuid_to_index.keys():
        #     if key == dpid:
        #         indexes.append(self.uuid_to_index[(key, val)])

        # for i in range(len(indexes)):
        #     for j in range(i+1, len(indexes)):
        #         # in the same device the number of jumps do not count
        #         self.add_edge(indexes[i], indexes[j], 0)
