from enum import Enum

from ryuController.utils import MAX_COST

import logging
logger = logging.getLogger(__name__)

#Always import config (the module), not from config import shared_value, because the latter copies the value into the importing file’s namespace and won’t update.
import ryuController.config_manager as cfg

def get_service(s):
    return s

def get_dataRate_by_service(s):
    bw = cfg.get_dataRate_by_port(s)
    if bw is None:
        raise Exception("DataRate is none for port %d", s)
    return bw

class FlowKey:
    def __init__(self, ip_src, ip_dst, ser):
        """
        ip_src != requester vertex
        Args:
            ip_src: vertex index
            ip_dst: vertex index
            ser:
        """
        self.ip_src = ip_src
        self.ip_dst = ip_dst
        self.service = ser # source udp port
        self.priority, self.og_bw = cfg.get_priority_bw(self.ip_src, self.ip_dst, self.service)
        if self.priority is None or self.og_bw is None:
            raise Exception("Something went wrong with config file")
        self.meter_id = None
        self.current_bw = 0

    def get_dataRate(self):
        if self.meter_id is not None:
            return  max(0, self.current_bw)
        return self.og_bw

    def update_meter_id(self, id, rate):
        self.meter_id = id
        self.current_bw = rate



    def __str__(self):
        return "({},{},{})".format(self.ip_src, self.ip_dst, self.service)
    def __repr__(self):
        return "({},{},{})".format(self.ip_src, self.ip_dst, self.service)

DEFAULT_HARD_TIMEOUT = 5

# functional syntax
PathState = Enum('State', ['START', 'HAS_PATH', 'LEGACY', 'BROKEN'])


def get_last_sdn_device(self, path, G):
    for n in path[::-1]:
        if G.nodes[n]['isOF']:
            return n

