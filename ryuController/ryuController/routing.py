
from ryuController.net_view import *
import ryuController.Path_Calculator.path_calculation as pc
from ryuController.utils import *
from ryuController.net_view import *
from ryuController.paths import *
from ryuController.routing_utils import *
from ryuController.orders_str import *
from ryuController.config_manager import RECALCULATION_LEVEL
import sys
# ------------------------------

# from net_view import *
# import Path_Calculator.path_calculation as pc
# from utils import *
# from net_view import *
# from paths import *
# from routing_utils import *
# from orders_str import *
# # ------------------------------
import traceback
import logging

import networkx as nx
import copy

logger = logging.getLogger(__name__)

NORMAL_OUTPUT_PORT = 4294967290


class RoutingMechanism:
    def __init__(self, W):

        self.net = Net(W)
        self.thetas = W
        self.routes = Routes()
        self.inserted_edges = set()

    def adjust_existing_flows(self, flow_avbw: int, min_avbw: int, sp: int, path: list) -> list:
        """
        adjust existing flows when added a new priority flow
        Args:
            required_rate: the rate i need for this new flow
            sp: (service priority) tos filed used for differentiate priority given to this flow
            path: all the nodes involved in this new path

        Returns: tuple with the meter updates and if the new flow is also required to be rate limited
        """
        assert isinstance(path, list), "arg path is not a list"

        # Step 1: Calculate overflow bandwidth
        overflow_bw = max(0, flow_avbw - min_avbw)
        if overflow_bw == 0:
            return {},None  # No need to drop anything

        paths_in_nodes = {} # {f_key: [nodes on the new path]}
        nIdx_to_fKey = self.routes.get_nIdx_to_fkey([item for item in path if self.net.G.nodes[item]['isOF']]) # fill nIdx_to_fKey
        logging.info("nIdx_to_fkey --> %s", str(nIdx_to_fKey))

        for n in path:
            if not self.net.G.nodes[n]['isOF']:
                continue
            # else (is openflow node)
            # 1. get paths_in nodes
            for fk in nIdx_to_fKey[n]:
                paths_in_nodes.setdefault(fk, []).append(n)
        # 2. (sort) get the f_keys where the first is lower in priority, high in rate, and the most common one
        sorted_f_keys = filter_and_sort_f_keys(paths_in_nodes, priority_threshold=sp)
        logger.info("Sorted f_keys: %s", str(sorted_f_keys))
        # 3. remove avbw but only until 10%, unless, to not starve

        removed_rate = 0
        # Drop bandwidth from lower-priority flows
        meters = {} # f_key: rate limiter
        # for f_key in sorted_f_keys:
        #     f_key_dataRate = f_key.get_dataRate()
        #     if (required_rate <= f_key_dataRate*0.9 + removed_rate):
        #         # this key needs a rate limiter of ( f_key.dataRate - (required_rate-removed_rate))
        #         meters[f_key] = f_key_dataRate - (required_rate-removed_rate) # bits
        #         removed_rate = required_rate
        #         break
        #     else:
        #         meters[f_key] = f_key_dataRate*0.1 # only 10%
        #         removed_rate += f_key_dataRate*0.9

        for f_key in sorted_f_keys:
            if overflow_bw <= 0:
                break  # Stop if overflow is handled

            # Minimum bandwidth this flow must retain (10% of its original allocation)
            min_bw = 0.1 *  f_key.og_bw # flow['orig_bw']

            flow_current_bw = f_key.get_dataRate()
            # Maximum amount this flow can still contribute to the drop
            max_drop = flow_current_bw - min_bw  # Only drop what's left above min_bw

            # Actual drop (limited by what we need and what this flow can afford)
            drop_amount = min(max_drop, overflow_bw)

            # Compute the new allowed bandwidth
            new_bw = flow_current_bw - drop_amount
            meters[f_key] = new_bw

            # Reduce the overflow bandwidth
            overflow_bw -= drop_amount

        # 4. if not all available, i can rate limit this one
        #assert overflow_bw <= flow_avbw, 'Calculation of overflow is wrong!'
        if overflow_bw > flow_avbw:
            logger.info("Calculation of overflow is wrong! overflow = %d | flow_avbw = %d", overflow_bw , flow_avbw)
        if (overflow_bw > 0):
            return meters, max(flow_avbw - overflow_bw, 0) # flow_current_bw - drop_amount
        else:
            return meters, None

    def update_orders_rate_limiter(self, f_key, _path, orders, G = None):
        logger.info("update_orders_rate_limiter")
        #return orders
        if G is None:
            G = self.net.G
        if self.routes.flows[f_key][0].state == PathState.HAS_PATH and orders[0][
            0] != NORMAL_OUTPUT_PORT:  # has at least one hop
            min_avbw = get_min_avbw(_path, G)
            required_avbw = f_key.og_bw # bits per seconds
            meters, my_rate_limit = None, None

            if min_avbw < required_avbw:
                logger.info(" ADJUSTING PATH: required avbw %d (link avbw: %d)", required_avbw, min_avbw)
                meters, my_rate_limit = self.adjust_existing_flows(required_avbw ,  min_avbw, f_key.priority, _path)
                logger.info(" adjust_existing_flows to acommodate flow %s\n meters %s \n rate_limit: %s", str(f_key), str(meters), str(my_rate_limit))

            meters_orders = []
            if my_rate_limit is not None:
                # change my flow mod to include meters
                my_meter_mod = meter_mod(self.routes.meter_counter, my_rate_limit)
                logger.info("ADDED METER MOD: %s", str(my_meter_mod))
                f_key.update_meter_id(self.routes.meter_counter, my_rate_limit)
                self.routes.meter_counter += 1
                f_mod_idx = orders[1][1].find(' write:')
                f1 = orders[1][1][0:f_mod_idx]
                f2 = orders[1][1][f_mod_idx::]
                orders[1] = (orders[1][0], add_meter_id(f1, f_key.meter_id) + f2)
                logger.info("My order updated %s: ", str(orders[1]))
                meters_orders += [(orders[1][0], my_meter_mod)]

            if meters is not None:
                for fk, r in meters.items():
                    fk_path = self.routes.flows[fk][0]

                    needs_flow_mod = False
                    if fk.meter_id is None:
                        fk.meter_id = self.routes.meter_counter
                        self.routes.meter_counter += 1
                        needs_flow_mod = True
                        str_meter_mod = meter_mod(fk.meter_id, r)  # update meter
                        logger.info("ADDED METER MOD 2: %s", str(str_meter_mod))
                    else:
                        # update meter mod
                        str_meter_mod = meter_mod_modify(fk.meter_id, r)
                        logger.info("UPDATED METER MOD: %s kbps", str(str_meter_mod))
                    f_key.update_meter_id(fk.meter_id, r)
                    meters_orders += [(fk_path.sdn_node_req, str_meter_mod)]
                    logger.debug("METER-MOD %s", str_meter_mod)

                    if needs_flow_mod:  # it needs to alter the flow-mod

                        port, mac = fk_path.get_mac_port(0, G)
                        new_flow_mod = flow_mod(
                            prio=self.routes.priority,
                            eth_src=fk_path.transmitter,
                            ip_src= G.nodes[f_key.ip_src]['ipv4_addr'],
                            ip_dst= G.nodes[f_key.ip_dst]['ipv4_addr'],
                            udp_port=f_key.service,
                            output=port,
                            meter_id=fk.meter_id,
                            set_field=mac,
                            idle_timeout=1,
                            hard_timeout=fk_path.hard_timeout)
                        meters_orders += [(fk_path.sdn_node_req, new_flow_mod)]
                        if orders[1][0] == fk_path.sdn_node_req:
                            orders = [orders[0]] + orders[2::]
                        logger.info("FLOW-MOD + METER %s", new_flow_mod)

                orders = [orders[0]] + meters_orders + orders[1::]
        return orders
    # DONE -----------

    def get_legacy_orders_for_path(self, f_key: FlowKey, P: Path) -> list:
        assert P.usable_path(), "Path is legacy!"
        logging.info("get_legacy_orders_for_path --> usable_path -> get_orders %s", P.transmitter)
        l_rules = P.get_orders(self.net, node_idx=0, trans_addr=P.transmitter)
        orders = []
        for (dpid_idx, port, mac, match) in l_rules:
            orders += [(dpid_idx,
                        "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={} eth_type=0x0800,eth_src={},ip_src={},ip_dst={},ip_proto={},"
                        "udp_dst={} apply:output=normal".format(1, 5,
                                                                self.routes.priority,
                                                                match,
                                                                self.net.G.nodes[f_key.ip_src][
                                                                    'ipv4_addr'],
                                                                self.net.G.nodes[f_key.ip_dst][
                                                                    'ipv4_addr'], 17,
                                                                f_key.service))]
            self.routes.priority += 1

        return orders

    def get_orders_for_path(self, f_key: FlowKey, P: Path) -> list:
        """
        get orderd from trans_addr for the entire path with Packet out information in index 0
        Args:
            f_key:
            P:
        Returns: list of orders

        """
        logging.info("get_orders_for_path %s %s", str(f_key), str(P))
        if P.usable_path():
            logging.info("usable_path -> get_orders %s", P.transmitter)
            l_rules = P.get_orders(self.net, node_idx=0, trans_addr=P.transmitter)

            orders = [(l_rules[0][1], l_rules[0][2])]  # packet_out
            for (dpid_idx, port, mac, match) in l_rules:
                if port == NORMAL_OUTPUT_PORT:
                    orders += [(dpid_idx,
                                "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={} eth_type=0x0800,eth_src={},ip_src={},ip_dst={},ip_proto={},"
                                "udp_dst={} apply:output=normal".format(1, P.hard_timeout,
                                                                        self.routes.priority,
                                                                        match,
                                                                        self.net.G.nodes[f_key.ip_src][
                                                                            'ipv4_addr'],
                                                                        self.net.G.nodes[f_key.ip_dst][
                                                                            'ipv4_addr'], 17,
                                                                        f_key.service))]
                else:
                    orders += [
                        (dpid_idx,
                         "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={} eth_type=0x0800,eth_src={},"
                         "ip_src={},ip_dst={},ip_proto={},udp_dst={} "
                         "write:set_field=eth_dst:{},output={}".format(1, P.hard_timeout,
                                                                       self.routes.priority,
                                                                       match,
                                                                       self.net.G.nodes[f_key.ip_src]['ipv4_addr'],
                                                                       self.net.G.nodes[f_key.ip_dst]['ipv4_addr'],
                                                                       17, f_key.service,
                                                                       mac, port))]
                self.routes.priority += 1
        else:  # LEGACY
            orders = [(NORMAL_OUTPUT_PORT, None)]
            orders += [(P.sdn_node_req,
                        "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={} eth_type=0x0800,eth_src={},ip_src={},ip_dst={},ip_proto={},"
                        "udp_dst={} apply:output=normal".format(1, P.hard_timeout,
                                                                self.routes.priority,
                                                                P.transmitter,
                                                                self.net.G.nodes[f_key.ip_src][
                                                                    'ipv4_addr'],
                                                                self.net.G.nodes[f_key.ip_dst][
                                                                    'ipv4_addr'], 17,
                                                                f_key.service))]
        return orders

    # ------------------------------------
    # ------ CALLED by: PAcket_In parser --------
    def path_calculation(self, index_src: int, index_dst: int, service: int, true_src: int, trans_addr: str,
                         G=None) -> str:
        """
        Computes paths between index_src and index_destination for a specific service, when path does not exist
        Args:
            index_src: node index for the MAC (requesting node)
            index_dst: node index for the dst IP (destination node)
            service: service of the flow
            true_src: the src of the flow IP (creator of the packets)
            trans_addr: mac address of the node that sent the packet to the requesting node
            G : graph

        Returns: string of orders serialized
        """
        logger.info("path_calculation %s", str((index_src, index_dst, service, true_src, trans_addr)))
        if G is None:
            G = self.net.G
        f_key = FlowKey(true_src, index_dst, service)
        assert f_key not in self.routes.flows, 'Path calculation should not be used, path already exists! Try path recalculation.'
        logger.info("Flow key created %s", str(f_key))
        if self.net.is_same_dev(index_src, index_dst):
            logger.info("Packet %d -> %d : normal (same device)", index_src, index_dst)
            packet_out = (NORMAL_OUTPUT_PORT, None)
            orders = [packet_out, (index_src,
                                   "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={} eth_type=0x0800,ip_src={},ip_dst={},ip_proto={},"
                                   "udp_dst={} apply:output=normal".format(65535, 65535,
                                                                           self.routes.priority,
                                                                           self.net.G.nodes[f_key.ip_src][
                                                                               'ipv4_addr'],
                                                                           self.net.G.nodes[f_key.ip_dst][
                                                                               'ipv4_addr'], 17,
                                                                           f_key.service))]
            self.routes.priority += 1
        else:
            logger.info("Packet %d -> %d : get path",index_src, index_dst )

            _cost, _path = pc.get_best_path(index_src, index_dst, self.thetas[service], G, self.routes)
            print(_cost, _path)
            #_cost , _path = 1,  [1, 4, 5]
            logger.info("Solution %s: %s with cost %f", str(f_key), str(_path), _cost)
            P = Path(_cost, _path)
            P.sdn_node_req = index_src
            P.transmitter = trans_addr
            self.routes.insert_path(f_key, index_src, P)  # [(packet_out), (flow_mods)...]
            logger.info("inserted path in routes: %s", str(f_key))

            assert (self.routes.flows[f_key][0].state == PathState.HAS_PATH or
                    self.routes.flows[f_key][0].state == PathState.LEGACY), (
                'Path needs a true state! {}'.format(self.routes.flows[f_key][0].state))

            logger.info("Path state: %s", str(self.routes.flows[f_key][0].state))

            orders = self.get_orders_for_path(f_key, P)
            logger.info("Orders %s: ", str(orders))

            if P.usable_path():
                # update orders with rate adjusting flows if needed
                orders = self.update_orders_rate_limiter(f_key, _path, orders)
                #pass

            # update avbw for the next calculation; it will be updated for real when the next experimenter message arrives
            logger.info("Orders %s: ", str(orders))
            l = self.routes.flows[f_key][0].path
            if len(l) > 1:  # there is a path
                for n in [i for i in l if self.net.G.nodes[i]['isOF']]:
                    # increase avbw from nodes
                    self.net.G.nodes[n]['avbw'] = max(self.net.G.nodes[n]['avbw'] - f_key.get_dataRate(), 0)

        return self.serialize_OPF_rules(orders)


    def recalculate_flow(self, index_src: int, index_dst: int, service: int, true_src: str, trans_addr: str) -> list:
        """
        check if node is in path, if not delete key, and recalculate path as a new path
        Args:
            index_src: my_index
            index_dst: ip dst
            service:
            true_src: ip src
            trans_addr: pkt's transmitter address

        Returns: list of strings

        """
        assert self.routes.flow_exists(true_src, index_dst, service), "Flow does not found on routing Table!"
        assert self.net.is_same_dev(index_src, index_dst) == False, "ERROR, same device, should be sent as NORMAL!"
        f_key = self.routes.get_flow_key(true_src, index_dst, service)
        assert self.routes.flows[f_key][0].path[
                   -1] != index_src, 'It is destination, it should not be here! ({}) - 0:{} {} {}'.format(
            len(self.routes.flows[f_key]), self.routes.flows[f_key][1].path, self.routes.flows[f_key][0].path[-1],
            index_src)

        # ----- Get path ----------

        trans_addr_idx = self.net.exists(None, None, None, trans_addr)
        path_idx = None

        # get the path in use for this node
        for i in range(0, len(self.routes.flows[f_key])):
            if self.routes.flows[f_key][i].state != PathState.BROKEN and index_src in self.routes.flows[f_key][i].path:
                path_idx = i
                logger.info("Node found in Path %d: %s", path_idx, self.routes.flows[f_key][path_idx].path)
                break
        # ---------- check of deviation occour ------------
        try:
            if path_idx:
                p = self.routes.flows[f_key][path_idx]
                logger.debug("%s , %s, %s, %s", p.path, index_src, p.path.index(index_src), p.transmitter)
                (dpid, port, n_mac, match_trans) = p.get_packet_out(self.net, node_idx=p.path.index(index_src), trans_addr=p.transmitter)
                logger.debug("Path: (graph idx) %s -> trans addr for node %d : (mac) %s . arrived trans addr : (mac) %s",
                             p.path,index_src,  match_trans , trans_addr_idx)

                # ------ NO DEVIATION ------
                if match_trans == trans_addr:
                    # same rule applies
                    orders = [(port, n_mac)]
                    logger.debug("No DEVIATION found! --> sending Packet_out %s", str(orders))
                    return self.serialize_OPF_rules(orders)

            # ---- DEVIATION FOUND -----

            ## ------ DELETE KEY -----
            self.routes.del_key(f_key)
            ## ------- RECALCULATE PATH -----
            return self.path_calculation(index_src, index_dst, service, true_src, trans_addr, self.net.G)

        except Exception as inst:
            print(inst)  # __str__ allows args to be printed directly,
            traceback.print_exc()
            sys.stderr.flush()
            exit(-1)





    def process_flow(self, index_src: int, index_dst: int, service: int, true_src: str, trans_addr: str) -> list:
        """
        Process flow trought the State machine.
        One OPF_NORMAL is issued, the same flow will no longer come up in this function???
        Args:
            index_src: my_index
            index_dst: ip dst
            service:
            true_src: ip src
            trans_addr: pkt's transmitter address

        Returns: list of strings

        """

        assert self.routes.flow_exists(true_src, index_dst, service), "Flow does not found on routing Table!"
        assert self.net.is_same_dev(index_src, index_dst) == False, "ERROR, same device, should be sent as NORMAL!"

        f_key = self.routes.get_flow_key(true_src, index_dst, service)
        for i in range(0, len(self.routes.flows[f_key])):
            logger.debug(" %d : %s --> %s", i, self.routes.flows[f_key][i].path , self.routes.flows[f_key][i].state)


        assert self.routes.flows[f_key][0].path[-1] != index_src, 'It is destination, it should not be here! ({}) - 0:{} {} {}'.format(
            len(self.routes.flows[f_key]), self.routes.flows[f_key][1].path, self.routes.flows[f_key][0].path[-1], index_src)

        _cost, path_from_here = None, None
        trans_addr_idx = self.net.exists(None, None, None, trans_addr)
        path_idx = None

        # get the path in use for this node
        for i in range(0, len(self.routes.flows[f_key])):
            if self.routes.flows[f_key][i].state != PathState.BROKEN and index_src in self.routes.flows[f_key][i].path:
                path_idx = i
                logger.info("Node found in Path %d: %s", path_idx, self.routes.flows[f_key][path_idx].path)
                break

        match_trans = ''
        # if flow_mod had no time to reach before packet_in, send packet_out
        try:
            if path_idx:
                p = self.routes.flows[f_key][path_idx]
                logger.debug("%s , %s, %s, %s", p.path, index_src, p.path.index(index_src), p.transmitter)
                (dpid, port, n_mac, match_trans) = p.get_packet_out(self.net, node_idx=p.path.index(index_src), trans_addr=p.transmitter)
                logger.debug("Path: (graph idx) %s -> trans addr for node %d : (mac) %s . arrived trans addr : (mac) %s",
                             p.path,index_src,  match_trans , trans_addr_idx)

                # ------ NO DEVIATION ------
                if match_trans == trans_addr:
                    # same rule applies
                    orders = [(port, n_mac)]
                    logger.debug("No DEVIATION found! --> sending Packet_out %s", str(orders))
                    return self.serialize_OPF_rules(orders)

                # ---- DEVIATION FOUND -----
                else:
                    logger.debug("DEVIATION occour! ")
                    if self.net.G.nodes[trans_addr_idx]['isOF']:
                        #logger.debug("f_key: %s, OG_path: %s | idx_src %s , TA addr in path: %s , TA _real: %s", f_key,p.path, index_src,  match_trans, trans_addr)
                        raise Exception('Transmitter Node cannot be SDN! - f_key: {}, OG_path: {} | idx_src {} , TA addr in path: {} , TA _real: {}'.format( f_key,p.path, index_src,  match_trans, trans_addr))
                    else:
                        logger.debug("Transmission node is legacy! (graph idx) %d - (mac) %s", trans_addr_idx, trans_addr)
                        # prevent loops, exclude the former transmission address
                        idx_node_in_path = self.routes.flows[f_key][path_idx].path.index(index_src)
                        if idx_node_in_path == 0:
                            raise Exception('deviation occour in the first sdn, cannot exclude the sender of the packet!')

                        # ---- get path excluding TA ----
                        og_trans_addr_idx = self.net.exists(None, None, None, match_trans)
                        logger.debug("Getting path excluding %d", og_trans_addr_idx)
                        _cost, path_from_og = pc.get_best_path(self.routes.flows[f_key][path_idx].sdn_node_req ,
                                                               index_dst, self.thetas[service],
                                                                 self.net.G,self.routes ,
                                                               exclude=[og_trans_addr_idx])


                        if len(path_from_og ) >1: # usable
                            logger.debug(" PATH from origin found : updating to new path %s", path_from_og)
                            new_path = Path(_cost, path_from_og)
                            # TODO path needs to include this node
                            # TODO rate limiter will have errors
                            new_path.sdn_node_req = self.routes.flows[f_key][path_idx].sdn_node_req
                            new_path.transmitter = self.routes.flows[f_key][path_idx].transmitter
                            del self.routes.flows[f_key][path_idx] # remove path_idx
                            self.routes.insert_path(f_key, index_src, new_path) # insert new path
                            orders = self.get_orders_for_path(f_key, new_path)
                            if new_path.usable_path():
                            # update orders with rate adjusting flows if needed
                                orders = self.update_orders_rate_limiter(f_key, path_from_og, orders)

                            return self.serialize_OPF_rules(orders)
                            #raise Exception("Not implemented, usable alternative loop prevention path found!")
                        # --------- NO PATH FOUND --------
                        else:
                            # delete nodes from this index on old path
                            self.routes.flows[f_key][path_idx].path = self.routes.flows[f_key][path_idx].path[:idx_node_in_path]
                            _cost, path_from_here = pc.get_best_path(index_src, index_dst,
                                                                     self.thetas[service],
                                                                     self.net.G, self.routes)
                            # jump to tag A1



            # ------ NOT IN PATH -----------
            else: # if path_idx is None:
                # new node not contemplated in the previous path
                _cost, path_from_here = pc.get_best_path(index_src, index_dst, self.thetas[service],
                                                         self.net.G, self.routes)
        except Exception as inst:
            # print(type(inst))  # the exception type
            # print(inst.args)  # arguments stored in .args
            print(inst)  # __str__ allows args to be printed directly,
            traceback.print_exc()
            sys.stderr.flush()
            exit(-1)

        # TAG: A1
        new_path = Path(_cost, path_from_here)
        new_path.sdn_node_req = index_src
        new_path.transmitter = trans_addr

        self.routes.insert_path(f_key, index_src, new_path)

        # Packet_out + Flow Mod
        orders = self.get_orders_for_path(f_key, new_path)
        if new_path.usable_path():
            # update orders with rate adjusting flows if needed
            orders = self.update_orders_rate_limiter(f_key, path_from_here, orders)

        return self.serialize_OPF_rules(orders)



    # ------------------------------------
    # ------ CALLED by: Experimenter parser --------
    def periodic_recalculation(self, priority_level=10):
        logger.info("Triggering periodic recalculation from experimenter tick")
        ## check what keys need to be recalculated based on priority level
        filtered_f_keys = [
            f_key for f_key in self.routes.flows.keys()
            if f_key.priority <= RECALCULATION_LEVEL ]

        ## pseudo graph with bw updates
        H = self.get_graph_without_flows(filtered_f_keys)

        ## recalculate
        affected_f_keys = sort_by_priority(filtered_f_keys)
        orders = []
        for fk in affected_f_keys:
            trans, req = self.get_transmitter_requester(fk)
            orders += self.get_path_calculation_after_break(fk, H, trans, req)

            # update avbw for the next recalculation
            l = self.routes.flows[fk][0].path
            if len(l) > 1:  # there is a path
                update_node_avbw_for_flow(H, path = l, flow_bw = fk.get_dataRate())


        ## return flow mods, and meter mods
        paths_str = self.serialize_OPF_rules(orders, flow_mods_only=True)
        logger.debug("RULES %s", paths_str)
        paths_str = bytes(paths_str, 'utf-8')
        return paths_str


    def topology_recalculation(self, f_keys):
        logger.info("Triggering recalculation from topology change")
        ## pseudo graph with bw updates
        H = self.get_graph_without_flows(f_keys)

        ## recalculate
        affected_f_keys = sort_by_priority(f_keys)
        orders = []
        for fk in affected_f_keys:
            trans, req = self.get_transmitter_requester(fk)
            orders += self.get_path_calculation_after_break(fk, H, trans, req)

            # update avbw for the next recalculation
            l = self.routes.flows[fk][0].path
            if len(l) > 1:  # there is a path
                update_node_avbw_for_flow(H, path=l, flow_bw=fk.get_dataRate())

        ## return flow mods, and meter mods
        paths_str = self.serialize_OPF_rules(orders, flow_mods_only=True)
        logger.debug("RULES %s", paths_str)
        paths_str = bytes(paths_str, 'utf-8')
        return paths_str


    def get_graph_without_flows( self, flows_to_remove):
        """
            Return a new graph where 'avbw' is recalculated as if certain flows were removed.

            Parameters:
                net            : Your net_view object with net.G (NetworkX graph)
                flows          : list of flows
                flows_to_remove: set or list of flow IDs to "remove" in the calculation

            Returns:
                nx.Graph : Copy of net.G with updated 'avbw' values
            """
        net = self.net
        flows = self.routes.flows

        # Make a deep copy so we don't modify the original graph
        G_copy = copy.deepcopy(net.G)

        # Initialize each node's avbw to its max capacity
        reset_avbw_to_maxC(G_copy)

        # Subtract bandwidth usage from remaining flows
        for flow_key in flows.keys():
            if flow_key in flows_to_remove:
                continue  # skip pseudo-deleted flows
            # update remaining avbw
            update_node_avbw_for_flow(G_copy, path = flows[flow_key][0].path, flow_bw = flow_key.get_dataRate())

        return G_copy



    def get_path_calculation_after_break(self, f_key, H, trans, req):
        logger.debug("get_path_calculation_after_break %s", f_key)
        assert f_key in self.routes.flows.keys(), 'Path Recalculation cannot be used!'
        assert trans is not None and req is not None , 'Recalculating after break is from origin, it needs the paths\' transmitter and sdn_node_req'

        index_src = req #self.routes.flows[f_key][0].sdn_node_req
        index_dst = f_key.ip_dst
        service = f_key.service

        path_cost, path_list = pc.get_best_path(index_src, index_dst, self.thetas[service], H, self.routes)
        logger.debug("path after break %s", path_list)
        P = Path(path_cost, path_list)
        P.sdn_node_req = req
        P.transmitter = trans

        # check if path goes from HAS_PATH to LEGACY
        bool_has_one_usable_path = False
        for p in self.routes.flows[f_key]:
            if p.usable_path():
                bool_has_one_usable_path = True
                break

        if bool_has_one_usable_path and not P.usable_path() : # had Path and now is legacy
            # get flow mods for all sdn capable devices in old path
            orders = self.get_legacy_orders_for_path(f_key, self.routes.flows[f_key][0])
            logger.debug("%s : %s", self.routes.flows[f_key][0].path, orders)
            self.routes.flows[f_key].clear() # clear old paths
            self.routes.insert_path(f_key, index_src, P) # insert new path

            return orders
            # return them
            #raise Exception("Sorry, not implemented! (path went from HAS_PATH -> BROKEN -> LEGACY)")

        # ---- delete all Paths ----
        self.routes.flows[f_key].clear()

        self.routes.insert_path(f_key, index_src, P)  # NOT_in_USE FLAG and update transmitter and sdn_node_req



        orders = self.get_orders_for_path(f_key, self.routes.flows[f_key][0])

        if self.routes.flows[f_key][0].state == PathState.HAS_PATH:
            # update orders with rate adjusting flows if needed
            orders = self.update_orders_rate_limiter(f_key, path_list, orders, H)

        return orders[1::]  # all the FLOW_MODS


    def serialize_OPF_rules(self, orders, flow_mods_only=False):
        """
        serialize orders to pass to the controller
        | divides orders
        <dpId> & <OPF_rule> divides who sends the OPF rule
        string ends with |
        Args:
            orders: list of orders (<node_vertex>, <str>)

        Returns: str
        """
        out_str = ''
        if not flow_mods_only:
            out_str += str(orders[0][0]) + '&' + str(orders[0][1]) + '|'  # packet out

        for (n, rule) in orders[1::]:
            out_str += str(self.net.G.nodes[n]['datapath_id']) + '&' + rule + '|'

        return out_str

    def get_transmitter_requester(self, fk: FlowKey) -> tuple:
        for i in reversed(self.routes.flows[fk]):
            logger.debug(" %s -> %s : %s %s", i.path , i.state, i.transmitter, i.sdn_node_req)
            if i.state != PathState.BROKEN and i.transmitter is not None:
                return (i.transmitter, i.sdn_node_req)
        if (self.routes.flows[fk][0].transmitter is not None):
            return (self.routes.flows[fk][0].transmitter, self.routes.flows[fk][0].sdn_node_req)

        raise Exception("Cannot find a valid transmitter and sdn_node_req for this flow key!")
