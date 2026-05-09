import netaddr
import struct
import logging
logger_exp = logging.getLogger()

log_filename= '/home/phd/Documents/PhD/ns-3-dev/output/scenario3/avg_load.log'

def norm(value, in_min=0, in_max=100):
    if value > in_max:
        return 1
    elif value < in_min:
        return 0

    return (value-in_min)/(in_max - in_min)

OPTIMIZATION_ENABLE = False

def parse_message(buf, dpid, net, routes = None):
    """
    Parses the exp_message. It updates the nodes in the graph
    :param buf:
    :param dpid:
    :param net:
    :return list_updates: A list of link updates that need to be made
    """
    # power,[port,mac,ip [neig ip, values, ...], ...]

    list_updates = []
    PACK_STR = '<iBI4s6sII'
    (power,) = struct.unpack_from('B', buf)
    #print('power', power)
    same_dev = []
    buf = buf[1:]
    while buf:  # FOR EACH PORT
        # print('len', len(buf), flush=True)
        (length, type, port_n, _ipv4, mac, avbw,maxC) = struct.unpack_from(PACK_STR, buf)
        # because is it little indian and it is interpreted as big
        # port_n = c_uint32_to_int(port_n)
        mac = addrconv_mac.bin_to_text(mac)
        _ipv4 = addrconv_ipv4.bin_to_text(_ipv4)
        if (dpid == 1):
            logger_exp.debug(" dpID 1 avbw: %d", avbw)
        #avbw = c_uint32_to_int(avbw)
        my_index = net.update_node(dpid, port_n, _ipv4, mac)
        net.G.nodes[my_index]['energy'] = power  # update energy of node
        net.G.nodes[my_index]['avbw'] = avbw  # update avbw of node
        net.G.nodes[my_index]['type'] = type #update type of interface
        net.G.nodes[my_index]['max_C'] = maxC

        assert avbw>0 , 'avbw is ZERO!'
        same_dev.append(my_index)

        with open(log_filename, "a") as f:
            # ipv4, avbw, t_idle, % ocupied, mbs que está a usar, n_routes a passar por aqui
            r = routes.get_nIdx_to_fkey([my_index])
            used_bw = maxC - avbw if maxC >0 else -1 #((avbw * (100-t_idle))/t_idle) if t_idle > 0 else avbw
            f.write("{},{},{},{}\n".format(_ipv4, avbw, maxC, max(0, maxC), used_bw ,len(r.get(my_index,[]))))

        # print((length, type, port_n, _ipv4, mac), flush=True)
        #print("My node graph index is" , my_index) #(1 + 4+4+6 + 1+4)

        inner_buf = buf[27:length]

        while inner_buf:  # FOR EACH NEIGH
            # ipv4 (4) + values (4+4+4+1)
            #print('len', len(inner_buf))
            assert len(inner_buf) // 17, "len {}".format(len(inner_buf))
            # (ipv4, value, value, value, value)
            (ip, ) = struct.unpack_from('!4s', inner_buf)
            (val1, val2, val3, val4) = struct.unpack_from(
                '<iiiB', inner_buf[4:])


            # assert val1 + val2+ val3+ val4 ==0 , "Values are all zero!" # TODO continue or do something about it
            # assert val1 == 0, 'Delivery is zero!'
            # assert val2>0.0 , 'Delay is zero, which is impossible because olsr packets are always being sent!'

            # if val1 + val2+ val3+ val4 !=0: # TODO ver isto: if not zero update, else something is wrong!

            # \in [0,100] --> \in [0,1] --> normalization
            val1 = val1 / 100
            val2 = norm(val2, in_min=0, in_max=10**6)  # [10^(-6), 1] Seconds
            val3 = val3 / 100
            val4 = val4 / 100
            #print('\t\n\t vals: ', val1, val2, val3, val4, '\n')


            ip = addrconv_ipv4.bin_to_text(ip)
            node_index = net.update_node(None, None, ip, None)
            if net.G.nodes[node_index]['type'] == None:
                net.G.nodes[node_index]['type'] = type
            list_updates += [(my_index, node_index, val1, val2, val3, val4)]

            inner_buf = inner_buf[17:]

        buf = buf[length:]

    # create edges for the same sdn devs
    attr = {'delivery': 1, 'delay': 0, 'throughput': 1,
            'link_stability': 1, 'snr': 1}
    for i in range(0, len(same_dev)-1):
        for k in range(i+1, len(same_dev)):
            net.G.add_edge(same_dev[i], same_dev[k], **attr)

    return list_updates

