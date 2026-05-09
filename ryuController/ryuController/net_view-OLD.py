import logging
import networkx as nx
from ryuController.Path_Calculator.path_search import *
from ryuController.Path_Calculator.multi_path_search import multi_path_multi_variable_Dn
import pickle
# struct node_uid
# {
#     bool isValid = false;
#     bool isOF = false;
#     int vertex_id;
#     uint64_t datapath;
#     uint32_t port_no ;
#     std::string uuid;
#     Mac48Address hw_addr;
#     Ipv4Address ipv4_addr;
# };

# type of info from node_uid
DEV_PATH = 1
IPv4 = 2
MAC = 4
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


logger = logging.getLogger(__name__)


class Net:

    def __init__(self, theta):
        self.G = nx.Graph()
        self.n_vertex = 0
        # self.mac2index = {} # std::map<Mac48Address, int node_index> mac2index;
        self.mac_to_index = {}
        self.ip_to_index = {}
        self.uuid_to_index = {}  # (dpid, port_no) ->vertex_id
        # routes[dpId].insert({i_dst, i_out, outPort, inPort});
        self.routes = {}
        self.theta = theta
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
        attr = {'datapath_id': None, 'port_no': None, 'hw_addr': None, 'ipv4_addr': None,
                'vertex_id': vertex_id, 'isValid': False, 'isOF': False, 'energy': 100}
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
        print("New Entry", vertex_id, dpid, port_no, ip, mac)
        print("\n\t dpId:", dpid, "Graph index:", vertex_id, "ip:", ip, "\n" )
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

    def get_path(self, src, dst, service, ip_src):
        """
        With the search alg retrieves if possible a path between nodeA and nodeB
        :param src: where i'm node index
        :param dst: node_index
        :return: (None | (next_mac, out_port) )
        """
        assert self.is_same_dev(
            src, dst) == False, "ERROR, same device, should be sent as NORMAL!"
        # nx.shortest_path(self.G, source=nodeA, target=nodeB, weight='weight')
        try:
            # path = nx.dijkstra_path(self.G, source=src, target=dst, weight='weight')
            # _dist, path = djikstra(self.G, source=src, target=dst, weight='weight')
            _dist, path = multi_variable_Dn(
                self.G, source=src, routes=self.routes, target=dst, weight=self.theta[service])
            
        except Exception as err: #nx.NetworkXNoPath
            print(err)
            return (None, None)
        try:
            with open('paths.txt', "a") as file_object:
                # Append 'hello' at the end of file
                file_object.write(str(self.G.nodes[src]['ipv4_addr']) + ',' +
                                  str(self.G.nodes[dst]['ipv4_addr']) + ',' +
                                  str([(self.G.nodes[n]['ipv4_addr'], n) for n in path]) +
                                  str(_dist) + '\n')
        except Exception as e:
            print(e)
            
            # print('\n\t', [(self.G.nodes[n]['ipv4_addr'],n) for n in path], '\n')

        if len(path) < 2:
            print("NO path found!")
            return (None, None)

        me = self.G.nodes[src]  # path[0]
        next = self.G.nodes[path[1]]
        port_out = me['port_no']  # default

        if self.is_same_dev(src, path[1]):  # next node is one of mine
            port_out = next['port_no']
            next = self.G.nodes[path[2]]
        # print("Get Path: in ", me['datapath_id'], me['port_no'], me['ipv4_addr'], " -> next ", next, " next_mac ",
        #       next['hw_addr'], " next_ip ", next['ipv4_addr'], " dst_ip " , self.G.nodes[dst]['ipv4_addr'] )
        # update routes
        try:
            key = me['datapath_id']
            if key not in self.routes.keys():
                self.routes[key] = {}

            self.routes[key][dst]  = (next['hw_addr'], port_out, (ip_src, dst, service) )

        except Exception as err:
            print(err)
            exit(0)

        return (next['hw_addr'], port_out)
    
    
    def get_path_to_string(self, src, dst, service):
        """
        With the search alg retrieves if possible a path between nodeA and nodeB
        :param src: where i'm node index
        :param dst: node_index
        :return: (None | (next_mac, out_port) )
        """
        assert self.is_same_dev(
            src, dst) == False, "ERROR, same device, should be sent as NORMAL!"

        # nx.shortest_path(self.G, source=nodeA, target=nodeB, weight='weight')
        try:
            # path = nx.dijkstra_path(self.G, source=src, target=dst, weight='weight')
            # _dist, path = djikstra(self.G, source=src, target=dst, weight='weight')
            _dist, path = multi_variable_Dn(
                self.G, source=src, routes=self.routes, target=dst, weight=self.theta[service])
            with open('paths.txt', "a") as file_object:
                # Append 'hello' at the end of file
                file_object.write(str(self.G.nodes[src]['ipv4_addr']) + ',' +
                                  str(self.G.nodes[dst]['ipv4_addr']) + ',' +
                                  str([(self.G.nodes[n]['ipv4_addr'], n) for n in path]) +
                                  str(_dist) + '\n')

            # print('\n\t', [(self.G.nodes[n]['ipv4_addr'],n) for n in path], '\n')
        except nx.NetworkXNoPath as err:
            print(err)
            return ""

        if len(path) < 2:
            print("NO path found!")
            return ""
        
        # --- serialize to string
        f_str = ""
        for i in range(0, len(path)-1):
            (sw_id, netx_hw_addr, port_out) = self.get_edge_rule(path[i:])
            f_str += "{} {} {},".format(sw_id, netx_hw_addr, port_out)
            # update routes
            self.routes[sw_id][dst]  = (netx_hw_addr, port_out, (src, dst, service) )

        # f_str = f_str[:-1]  # remove last comma
        f_str += "|"
        # f_str = f_str[1:]  # remove first "|"
        print(f_str)
        return f_str
        


    def get_multi_path(self, src, dst, service, n_paths=2, disj_perc=70):
        """
        retrieves multiple paths if exists-> string format
        1: returns multiple paths in list form or other form?

        """
        disj_perc = disj_perc/100
        print("OLA1")
        try:
            paths = multi_path_multi_variable_Dn(
                self.G, src, self.routes, dst, self.theta[service], k=n_paths, disj_perc=disj_perc)
        except Exception as e:
            print(e)
        if paths == []:
            print("empty paths")
            return ""
        print("OLA2")
        f_str = ""
        # serialize to string
        for p in paths:
            if len(p) < 2:
                print("NO path found!")
                return (None, None) # TODO

            for i in range(0, len(p)-1):
                (sw_id, netx_hw_addr, port_out) = self.get_edge_rule(p[i:])
                f_str += "{} {} {},".format(sw_id, netx_hw_addr, port_out)

                # TODO preciso de update das rotas
            # f_str = f_str[:-1]  # remove last comma
            f_str += "|"
        # f_str = f_str[1:]  # remove first "|"
        print(f_str)
        return f_str

    def get_edge_rule(self, p):
        src = p[0]
        me = self.G.nodes[src]  # path[0]
        next = self.G.nodes[p[1]]
        port_out = me['port_no']  # default
        # print(path)

        if self.is_same_dev(src, p[1]):  # next node is one of mine
            port_out = next['port_no']
            next = self.G.nodes[p[2]]

        # sw_id, netx_hw_addr, port_out
        return (me['datapath_id'], next['hw_addr'], port_out)

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
