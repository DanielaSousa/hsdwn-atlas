import yaml
import logging
logger = logging.getLogger(__name__)

RECALCULATION_LEVEL = 10 # Higher priority (lower numerical value)

config = None

def load_config(filename):
    global config
    print(filename)
    with open(filename, 'r') as file:
        config = yaml.safe_load(file)
    print(config)
    if config is None:
        logger.error("Config file not loaded! %s", filename)
        return None

    print("Service Types:")
    for service in config['service_types']:
        print(f"  Name: {service['name']}, Port: {service['port']}, Priority: {service['priority']}, Bandwidth: {service['bandwidth']*1000}, Weights: {service['link_weights']}")

    config['flows'] = [] # [{ip_src: <>, ip_dst: <>, port: <>, priority: <>}, {...}, ...]
    return config

def update_flow( ip_src, ip_dst, port, priority):
    if "flows" not in config:
        config["flows"] = []

    # search for an existing flow
    for flow in config["flows"]:
        if (flow["ip_src"] == ip_src and
            flow["ip_dst"] == ip_dst and
            flow["port"] == port):
            flow["priority"] = priority   # update priority
            return

    # if not found, append new flow
    config["flows"].append({
        "ip_src": ip_src,
        "ip_dst": ip_dst,
        "port": port,
        "priority": priority
    })


# i need priority and dataRate given by port
def get_priority_bw(ip_src, ip_dst, port):
    if config is None:
        logger.error("File not loaded!")
        return None, None

    priority = None
    bw = None
    flow_prio = False
    for flow in config.get('flows', []):
        if flow['ip_src'] == ip_src and flow['ip_dst'] == ip_dst and flow['port'] == port:
            priority = flow['priority']
            flow_prio = True
            break
        #print(f"  Src: {flow['ip_src']}, Dst: {flow['ip_dst']}, Port: {flow['port']}, Priority: {flow['priority']}")

    for service in config.get('service_types', []):
        if service['port'] == port:
            if not flow_prio:
                priority = service['priority']
            bw = service['bandwidth']*1000
            break
        #print(f"  Name: {service['name']}, Port: {service['port']}, Priority: {service['priority']}, Bandwidth: {service['bandwidth']}")
    return priority, bw

def get_dataRate_by_port(port):
    if config is None:
        logger.error("File not loaded!")
        return None
    bw = None
    for service in config.get('service_types', []):
        print(
            f"  Name: {service['name']}, Port: {service['port']}, Priority: {service['priority']}, Bandwidth: {service['bandwidth']}")
        print(type(service['port']), type(port), service['port']== port)
        if service['port'] == port:
            bw = service['bandwidth']*1000
            break
    return bw