def update_graph_periodically(dpid, list_update, net):
    """
    Updates links and neighbours based on the exp_message.
    It also updates the tick's count on each link (link stability
    :return: True | False if changes occoured
    """

    old_edges = net.get_neigh_edges_from(dpid) # to not insert edges for the same sdn
    logger_exp.debug("OLD EDGES: %s", str(old_edges))
    # TODO if Throughput (val 3) or Delay (val 2) is zero, then the flow is not active, it didn't received any olsr messages
    for (my_index, node_index, val1, val2, val3, val4) in list_update:
        assert 0 <= val1 <= 1 and 0 <= val2 <= 1 and 0 <= val3 <= 1 and 0 <= val4 <= 1, \
            "Values are not normalized! [0,1]!({},{},{},{})".format(
                val1, val2, val3, val4)

        if (my_index, node_index) not in net.G.edges:
            # try:
            net.add_edge(my_index, node_index, (val1, val2, val3, 1, val4))
            # except Exception as e:
            #    print(e)

        else:
            if OPTIMIZATION_ENABLE:
                net.G.edges[(my_index, node_index)]['delivery'] = val1
                net.G.edges[(my_index, node_index)]['throughput'] = val3

                if val2 == 0 and val3 == 0:
                    # delay is zero
                    logger_exp.debug("Link (%s (%s) -> %s (%s)) is not active (zero OLSR packets), but is still in OLSR tables! Values are all zero.",
                                     my_index, net.G.nodes[my_index]['ipv4_addr'], node_index, net.G.nodes[node_index]['ipv4_addr'])

                    net.G.edges[(my_index, node_index)]['snr'] = 0
                    net.G.edges[(my_index, node_index)]['delay'] = 1
                    # net.G.edges[(my_index, node_index)]['link_stability'] nao altera
                else:

                    # net.G.edges[(my_index, node_index)]['delivery'] = val1
                    net.G.edges[(my_index, node_index)]['delay'] = val2
                    # net.G.edges[(my_index, node_index)]['throughput'] = val3
            # SNR
            if val4==0:
                logger_exp.debug(
                    "Link (%s (%s) -> %s (%s)) is not active (zero OLSR packets), but is still in OLSR tables! Values are all zero.",
                    my_index, net.G.nodes[my_index]['ipv4_addr'], node_index, net.G.nodes[node_index]['ipv4_addr'])
                logger_exp.error("Something is wrong: packet was not received??")
                net.G.edges[(my_index, node_index)]['link_stability'] = max(0,
                                                                            net.G.edges[(my_index, node_index)]['link_stability']-1)  # ticks
            else:
                net.G.edges[(my_index, node_index)]['snr'] = val4

            # Link Satbility (Note: para o mesmo link entre dois sdn devs vai somar duplamente em relaçao aos links que sejam leg-sdn)
            if net.G.nodes[my_index]['isOF'] and net.G.nodes[node_index]['isOF']:
                net.G.edges[(my_index, node_index)
                            ]['link_stability'] += 0.5  # ticks
            else:
                net.G.edges[(my_index, node_index)
                            ]['link_stability'] += 1  # ticks


            # net.G.edges[(my_index, node_index)]['snr'] = val4
            if (my_index, node_index) in old_edges:
                old_edges.remove((my_index, node_index))

    # remove old edges that were not updated
    net.G.remove_edges_from(old_edges)

    # changes always occoured if list_update has elements
    # return len(list_update) > 0
    return old_edges



