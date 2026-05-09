
DEFAULT_OPF_TIMEOUT =  65535

def flow_mod_match_udp(prio, eth_src, ip_src, ip_dst, udp_port, idle_timeout = DEFAULT_OPF_TIMEOUT , hard_timeout = DEFAULT_OPF_TIMEOUT):
    flowCmd = "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={}".format(idle_timeout, DEFAULT_OPF_TIMEOUT, prio)
    # match fields
    flowCmd += " eth_type=0x0800,eth_src={},ip_proto={},ip_src={},ip_dst={},udp_dst={}".format(
        eth_src,
        17,
        ip_src,
        ip_dst, udp_port)

    prio += 1
    return flowCmd

def add_meter_id(flowCmd, meter_id):
    # meters
    flowCmd += " meter:{}".format(meter_id)
    return flowCmd

def add_instructions(flowCmd, output, set_field = None):
    # instructions
    if set_field is not None:
        flowCmd += " write:set_field=eth_dst:{},output={}".format(set_field, output)
    else:
        flowCmd += " write:output={}".format(output)
    return flowCmd

def flow_mod(prio, eth_src, ip_src, ip_dst, udp_port, output, meter_id = None,set_field = None, idle_timeout = DEFAULT_OPF_TIMEOUT , hard_timeout = DEFAULT_OPF_TIMEOUT):
    # match
    #flowCmd = flow_mod_match_udp(prio, eth_src, ip_src, ip_dst, udp_port, idle_timeout , hard_timeout)
    flowCmd = flow_mod_match_udp(prio, eth_src, ip_src, ip_dst, udp_port, idle_timeout, DEFAULT_OPF_TIMEOUT)
    # meters
    if meter_id is not None:
        flowCmd = add_meter_id(flowCmd, meter_id)
    # instructions
    flowCmd = add_instructions(flowCmd, output, set_field)

    return flowCmd

def flow_mod_normal(prio, eth_src, ip_src, ip_dst, udp_port):
    # match
    #flowCmd = flow_mod_match_udp(prio, eth_src, ip_src, ip_dst, udp_port, idle_timeout , hard_timeout)
    flowCmd = flow_mod_match_udp(prio, eth_src, ip_src, ip_dst, udp_port, 5, 5)
    # instructions
    flowCmd += " apply: output = normal"

    return flowCmd

def meter_mod (meter_id, rate): # Rate in mbps
    """
    datapath=datapath,
    command=ofproto.OFPMC_ADD,
    flags=ofproto.OFPMF_KBPS,
    meter_id=meter_id,
    bands=bands

    # bands = [parser.OFPMeterBandDrop(rate=rate, burst_size=rate // 10)]
    """
    return "meter-mod cmd=add,flags=1,meter={} drop:rate={}".format(meter_id, rate/1000) # rate in Kbps

def meter_mod_modify(meter_id, rate):
    # dpctl commands {add, mod, del}
    return "meter-mod cmd=add,flags=1,meter={} drop:rate={}".format(meter_id, rate / 1000)  # rate in Kbps
    return "meter-mod cmd=mod,flags=1,meter={} drop:rate={}".format(meter_id, rate / 1000)  # rate in Kbps
    #meter-mod cmd=modify meter=1 kbps bands=type=drop,rate=2000,burst=100 # drops everything above 2000kbs with a burst of 100kbs



# def flow_mod_NORMAL(prio, ip_proto, ip_src, ip_dst, idle_timeout = DEFAULT_OPF_TIMEOUT , hard_timeout = DEFAULT_OPF_TIMEOUT):
#
#     flowCmd = "flow-mod cmd=add,table=0,idle={},hard={},flags=0x0001,prio={} eth_type=0x0800,ip_proto={},ip_src={},ip_dst={}".format(idle_timeout, hard_timeout ,prio,
#                                                                                                         ip_proto,
#                                                                                                         ip_src,
#                                                                                                         ip_dst)
#     flowCmd +=" apply:output=normal"
#     return flowCmd
#
#
#
# def serialize_orders(l):
#     return str
