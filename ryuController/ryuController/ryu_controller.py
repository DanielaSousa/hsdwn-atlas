from csv import excel
import copy
import threading
lock = threading.Lock()
# import os
# print(os.getcwd()) # running tests: /home/phd/Documents/PhD/ns-3-dev


# import sys
# sys.path.insert(
#     0, '/home/phd/Documents/PhD/ns-3-dev/build/bindings/python/ryuController/')

import ryuController.net_view as n
import ryuController.exp_msg as exp
from ryuController.utils import *
from ryuController.routing import *
from ryuController.paths import PathState, FlowKey
from ryuController.routing_utils import sort_by_priority
import ryuController.config_manager as config
# -------------------------------

# import net_view as n
# import exp_msg as exp
# from utils import *
# from routing import *
# from paths import PathState
# from routing_utils import sort_by_priority
# # ----------------------------------------------------
import pickle
import traceback
import json

started = False
rm = None

import logging
import sys
logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.DEBUG)
#                     format='[%(levelname)s] (%(shared_value)s) %(message)s',
#                     stream=sys.stdout) # logger.setLevel(logging.DEBUG) #
# #logger.addHandler(logging.StreamHandler(sys.stdout))
# logging.getLogger().addFilter(GlobalValueFilter())


# At top-level in exp_msg.py
exp_msg_counters = {}  # key: datapath_id, value: count since last recalculation
EXP_MSG_THRESHOLD = 5  # number of messages before triggering periodic recalculation

sce_routing_vars = {}

def get_config_for_time(ns3_time: float):
    """
    Find the latest configuration <= ns3_time.
    If exact timestamp not found, use the closest past one.
    """
    times = sorted(float(t) for t in sce_routing_vars.keys())
    chosen_time = max([t for t in times if t == ns3_time], default=None)
    if chosen_time is None:
        return None
    return sce_routing_vars[str(chosen_time)]



def receive_clock(ns3_time):
    logger.debug(f"[Python] Received NS-3 clock: {ns3_time:.6f} s")
    try:
        s_conf = get_config_for_time(ns3_time)
        if s_conf is None:
            # print("No config found for this timestamp")
            return

        global rm

        # --- Update rm.thetas ---
        if "theta" in s_conf.keys():
            theta_from_json = {int(k): v for k, v in s_conf["theta"].items()}
            rm.thetas.update(theta_from_json)
            logger.info("Updated theta: %s", str(rm.thetas))

        # --- Update rm.routes.routing_variables ---
        if "gama" in s_conf.keys():
            rm.routes.routing_variables["gama"] = s_conf["gama"]
            logger.info("Updated gama: %s", str(rm.routes.routing_variables["gama"]))

        # Convert tech_costs into {type: value}
        if "tech_costs" in s_conf.keys():
            tech_cost = {v["type"]: v["value"] for v in s_conf["tech_costs"].values()}
            rm.routes.routing_variables["tech_cost"].update(tech_cost)
            logger.info("Updated tech costs: %s", str(rm.routes.routing_variables["tech_cost"]))


        if "flows" in s_conf:
            for key_str, priority in s_conf["flows"].items():
                ip_src, ip_dst, port = key_str.split(",")
                port = int(port)
                config.update_flow(ip_src, ip_dst, port, priority)
                logger.info("Update config %s", str((ip_src, ip_dst, port, priority)))

                # check if flow already exists and updare priority on the flowKey
                if (not started):
                    continue

                f_key = rm.routes.get_flow_key(ip_src, ip_dst, port)
                if f_key:
                    f_key.priority = priority
                    logger.info("Update flow %s", f_key)

    except Exception as e:
        print(e)
        traceback.print_exc()
        #return -1 # Fail




import os
def start(config_path):
    logger.debug("------- Config controller started: %s", config_path)
    W = {}
    if config_path[-1] != '/':
        config_path = config_path + "/"
    config_services = config_path + "configs/service_types.yaml"
    path_routing_vars = config_path + "configs/scenario.json"

    if not os.path.exists(path_routing_vars) :
        logger.error("Config files NOT FOUND: " + path_routing_vars)
        return -1 # Fail
    if not os.path.exists(config_services):
        logger.error("Config files NOT FOUND: " + config_services)
        return -1  # Fail


    # Load config file once at module import
    global sce_routing_vars
    try:
        with open(path_routing_vars, "r") as f:
            sce_routing_vars = json.load(f)
            logger.debug("File !!")
        #if config_path == "" :



        cfg = config.load_config(config_services)
        #print(cfg)
        for service in cfg.get('service_types', []):
            # print(
            #     f"  Name: {service['name']}, Port: {service['port']}, Priority: {service['priority']}, Bandwidth: {service['bandwidth']}, Weights: {service['link_weights']}")
            W[service['port']] = service['link_weights']
            assert len(service['link_weights']) == 7, "Not enough weights!"

    except Exception as e:
        print(e)
        traceback.print_exc()
        sys.stderr.flush()
        return -1 # Fail

        # self.weights = [0.142, 0.142, 0.142, 0.142, 0.142, 0.142, 0.142]


    # else:
    #     W[S100] =  [0.184, 0.078, 0.196, 0.173, 0.116, 0.158, 0.091]
    #     W[S250] =  [0.130, 0.168, 0.187, 0.138, 0.117, 0.139, 0.126]
    #     W[S450] =  [0.211, 0.116, 0.176, 0.102, 0.145, 0.086, 0.172]
    #     W[S1000] = [0.141, 0.168, 0.128, 0.120, 0.155, 0.130, 0.162]
    #     W[S2000] = [0.145, 0.186, 0.125, 0.140, 0.138, 0.135, 0.129]

    logger.debug("Theta weights:", W)

    # try:
    #     with open('/home/phd/Documents/PhD/ns-3-dev/src/ryuController/ryuController/configs.json', 'r') as file:
    #         data = json.load(file)
    # except Exception as e:
    #     print(e)
    # finally:
    #     # Print the data
    #     print(data)


    global rm
    rm = RoutingMechanism(W)
    global started
    started = True

    return 0 # success