class Path:
    def __init__(self, cost, path):
        if len(path) ==1:
            self.cost = MAX_COST
        else:
            self.cost = cost
        self.hard_timeout = DEFAULT_HARD_TIMEOUT
        self.path = path
        self.state = PathState.START # just used for this part
        #self.last_known_sdn = path[0]
        self.sdn_node_req = None # index in graph
        self.transmitter = None # mac address

    # def canChangePath(self, p2, t):
    #     if (self.cost != p2.cost and self.path!= p2.path) or (self.path == p2.path and (self.trans_t + DEFAULT_BLOCK_T < t or self.cost != p2.cost )) :
    #         return True
    #     return False

    def usable_path(self):
        return len(self.path) > 1

    def is_last_sdn_device(self, v, G):
        assert v in self.path, 'node not in predifined path!'
        if get_last_sdn_device(self.path, G) == v :
            return True
        return False

    def get_sub_path(self,v, G):
        if self.path == []: # OPF_NORMAL
            return []

        if v in self.path:
            return self.path[0:self.path.index(v)] # [0 ..., v-1]
        # G_neigh = G.neighbors(v)
        # count = 0
        # while G_neigh != [] and count < 50: # cut-off : 50 nodes
        #     u = G_neigh[0]
        #     G_neigh = G_neigh[1::]
        #     if u in self.path and  not G.nodes[u]['isOF']: # 'Cannot be OPF device the last neighbour'
        #         return self.path[0:self.path.index(u)+1] # [0 ...., u]
        #     else:
        #         G_neigh = G_neigh + G.neighbors(u)
        #     count +=1

        idx_last_sdn = self.path.index(self.last_known_sdn)
        u = self.path[idx_last_sdn+1]
        if G.nodes[u]['isOF'] == False: # if next is legacy
            if v == u:
                return self.path[0:idx_last_sdn]
            else:
                return self.path[0:idx_last_sdn+1]  # [0 ..., v-1]
        else:
            assert self.last_known_sdn == u, 'Something is not correct {} {}'.format(self.last_known_sdn, u)
            return  []

        return []




    def get_packet_out(self, net, node_idx = 0):
        """
        gets the corresponding information for packet_out out of self.path
        Returns: packet_out = (<port>, <mac>)
        """
        tmp_path = self.path[node_idx::]
        i = 1
        while i< len(tmp_path):
            if  not net.is_same_dev(tmp_path[i-1], tmp_path[i] ):
                return (self.G.nodes[i-1]['port_no'], self.G.nodes[i]['hw_addr'])
            else: # is same device
                i+=1


        # packet_out = (<port>, <mac>)

        return (4294967290, None) # NORMAL_OUTPUT_PORT


    def get_mac_port(self, node_idx, G ):
        assert node_idx != self.path[-1], 'Destination node, cannot compute mac and por for next hop!'
        v_next = self.path[node_idx+1]
        u_sender = self.path[node_idx]
        return G.nodes[u_sender]['port_no'], G.nodes[v_next]['hw_addr']


    # def merge_paths(self, p:list)->:
    def get_orders(self, net, node_idx = 0, trans_addr = None): # checked!!!
        """
        Get all the FLOW mods orders from node_idx to the end
        Args:
            net: the graph
            node_idx: the sdn device that sent the packet_in
            trans_addr: the src mac addr of the packet received by node_idx

        Returns: list of tuples with orders for FLOW mods (<idx for dpid>, <port>, <next mac>, <rcv_mac for match field>)
        """

        assert node_idx==0 and trans_addr is not None, 'If fisrt sdn device is required, then the transmission address of the message needs to be declred!'
        if node_idx !=0:
            assert not net.is_same_dev(self.path[node_idx], self.path[node_idx-1]), 'If devices are the same, the one requesting for the FLOW_MODS should be interface receiving the packet = node_idx -1!'

        assert len(self.path) > 1, 'PATH is LEGACY!'
        logger.info("get_orders %s", str((node_idx , trans_addr)))
        p = [net.exists(None,None, '', trans_addr) ]+self.path
        l = []
        i = node_idx + 1
        while i+1 < len(p):
            # index in path
            sender_idx = i
            next_idx = i+1
            trans_addr_idx = i - 1

            # index in graph
            u_sender = p[sender_idx]
            v_next = p[next_idx]
            b_match_rcv = p[trans_addr_idx]

            #print(b_match_rcv, u_sender, v_next)

            if net.G.nodes[u_sender]['isOF'] == False:
                i+=1
                continue
            if not net.is_same_dev(u_sender, v_next):
                l+=[ (u_sender, net.G.nodes[u_sender]['port_no'], net.G.nodes[v_next]['hw_addr'],net.G.nodes[b_match_rcv]['hw_addr'] )]
                i+=1
            else:  # is same device
                #u_sender = self.path[sender_idx+1]
                if p[-1] == v_next: #last node --> normal
                    l += [(u_sender, 4294967290, None, net.G.nodes[b_match_rcv]['hw_addr'])]
                    logger.debug("end get_orders %s", str(l))
                    return l
                else:
                    v_next_next = p[next_idx+1]
                    l += [(u_sender, net.G.nodes[v_next]['port_no'], net.G.nodes[v_next_next]['hw_addr'],
                           net.G.nodes[b_match_rcv]['hw_addr'])]
                i += 2



        if net.G.nodes[p[-1]]['isOF']:
            b_match_rcv = p[-2]
            l+=[(p[-1], 4294967290, None, net.G.nodes[b_match_rcv]['hw_addr'])]
        logger.debug("end get_orders %s", str(l))
        return l


    def get_packet_out(self, net, node_idx = 0, trans_addr = None): # checked!!!
        """
        Get all the FLOW mods orders from node_idx to the end
        Args:
            net: the graph
            node_idx: the sdn device that sent the packet_in
            trans_addr: the src mac addr of the packet received by node_idx

        Returns: list of tuples with orders for FLOW mods (<idx for dpid>, <port>, <next mac>, <rcv_mac for match field>)
        """
        assert isinstance(trans_addr, str), "Transmission address should be a string! and not the index"
        # assert node_idx==0 and trans_addr is not None, 'If fisrt sdn device is required, then the transmission address of the message needs to be declred!'
        assert not (node_idx == 0 and trans_addr is None), 'If fisrt sdn device is required, then the transmission address of the message needs to be declred!'
        if node_idx !=0:
            assert not net.is_same_dev(self.path[node_idx], self.path[node_idx-1]), 'If devices are the same, the one requesting for the FLOW_MODS should be interface receiving the packet = node_idx -1!'

        assert len(self.path) > 1, 'PATH is LEGACY!'
        logger.info("get_orders %s", str((node_idx , trans_addr)))
        p = [net.exists(None,None, '', trans_addr) ]+self.path
        l = []
        i = node_idx + 1
        print(p)
        while i+1 < len(p):
            # index in path
            sender_idx = i
            next_idx = i+1
            trans_addr_idx = i - 1

            # index in graph
            u_sender = p[sender_idx]
            v_next = p[next_idx]
            b_match_rcv = p[trans_addr_idx]

            #print(b_match_rcv, u_sender, v_next)

            if net.G.nodes[u_sender]['isOF'] == False:
                i+=1
                continue
            if not net.is_same_dev(u_sender, v_next):
                l+=[ (u_sender, net.G.nodes[u_sender]['port_no'], net.G.nodes[v_next]['hw_addr'],net.G.nodes[b_match_rcv]['hw_addr'] )]
                break
            else:  # is same device
                #u_sender = self.path[sender_idx+1]
                if p[-1] == v_next: #last node --> normal
                    l += [(u_sender, 4294967290, None, net.G.nodes[b_match_rcv]['hw_addr'])]
                    logger.debug("end get_orders %s", str(l))
                    return l
                else:
                    v_next_next = p[next_idx+1]
                    l += [(u_sender, net.G.nodes[v_next]['port_no'], net.G.nodes[v_next_next]['hw_addr'],
                           net.G.nodes[b_match_rcv]['hw_addr'])]
                break


        if net.G.nodes[p[-1]]['isOF']:
            b_match_rcv = p[-2]
            l+=[(p[-1], 4294967290, None, net.G.nodes[b_match_rcv]['hw_addr'])]
        logger.debug("end get_orders %s", str(l[0]))
        return l[0]

    def get_next_order(self, net, node_idx = 0):
        """
        gets the corresponding information for packet_out out of self.path
        Returns: packet_out = (<port>, <mac>)
        """
        tmp_path = self.path[node_idx::]
        i = 1
        while i< len(tmp_path):
            if  not net.is_same_dev(tmp_path[i-1], tmp_path[i] ):
                return (self.G.nodes[i-1]['port_no'], self.G.nodes[i]['hw_addr'], i)
            else: # is same device
                i+=1


        # packet_out = (<port>, <mac>)

        return (4294967290, None, 0) # NORMAL_OUTPUT_PORT