def update_graph(dpid, list_update, net):
    """
    Updates links and neighbours based on the exp_message.
    It also updates the tick's count on each link (link stability
    :return: True | False if changes occoured
    """
    old_edges = net.get_neigh_edges_from(dpid)
    # TODO if Throughput (val 3) or Delay (val 2) is zero, then the flow is not active, it didn't received any olsr messages
    for (my_index, node_index, val1, val2, val3, val4) in list_update:
        assert 0 <= val1 <= 1 and 0 <= val2 <= 1 and 0 <= val3 <= 1 and 0 <= val4 <= 1, \
            "Values are not normalized! [0,1]!({},{},{},{})".format(
                val1, val2, val3, val4)

        if (my_index, node_index) not in net.G.edges:
            # try:
            net.add_edge(my_index, node_index, (val1, val2, val3, 1, val4))
            # except Exception as e:
            #    print(e)

        else:
            if OPTIMIZATION_ENABLE:
                net.G.edges[(my_index, node_index)]['delivery'] = val1
                net.G.edges[(my_index, node_index)]['throughput'] = val3

                if val2 == 0 and val3 == 0:
                    # delay is zero
                    logger_exp.debug("Link (%s (%s) -> %s (%s)) is not active (zero OLSR packets), but is still in OLSR tables! Values are all zero.",
                                     my_index, net.G.nodes[my_index]['ipv4_addr'], node_index, net.G.nodes[node_index]['ipv4_addr'])

                    net.G.edges[(my_index, node_index)]['snr'] = 0
                    net.G.edges[(my_index, node_index)]['delay'] = 1
                    # net.G.edges[(my_index, node_index)]['link_stability'] nao altera
                else:

                    # net.G.edges[(my_index, node_index)]['delivery'] = val1
                    net.G.edges[(my_index, node_index)]['delay'] = val2
                    # net.G.edges[(my_index, node_index)]['throughput'] = val3
            # SNR
            if val4 == 0:
                logger_exp.debug(
                    "Link (%s (%s) -> %s (%s)) is not active (zero OLSR packets), but is still in OLSR tables! Values are all zero.",
                    my_index, net.G.nodes[my_index]['ipv4_addr'], node_index, net.G.nodes[node_index]['ipv4_addr'])
                logger_exp.error("Something is wrong: packet was not received??")
                net.G.edges[(my_index, node_index)]['link_stability'] = max(0,
                                                                            net.G.edges[(my_index, node_index)][
                                                                                'link_stability'] - 1)  # ticks
            else:
                net.G.edges[(my_index, node_index)]['snr'] = val4

            # net.G.edges[(my_index, node_index)]['snr'] = val4
            if (my_index, node_index) in old_edges:
                old_edges.remove((my_index, node_index))

    # remove old edges that were not updated
    net.G.remove_edges_from(old_edges)

    # changes always occoured if list_update has elements
    #return len(list_update) > 0
    return old_edges

def recalculate_paths_without_spread(datapath, parser, net, prio):
    logger_exp.debug("Recalculate Paths!...")
    flow_mods = []  # --> (match, actions, idle_timeout, prio)
    #    self.net.routes --> {dpid: {i_dst: (next_mac, outPort, match}}
    # i can only send flow mods to me without knowing the others datapath
    for key in net.routes[datapath.id].keys():
        my_index = net.exists(datapath.id, None, None, None)
        (next_mac, out_port) = net.get_path(my_index, key)
        if next_mac is not None and next_mac != net.routes[datapath.id][key][0]:
            # 4. paths changed -> send flow_mods
            actions = [parser.OFPActionSetField(
                eth_dst=next_mac), parser.OFPActionOutput(out_port)]
            match_tmp = net.routes[datapath.id][key][2]
            flow_mods += [(match_tmp, actions, 10, prio)]
            # self.add_flow(datapath, prio, match_tmp, actions, idle_timeout=10, hard_timeout=20)
            # update self.routes
            net.routes[datapath.id][key] = (
                next_mac, out_port, match_tmp, prio)
            prio += 1
    return flow_mods


# --------- UTILS ---------------
def c_uint32_to_int(p):
    """
    parse uint32_t in little indian, that ryu parsed as big indian
    :param p: int
    :return: int
    """
    d = p.to_bytes(4, 'big')  # big indian (because it was parsed as that)
    (x,) = struct.unpack_from('I', d)  # return as little indian
    return x


class AddressConverter(object):
    def __init__(self, addr, strat, fallback=None, **kwargs):
        self._addr = addr
        self._strat = strat
        self._fallback = fallback
        self._addr_kwargs = kwargs

    def text_to_bin(self, text):
        try:
            return self._addr(text, **self._addr_kwargs).packed
        except Exception as e:
            if self._fallback is None:
                raise e

            # text_to_bin is expected to return binary string under
            # normal circumstances. See ofproto.oxx_fields._from_user.
            ip = self._fallback(text, **self._addr_kwargs)
            return ip.ip.packed, ip.netmask.packed

    def bin_to_text(self, bin):
        return str(self._addr(self._strat.packed_to_int(bin),
                              **self._addr_kwargs))


addrconv_ipv4 = AddressConverter(netaddr.IPAddress, netaddr.strategy.ipv4,
                                 fallback=netaddr.IPNetwork, version=4)


class mac_mydialect(netaddr.mac_unix):
    word_fmt = '%.2x'


addrconv_mac = AddressConverter(netaddr.EUI, netaddr.strategy.eui48, version=48,
                                dialect=mac_mydialect)