def parse_idle_flow_remove(index_dst, service, index_ip_src):
    """
    remove flow if idle time reached!
    Args:Flow key:
        index_dst:
        service:
        index_ip_src:
    """
    #OPF remove msg says if it is from idle or hardtime
    pass # TODO

def process_packet_in(index_src, index_dst, service, index_ip_src, trans_addr):
    """
    Process what to do with the flow
    1. Gets Path if flow does not exist
    2. Evaluates flow state machine
    Args:
        index_ip_src, index_dst, service: flow_key
        index_src: who is requesting the path (src of the path)
        trans_addr: mac transmissior address
        service: udp source port, as PoC, but should be ToS
    Returns: string with orders
    """
    global rm
    with lock:  # Ensures only one function runs at a time
        # try:
        #     with open('debug.pickle', 'wb') as handle:
        #         pickle.dump(rm, handle, protocol=pickle.HIGHEST_PROTOCOL)
        # except Exception as inst:
        #     print(type(inst))  # the exception type
        #     print(inst.args)  # arguments stored in .args
        #     print(inst)  # __str__ allows args to be printed directly,
        #     # but may be overridden in exception subclasses
        logger.debug("\n --> process_packet_in %s",(index_src, index_dst, service, index_ip_src, trans_addr) )
        assert started, "NOT started!"
        assert rm is not None, 'Routing mechanism is not started!'

        try:
            if rm.routes.flow_exists(index_ip_src, index_dst, service) and rm.routes.flows[rm.routes.get_flow_key(index_ip_src, index_dst, service)][0].state != PathState.LEGACY:
                logger.debug("\t Flow Exists! index_src %d, index_dst %d", index_src, index_dst)
                # paths_str = rm.process_flow(index_src, index_dst, service, index_ip_src, trans_addr)
                paths_str = rm.recalculate_flow(index_src, index_dst, service, index_ip_src, trans_addr)
            else:
                #return get_path(index_src, index_dst, service, index_ip_src) # START state
                logger.info("\t PACKET_IN - get_path index_src %d, index_dst %d", index_src, index_dst)
                paths_str = rm.path_calculation(index_src, index_dst, service, index_ip_src,trans_addr, rm.net.G)
        except Exception as inst:
            # print(type(inst))  # the exception type
            # print(inst.args)  # arguments stored in .args
            print(inst)  # __str__ allows args to be printed directly,
            traceback.print_exc()
            sys.stderr.flush()
            print("DONE", flush=True)
            exit(-1)

        logger.info("RULES:%d %s", len(paths_str), paths_str)
        paths_str = bytes(paths_str, 'utf-8')
        #logger.info("RULES: %d", len(paths_str))
        return paths_str

def process_hard_timeout(index_src, index_dst, service, index_ip_src):
    logger.debug("[Function] process_hard_timeout")
    with lock:  # Ensures only one function runs at a time
        try:
            logger.debug("%s %s", [str(i) for i in rm.routes.flows.keys()], (index_ip_src, index_dst, service))
            f_key = rm.routes.get_flow_key(index_ip_src, index_dst, service)

            if not f_key:
                logger.info("Key not found %s", str((index_ip_src, index_dst, service)))
                return 0
            rm.routes.del_key(f_key)

            # # ------ Para que e isto???
            # dpid_idx = index_src
            # possible_dpid = [dpid_idx]
            # for i in list(rm.net.G.neighbors(dpid_idx)):
            #     if rm.net.is_same_dev(i, dpid_idx):
            #         possible_dpid.append(i)
            # logger.info("process_hard_timeout sw %s: %s", str(possible_dpid), str((index_src, index_dst, service, index_ip_src)) )
            # for p in rm.routes.flows[f_key]:
            #     if p.state == PathState.BROKEN:
            #         #delete
            #         continue
            #     else:
            #         if p.path[0] in possible_dpid:
            #             #delete
            #             p.state = PathState.BROKEN
            #
            # if rm.routes.flows[f_key][0].state == PathState.BROKEN :
            #     # delete key
            #     rm.routes.flows.pop(f_key, None)
            #     logger.info("Deleted key %s", str(f_key))
            # else:
            #     #clean all BROKEN paths
            #     not_deleted_paths = []
            #     for p in rm.routes.flows[f_key]:
            #         logger.info("Deleted path %s", str(p.path))
            #         if p.state != PathState.BROKEN:
            #             not_deleted_paths.append(p)
            #
            #     rm.routes.flows[f_key] = not_deleted_paths

            logger.info(" --- END process_hard_timeout")
        except Exception as inst:
            # print(type(inst))  # the exception type
            # print(inst.args)  # arguments stored in .args
            print(inst)  # __str__ allows args to be printed directly,
            traceback.print_exc()
            sys.stderr.flush()
            exit(-1)
        return 0