class Routes:
    def __init__(self):
        #self.routes = {} # {dpId: {flow_key: (next_mac, out_port) } } --> flow_key = (src,dst,service)
        self.flows = {}  # {flow_key: [class Paths] } graph indexes # TODO index 0 is the one in place
        self.priority = 1 # incremental variable
        #self.paths = {}  # {flow_key: [paths] }
        self.routing_variables = {'gama': 1.0, 'tech_cost':{1:0.5, 2:0.5}}

        self.meter_counter = 1

    def delete_path_req(self, fkey, requester):
        if fkey not in self.flows.keys():
            return -1
        # delete the most old path from this requester (as adding a new flow mod with different priorities will not
        #   delete the entrey, and therefore will trigger this function on idle timeout)
        for i in range(len(self.flows[fkey]) - 1, -1, -1):  # iterate backwards
            if self.flows[fkey][i].sdn_node_req == requester:
                del self.flows[fkey][i]
                break  # stop after deleting the first (rightmost) match

        if len(self.flows[fkey]) == 0:
            #delete key
            del self.flows[fkey]
            logger.info("Deleted Key %s", str(fkey))
        return 0

    def del_key(self, fkey):
        if fkey not in self.flows.keys():
            return -1
        del self.flows[fkey]
        logger.info("Deleted Key %s", str(fkey))
        return 0

    def get_nIdx_to_fkey(self, nodes):
        nIdx_to_fKey = {}  # {idx: [f_key]}
        for fk in self.flows.keys():
            for p in self.flows[fk]:
                if p.state != PathState.HAS_PATH:
                    break
                for n in list(set(p.path) & set(nodes)):
                    nIdx_to_fKey.setdefault(n, []).append(fk)
        return nIdx_to_fKey


    def __str__(self):
        return str(len(self.flows))


    def flow_exists(self, ip_src, index_dst, service):
        for i in self.flows.keys():
            if i.ip_src== ip_src and  i.ip_dst == index_dst and i.service== service:
                return True

        return False
    def get_flow_key(self, ip_src, index_dst, service):
        for i in self.flows.keys():
            if i.ip_src== ip_src and  i.ip_dst == index_dst and i.service== service:
                return i

        return None

    def delete_path(self, f_key):
        """
        sends order to delete the path in the sdn device
        Args:
            f_key: the path/paths to delete

        Returns: list of orders for OPF

        """
        # TODO !!
        for dev in self.routes.keys():
            if f_key in self.routes[dev].keys():
                del self.routes[dev][f_key]
                # TODO send order to delete this in device

        for dev in self.groups.keys():
            if f_key in self.groups[dev].keys():
                del self.groups[dev][f_key]
                # TODO delete group
        for dev in self.meters.keys():
            pass




    def insert_path(self, f_key, node, P):
        """
        inserts path in the class and creates the orders to send to the controller
        Args:
            f_key: <type FlowKey>
            node: node that requested this path, in graph vertex
            P: Path to be inserted
        """
        logger.info("insert_path %s %d %s", str(f_key), node, P.path)
        f_key.og_bw = get_dataRate_by_service(f_key.service)

        assert isinstance(P, Path), 'New Path not in accordance!'
        assert P.sdn_node_req is not None, 'Path needs a sdn_node_req!'
        assert P.transmitter is not None , 'Path needs a transmitter!'

        # update state of P
        if P.usable_path():
            P.state = PathState.HAS_PATH
        else:
            P.state = PathState.LEGACY

        if f_key not in self.flows.keys():
            self.flows[f_key] = [P]
            logger.debug("inserted new path in route.flows, position [0]: %s --> %s", self.flows[f_key][0].path, self.flows[f_key][0].state)
            return

        # --------------
        self.flows[f_key].insert(0, P)  # # {flow_key: [paths] } graph indexes
        logger.debug("Path inserted in route.flows %d", len(self.flows[f_key]))

        for i in range(0, len(self.flows[f_key])):
            logger.debug(" %d : %s --> %s", i, self.flows[f_key][i].path, self.flows[f_key][i].state)



    def check_broken_paths(self, deleted_edges):
        """
        when graph updates occur, check if any affect the routes in use
        Args:
            deleted_edges: list of deleted edges

        Returns: list of f_keys

        """
        logger.debug("--check_broken_paths %s", str(deleted_edges))
        affected_f_keys = []
        bool_key_affected = False
        for k, v in self.flows.items(): # v --> [<Path>]
            for idx in range(0, len(v)) : # for each path for this f_key
                if v[idx].state != PathState.LEGACY:
                    l = v[idx].path
                    for e in [(l[i], l[i + 1]) for i in range(0, len(l) - 1)]:
                        if e in deleted_edges or (e[1], e[0]) in deleted_edges:
                            affected_f_keys += [k]
                            bool_key_affected = True
                            break
                if bool_key_affected :
                    bool_key_affected = False
                    break

        # update all paths to BROKEN state to be deleted
        for fk in affected_f_keys:
            for p in self.flows[fk]:
                p.state = PathState.BROKEN

        return affected_f_keys
