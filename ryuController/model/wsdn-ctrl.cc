#include "wsdn-ctrl.h"

TypeId
WSDNController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::WSDNController")
                            .SetParent<WSDNCtrlAbstract>()
                            .SetGroupName("WSDNController")
                            .AddConstructor<WSDNController>();
    return tid;
}

WSDNController::WSDNController(std::string ryuconfig)
    : WSDNCtrlAbstract(ryuconfig)
{
}

WSDNController::~WSDNController()
{
    // Destroy multipath
}

pOutRule
WSDNController::Handle_Ipv4(uint64_t dpId,
                            uint8_t ip_proto,
                            ofl_msg_packet_in* msg,
                            Mac48Address src48,
                            Ipv4Address srcIp,
                            Mac48Address dst48,
                            Ipv4Address dstIp)
{
    std::stringstream str1;
    str1 << srcIp;

    int srcIp_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), "");
    NS_LOG_DEBUG("[Py_node]  Update entry4: index=" << srcIp_index << " (ip:" << str1.str() << ")");
    str1.clear();
    str1 << dstIp;
    int dstIp_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), "");
    NS_LOG_DEBUG("[Py_node]  Update entry5: index=" << dstIp_index << " (ip:" << str1.str() << ")");
    str1.clear();
    str1 << src48;
    int mac_ta_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, "", str1.str());
    NS_LOG_DEBUG("[Py_node]  Update entry5: index=" << mac_ta_index << " (mac:" << str1.str()
                                                    << ")");

    std::ostringstream actions, match;

    // search Path
    pOutRule pOut;
    pOut.outPort = OFPP_NORMAL;

    // match << "in_port=" << inPort << ",eth_src=" << src48 << ",eth_type=0x0800, eth_dst=" <<
    // dst48
    //       << ",ip_dst=" << dstIp << ",ip_src=" << srcIp; // needs commas

    match << "ip_dst=" << dstIp << ",ip_src=" << srcIp;

    // -- Rule 1: Destination node is unknown
    if (dstIp_index == -1)
    {
        // actions << "apply:output=normal";
        // pOut.outPort = OFPP_NORMAL;
        // NO ADD_FLOW : wait, because next time this node may be in the tables
        return pOut;
    }

    int my_index = Py_node(pModule, NODE_CODE_EXISTS, dpId, -1, "", ""); // EXISTS
    NS_LOG_DEBUG("[dpId " << dpId << "]"
                          << "[Py_node]  EXISTS entry: index=" << my_index << " (dpIp:" << dpId
                          << ")");
    int sameDev = Py_check_same_dev(pModule, my_index, dstIp_index);

    // --  Rule 2: i'm already in the destination device
    if (sameDev || (ip_proto != 17 && ip_proto != 6)) // same | not udp and not tcp
    {
        actions << "apply:output=normal";
        AddFlow(dpId, prioCounter, match, actions, NO_BUFFER); // FOREVER
        NS_LOG_INFO("[dpId " << dpId << "]"
                             << "Destinatio:" << dstIp << " is same Dev!");
        return pOut; // OFPP_NORMAL
    }

    // -- Rule 3: Get Path
    //
    NS_ASSERT_MSG(ip_proto == 17, "Library is only prepared for UDP FLows");

    // 2. path = net.get_path(my_index, dst_index)
    uint32_t src_port = Get_UDP_DST((struct ofl_match*)msg->match);

    int service;
    if (src_port < 200)
        service = 1000;
    else if (src_port < 300)
        service = 2000;
    else if (src_port < 400)
        service = 450;
    else if (src_port < 500)
        service = 250;
    else
        service = 100;

    // my_index, dst_ip, service, src_ip, pkt_mac_ta
    std::vector<std::string> res =
        Py_get_path(pModule, my_index, dstIp_index, service, srcIp_index, mac_ta_index);
    NS_ASSERT_MSG(res.size() > 0, "Python Module ERROR!");

    // if (res.size() == 0) // NO PATH FOUND!
    // {
    //     actions << "apply:output=normal";
    //     AddFlow(dpId, prioCounter, match, actions, NO_BUFFER, hard_timeout = 1); // 1 Second
    //     NS_LOG_INFO("[dpId " << dpId << "]"
    //                          << "Destinatio:" << dstIp << " is same Dev!");
    //     return pOut; // OFPP_NORMAL
    // }

    // 2. path = net.get_path(my_index, dst_index)
    // Partindo do suposto que nao há multipath
    // TODO
    DpctlExecute();
    Parse first string to pOut

        return pOut;
    /*
    // 1. check if route exists
    // NO need, if it's a no MATCH, then the rule does not exists!

    // 2. path = net.get_path(my_index, dst_index)
    FlowKey_t path_key = FlowKey_t(srcIp, dstIp, getServiceType(msg));

    //----- Divergence from primary path : TODO
    NS_ASSERT_MSG(
        m_paths.find(key) == m_paths.end(),
        "key already exists, and legacy devices didn't go along with our route"); // TODO resolve
    // {
    //     get_Path eliminating legacy devices
    //     if not possible -> rule normal and eliminate rule?
    // }
    // ----------------

    std::string ss_path = Py_get_path(pModule, my_index, dstIp_index);
    NS_LOG_DEBUG("[dpId " << dpId << "]"
                          << "\tPy_get_path (" << srcIp << " -> " << dstIp << "): "
                          << "  (" << ss_path << ")");

    std::set<Hop_t, decltype(compareKey)> hops;

    NS_LOG_INFO("[dpId " << dpId << "] Flow key = " << path_key);
    if (ss != "")
    {
        hops.ParseStringPaths(ss); // it already validates de correctness of the path
    }

    if (hops.empty())
    {
        NS_LOG_INFO("[dpId " << dpId << "] NO PATH FOUND! " << path_key);
        return pOut; // OFPP_NORMAL
    }

    // ---- Process path ----

    // it is not a multipath so each device only has one FLOWMOD
    //  solution for now to avoid loops # todo better
    //[phase 1]---- search for loop ----
    for (auto it = hops.begin(); it != hops.end(); it++)
    {
        if (it->nextMac == src48)
        {
            // In order to avoid loops, send to normal, and hopefully in the next search, a
            // better path is given
            std::cout << "LOOP, next hop is the sender of this message" << std::endl;
            NS_LOG_WARN("LOOP, next hop is the sender of this message" << src48);

            return pOut; // OFPP_NORMAL
        }
    }

    m_paths.CreateFlow(key);
    m_paths[key].match = match.str(); // general match
    m_paths[key].hops = hops;         // {(swId,nextMac,portOut,prevMac), ...}
    m_paths[key].firstDev = dpId;

    // ---- send FLOWMOD

    for (auto it = hops.begin(); it != hops.end(); it++)
    {
        // Add Flow mod and route
        actions.clear();
        actions << "write:set_field=eth_dst:" << it->nextMac << ",output=" << it->portOut;

        AddFlow(it->swId,
                prioCounter,
                match << ",eth_src=" << it->prevMac << ",eth_type=0x0800",
                actions,
                NO_BUFFER,
                10);

        // TODO -----
        // Update sw hop
        RuleInfo_t ab = {it->nextMac,
                         it->portOut,
                         prioCounter,
                         actions.str()}; // dstMac, out_port, prioCounter, actions.str()
        m_paths[key].hopSw[it->swId].out_rules.insert(ab);
        //  ----

        // variables to PacketOut
        // pOut.set_Eth_Dst = dstMac;
        // manual overwrite  (outPort)
        pOut.outPort = OFPP_NORMAL;
    }

    return pOut;
    */
}