def process_idle_timeout(index_src, index_dst, service, index_ip_src):
    logger.debug("[Function] process_idle_timeout")
    with lock:  # Ensures only one function runs at a time
        try:
            logger.debug("%s %s", [str(i) for i in rm.routes.flows.keys()], (index_ip_src, index_dst, service))
            f_key = rm.routes.get_flow_key(index_ip_src, index_dst, service)
            dpid_idx = index_src
            if not f_key:
                logger.info("Key not found %s", str((index_ip_src, index_dst, service)))
                return -1
            rm.routes.delete_path_req(f_key, index_src)
            logger.info(" --- END process_idle_timeout sw %d: %s", dpid_idx, str((index_src, index_dst, service, index_ip_src)))

        except Exception as inst:
            # print(type(inst))  # the exception type
            # print(inst.args)  # arguments stored in .args
            print(inst)  # __str__ allows args to be printed directly,
            traceback.print_exc()
            exit(-1)
        return 0

# ------------------------------------------------------
# --------------- EXPERIMENTER PARSER ------------------ DONE
def parse_experimenter_message(buf, dpId, periodically):
    # print("\t [PYTHON] parse_message", len(buf), dpId)
    """Parses the exp_message. It updates the nodes in the graph

    Args:
        buf (string): payload of Experimenter message
        dpId (int): datapath id of the sender node

    Returns:
        str: FLOW_MODS in string format
    """
    global rm
    global exp_msg_counters
    try:
        with lock:  # Ensures only one function runs at a time
            assert started, "NOT started!"
            logger.info(" ---- Experimenter message from dpId %d", dpId)
            deleted_edges = -1
            # 1. parse_message
            assert isinstance(buf, bytes) or isinstance(
                buf, str), ("Input variable [buf] should be string %s", buf)


            list_updates = exp.parse_message(buf, dpId, rm.net, rm.routes)
            if dpId not in exp_msg_counters.keys():
                exp_msg_counters[dpId] = 0
            # 2. Update graph
            if periodically:
                # Increment message counter
                exp_msg_counters[dpId] = exp_msg_counters.get(dpId, 0) + 1
                deleted_edges = exp.update_graph_periodically(dpId, list_updates, rm.net)
            else:
                deleted_edges = exp.update_graph(dpId, list_updates, rm.net)

            logger.debug("DELETED EDGES %s", deleted_edges)

            rm.net.add_edge_same_dev(dpId)
            affected_f_keys = []
            if deleted_edges:
                # check if there is broken paths
                affected_f_keys = rm.routes.check_broken_paths(deleted_edges)
                if(len(affected_f_keys)>0):
                    return rm.topology_recalculation(affected_f_keys)

            # Check threshold

            if exp_msg_counters[dpId] >= EXP_MSG_THRESHOLD:
                exp_msg_counters = {}  # reset
                if len(rm.routes.flows.keys()) == 0:
                    return bytes('', 'utf-8')
                return rm.periodic_recalculation()

    except Exception as inst:
        # print(type(inst))  # the exception type
        # print(inst.args)  # arguments stored in .args
        print(inst)  # __str__ allows args to be printed directly,
        traceback.print_exc()
        sys.stderr.flush()
        print("DONE")
        exit(0)

    return bytes('', 'utf-8')

# ------------------------------------------------------
# --------------------- NODE UTILS --------------------- DONE
# CODE
EXISTS = 0
UPDATE = 1


def node(code, dpid, port_no, ip, mac):
    """node

    Args:
        code (int): _description_
        dpid (int): _description_
        port_no (int): _description_
        ip (string): _description_
        mac (string): _description_

    Returns:
        int:  index not found (-1), or the index
    """
    # print("\t [PYTHON] node", code, dpid, port_no, ip, mac)
    assert started, "NOT started!"

    # parse aguments
    if dpid == -1:
        dpid = None
    if port_no == -1:
        port_no = None
    if ip == "":
        ip = None
    if mac == "":
        mac = None

    index = None
    if code == EXISTS:
        index = rm.net.exists(dpid, port_no, ip, mac)

    elif code == UPDATE:
        index = rm.net.update_node(dpid, port_no, ip, mac)

    if index != None:
        logger.info("Node info: %s--> graph index: %d", str((dpid, port_no, ip, mac)), index)
        return index
    return -1


def check_same_dev(idx_a, idx_b):
    _tmp = int(rm.net.is_same_dev(idx_a, idx_b))
    logger.info("%d same dev as %d: %d",idx_a, idx_b,_tmp )
    return _tmp