void
WSDNController::RecalculatePathsWithoutSpread(uint64_t dpId) // int index_dst
{
    NS_LOG_FUNCTION(this << dpId);
    for (auto iter = m_paths.begin(); iter != m_paths.end(); iter++)
    {
        if (iter->second.hopSw.find(dpId))
        {
            // TODO recalculate rule iter->first
            // Get my index from python module
            int my_index = Py_node(pModule, 0, dpId, -1, "", ""); // EXISTS
            NS_LOG_DEBUG("PyNode " << dpId << " --> " << my_index);
        }
    }
}

/**
 * @brief Recalculates all paths that pass by the switch
 *
 */
void
WSDNController::RecalculatePaths()
{
    for (auto iter = m_paths.begin(); iter != m_paths.end(); iter++)
    {
        Ipv4Address key = iter->first.dst; // destination IPV4

        // Get destination index from python module
        std::stringstream str;
        str << key;
        int dstIp_index = Py_node(pModule, 0, -1, -1, str.str(), ""); // EXISTS
        NS_LOG_INFO("PyNode " << key << " --> " << dst);

        // Get Source index
        int src_index = Py_node(pModule, NODE_CODE_EXISTS, iter->second.firstDev, -1, "", "");

        // get Path
        std::string ss_path = Py_get_path(pModule, src_index, dstIp_index);

        std::set<Hop_t, decltype(compareKey)> hops;
        FlowKey_t path_key = iter->first;
        NS_LOG_INFO("[dpId " << dpId << "] Flow key = " << path_key);
        if (ss != "")
        {
            hops.ParseStringPaths(ss); // it already validates de correctness of the path
        }

        if (hops.empty())
        {
            NS_LOG_INFO("[dpId " << dpId << "] NO PATH FOUND! " << path_key);
            // TODO eliminate all add Flows, and eliminate path??
            continue;
        }

        // search for loops
        for (auto it = hops.begin(); it != hops.end(); it++)
        {
            if (it->nextMac == src48)
            {
                // In order to avoid loops, send to normal, and hopefully in the next search, a
                // better path is given
                std::cout << "LOOP, next hop is the sender of this message" << std::endl;
                NS_LOG_WARN("LOOP, next hop is the sender of this message" << src48);
                // TODO eliminate all add Flows, and eliminate path??
                continue;
            }
        }

        if (iter->second.hops == hops)
        {
            continue;
        }
        else
        {
            std::ostringstream actions, match;
            // PS: i don't have to delete old hops, because eventually it will erase on its own. I
            // can't even because some packet might be on route
            iter->second.hops = hops; // I could only send the new hops, but it's to mutch work
            iter->second.hopSw.clear();
            // ---- send FLOWMOD
            for (auto it = hops.begin(); it != hops.end(); it++)
            {
                // Add Flow mod and route
                actions.clear();
                actions << "write:set_field=eth_dst:" << it->nextMac << ",output=" << it->portOut;
                match << iter->second.match;
                AddFlow(it->swId,
                        prioCounter,
                        match << ",eth_src=" << it->prevMac << ",eth_type=0x0800",
                        actions,
                        NO_BUFFER,
                        10);

                // TODO -----
                // Update sw hop
                RuleInfo_t ab = {it->nextMac,
                                 it->portOut,
                                 prioCounter,
                                 actions.str()}; // dstMac, out_port, prioCounter, actions.str()
                iter->second.hopSw[it->swId].out_rules.insert(ab);
            }
        }

        //
    }
}

int
WSDNController::getServiceType(struct ofl_msg_packet_in* msg)
{
    uint32_t src_port = Get_UDP_DST((struct ofl_match*)msg->match);

    int service;
    if (src_port < 200)
        service = 1000;
    else if (src_port < 300)
        service = 2000;
    else if (src_port < 400)
        service = 450;
    else if (src_port < 500)
        service = 250;
    else
        service = 100;
    return service;
}
