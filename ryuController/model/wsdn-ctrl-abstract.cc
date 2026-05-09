/*
 * Adapted from the ofswitch13 - LearningController
 */

#ifdef NS3_OFSWITCH13

#include "ryuController.h"
#include "sdn-controller.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("WSDNCtrlAbstract");
NS_OBJECT_ENSURE_REGISTERED(WSDNCtrlAbstract);

/********** Public methods ***********/
WSDNCtrlAbstract::WSDNCtrlAbstract(std::string ryuconfig)
{
    NS_LOG_FUNCTION(this);
    const wchar_t* argv[] = {L"python", nullptr};
    Py_Initialize();
    PySys_SetArgv(1, (wchar_t**)argv);

    // PyRun_SimpleString("import ryuController\n"
    //                     "ryuController.start();\n");

    // PyRun_SimpleString("import sys");
    // PyRun_SimpleString("sys.path.append('./')");
    PyObject* pName = PyUnicode_FromString("ryuController");
    PyObject* pFunc;
    pModule = PyImport_Import(pName);
    // Py_DECREF(pName);
    if (!pModule)
    {
        printf("Cant open python file!\n");
        Py_Finalize();
        exit(-2);
    }
    // std::string ryu_config_file("/home/phd/Documents/PhD/RYU/ryu_config.txt");
    pFunc = PyObject_GetAttrString(pModule, "start");
    if (!pFunc)
    {
        Py_Finalize();
        exit(-2);
    }
    PyObject* args = PyTuple_Pack(1, PyUnicode_FromString(ryuconfig.c_str()));
    PyObject_CallObject(pFunc, args);

    // pFunc = PyObject_GetAttrString(pModule, "myabs");

    // // PyObject * ryuController = PyImport_ImportModule("ryuController");
    // // PyObject* myFunction = PyObject_GetAttrString(ryuController,(char*)"myabs");

    // PyObject* args = PyTuple_Pack(1, PyFloat_FromDouble(2.0));
    // PyObject* myResult = PyObject_CallObject(pFunc, args);
    // double result = PyFloat_AsDouble(myResult);
    // std::cout << " ----- Result: " << result << std::endl;
    NS_LOG_INFO("Starting clock on python module");
    CallPythonWithTime(pModule);
}

WSDNCtrlAbstract::~WSDNCtrlAbstract()
{
    NS_LOG_FUNCTION(this);
    Py_Finalize();
}

TypeId
WSDNCtrlAbstract::GetTypeId()
{
    static TypeId tid = TypeId("ns3::WSDNCtrlAbstract")
                            .SetParent<OFSwitch13Controller>()
                            .SetGroupName("WSDNCtrlAbstract")
                            .AddConstructor<WSDNCtrlAbstract>();
    return tid;
}

void
WSDNCtrlAbstract::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_paths.clear();
    OFSwitch13Controller::DoDispose();
}

void
WSDNCtrlAbstract::Handle_OLSR(uint64_t dpId,
                              struct ofl_msg_packet_in* msg,
                              Mac48Address src48,
                              Ipv4Address srcIp)
{
    outPort = OFPP_NORMAL;
    // 1.  Handle OLSR
    //  # 1. LEARN the mac -> IP from the Hellos
    auto it = m_arpTable.find(srcIp);
    if (it == m_arpTable.end())
    {
        m_arpTable[srcIp] = src48;
        // update graph
        std::stringstream str1, str2;
        str1 << srcIp;
        str2 << src48;
        int index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
        NS_LOG_DEBUG("[Py_node] Update entry3: index=" << index << " (ip:" << str1.str()
                                                       << ",mac:" << str2.str() << ")");
    }
    else
    {
        NS_ASSERT_MSG(it->second == src48,
                      "Inconsistent ARP table" << it->second << " | " << src48);
    }

    // 2.  FLOW MOD

    match << "eth_src=" << src48 << ",eth_type=0x0800,ip_proto=17,udp_src=698";
    actions << "apply:output=normal";

    AddFlow(dpId, prioCounter, match, actions, NO_BUFFER);
}

void
WSDNCtrlAbstract::Handle_ARP_Normal(uint64_t dpId, struct ofl_msg_packet_in* msg)
{
    uint16_t arpOp = Get_ARP_TYPE((struct ofl_match*)msg->match);

    Ipv4Address reqIp = Get_ARP_SPA((struct ofl_match*)msg->match);
    Mac48Address reqMac = Get_ARP_SHA((struct ofl_match*)msg->match);
    /* arp fields need commas instead of spaces!! */
    match << "eth_type=0x0806"
          << ",arp_sha=" << reqMac << ",arp_op=" << arpOp << ",arp_spa=" << reqIp;
    actions << " apply:output=normal";
    outPort = OFPP_NORMAL;

    // 1. LEARN the SENDER src_mac -> src_IP
    auto it = m_arpTable.find(reqIp);
    if (it == m_arpTable.end())
    {
        m_arpTable[reqIp] = reqMac;
        // update graph
        std::stringstream str1, str2;
        str1 << reqIp;
        str2 << reqMac;
        int index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
        NS_LOG_DEBUG("[Py_node] Update entry1: index=" << index << " (ip:" << str1.str()
                                                       << ",mac:" << str2.str() << ")");
    }
    else
    {
        NS_ASSERT_MSG(it->second == reqMac,
                      "Inconsistent ARP table" << it->second << " | " << reqMac);
    }

    // Parse request
    if (arpOp == ArpHeader::ARP_TYPE_REQUEST)
    {
        // TODO can reply
    }
    else if (arpOp == ArpHeader::ARP_TYPE_REPLY) // Parse reply
    {
        // 2. if REPLY --> LEARN the dst_mac -> dst_IP
        Ipv4Address repIp = Get_ARP_TPA((struct ofl_match*)msg->match);
        Mac48Address repMac = Get_ARP_THA((struct ofl_match*)msg->match);
        it = m_arpTable.find(repIp);
        if (it == m_arpTable.end())
        {
            m_arpTable[repIp] = repMac;
            // update graph
            std::stringstream str1, str2;
            str1 << repIp;
            str2 << repMac;
            int index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
            NS_LOG_DEBUG("[Py_node] Update entry2: index=" << index << " (ip:" << str1.str()
                                                           << ",mac:" << str2.str() << ")");
        }
        else
        {
            NS_ASSERT_MSG(it->second == repMac,
                          "Inconsistent ARP table" << it->second << " | " << repMac);
        }

        match << ",arp_tha=" << repMac << ",arp_tpa=" << repIp;
    }
    else
    {
        NS_LOG_ERROR("Inconsistent ARP Type" << arpOp);
    }

    AddFlow(dpId, prioCounter, match, actions, msg->buffer_id);
    // send packet_out is below
}

/**
 * @brief TODO
 *
 * @param dpId
 * @param msg
 */
void
WSDNCtrlAbstract::Handle_ARP(uint64_t dpId, struct ofl_msg_packet_in* msg)
{
    uint16_t arpOp = Get_ARP_TYPE((struct ofl_match*)msg->match);

    Ipv4Address reqIp = Get_ARP_SPA((struct ofl_match*)msg->match);
    Mac48Address reqMac = Get_ARP_SHA((struct ofl_match*)msg->match);
    /* arp fields need commas instead of spaces!! */
    match << "eth_type=0x0806"
          << ",arp_sha=" << reqMac << ",arp_op=" << arpOp << ",arp_spa=" << reqIp;
    actions << " apply:output=normal";
    outPort = OFPP_NORMAL;

    // 1. LEARN the SENDER src_mac -> src_IP
    auto it = m_arpTable.find(reqIp);
    if (it == m_arpTable.end())
    {
        m_arpTable[reqIp] = reqMac;
        // update graph
        std::stringstream str1, str2;
        str1 << reqIp;
        str2 << reqMac;
        int index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
        NS_LOG_DEBUG("[Py_node] Update entry1: index=" << index << " (ip:" << str1.str()
                                                       << ",mac:" << str2.str() << ")");
    }
    else
    {
        NS_ASSERT_MSG(it->second == reqMac,
                      "Inconsistent ARP table" << it->second << " | " << reqMac);
    }

    // Parse request
    if (arpOp == ArpHeader::ARP_TYPE_REQUEST)
    {
        // TODO can reply
    }
    else if (arpOp == ArpHeader::ARP_TYPE_REPLY) // Parse reply
    {
        // 2. if REPLY --> LEARN the dst_mac -> dst_IP
        Ipv4Address repIp = Get_ARP_TPA((struct ofl_match*)msg->match);
        Mac48Address repMac = Get_ARP_THA((struct ofl_match*)msg->match);
        it = m_arpTable.find(repIp);
        if (it == m_arpTable.end())
        {
            m_arpTable[repIp] = repMac;
            // update graph
            std::stringstream str1, str2;
            str1 << repIp;
            str2 << repMac;
            int index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
            NS_LOG_DEBUG("[Py_node] Update entry2: index=" << index << " (ip:" << str1.str()
                                                           << ",mac:" << str2.str() << ")");
        }
        else
        {
            NS_ASSERT_MSG(it->second == repMac,
                          "Inconsistent ARP table" << it->second << " | " << repMac);
        }

        match << ",arp_tha=" << repMac << ",arp_tpa=" << repIp;
    }
    else
    {
        NS_LOG_ERROR("Inconsistent ARP Type" << arpOp);
    }

    AddFlow(dpId, prioCounter, match, actions, msg->buffer_id);
    // send packet_out is below
}

ofl_err
WSDNCtrlAbstract::HandlePacketIn(struct ofl_msg_packet_in* msg,
                                 Ptr<const RemoteSwitch> swtch,
                                 uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    match.str(std::string());
    actions.str(std::string());

    uint32_t outPort = OFPP_NORMAL;

    // uint32_t outPort = OFPP_FLOOD;
    enum ofp_packet_in_reason reason = msg->reason;

    // Get the switch datapath ID
    uint64_t dpId = swtch->GetDpId();

    NS_LOG_DEBUG(dpId << prioCounter);

    char* msgStr = ofl_structs_match_to_string((struct ofl_match_header*)msg->match, nullptr);
    NS_LOG_DEBUG("Packet in match: " << msgStr);
    free(msgStr);

    Mac48Address set_Eth_Dst;

    if (reason == OFPR_NO_MATCH)
    {
        // Let's get necessary information (input port and mac address)
        uint32_t inPort = Get_IN_PORT((struct ofl_match*)msg->match);

        Mac48Address src48 = Get_ETH_SRC((struct ofl_match*)msg->match);

        Mac48Address dst48 = Get_ETH_DST((struct ofl_match*)msg->match);

        // Get L3Table for this datapath
        auto it = m_experimenterCount.find(dpId);
        if (it != m_experimenterCount.end())
        {
            //----------------------------------------------
            // Parse Packet
            uint16_t ethType = Get_ETH_TYPE((struct ofl_match*)msg->match);

            // Check ARP
            if (ethType == 2054) // ARP packet
            {
                /* -- RULE : update tables, send to NORMAL port*/
                Handle_ARP(dpId, msg);
            }
            else if (ethType == 0x800)
            {
                /* -- RULE : OLSR --> update tables, send to NORMAL port*/

                Ipv4Address srcIp = Get_IPV4_SRC((struct ofl_match*)msg->match);
                Ipv4Address dstIp = Get_IPV4_DST((struct ofl_match*)msg->match);
                uint8_t ip_proto = Get_IP_PROTO((struct ofl_match*)msg->match);

                // 1. Check if OLSR
                if (ip_proto == 17 && Get_UDP_SRC((struct ofl_match*)msg->match) == 698)
                { // OLSR

                    Handle_OLSR(dpId, msg, src48, srcIp);
                }
                else if (dst48.IsBroadcast())
                { // BROADCAST
                    /* -- RULE : Bcast --> update tables, output FLOOD*/
                    outPort = OFPP_FLOOD;
                    match << "in_port=" << inPort << ",eth_src=" << src48
                          << ",eth_type=0x0800,eth_dst=" << dst48;
                    actions << "apply:output=flood";
                    // TODO can i retrieve info?
                }
                else
                {
                    // 3. Chek other Ipv4 packets
                    /* -- RULE : Ipv4 --> update tables, search next port, if no match output*/
                    // CANNOT UPDATE (src_ip -> src_mac), AD HOC DOES NOT CORRESPOND
                    // the only info i can extract is that a node with srcIp exists, and that I have
                    // a neigh in mac_src
                    // [phase 3] : search Path

                    pOutRule tmp = Handle_Ipv4(dpId, ip_proto, msg, src48, srcIp, dst48, dstIp);
                    set_Eth_Dst = tmp.set_Eth_Dst;
                    outPort = tmp.outPort;
                }
            }
        }
        else
        {
            NS_LOG_ERROR("[HANDSHAKE NOT FINISHED] No L3 table for this datapath id " << dpId);
        }

        // Lets send the packet out to switch.
        SendPacket_Out(swtch,
                       msg->buffer_id,
                       inPort,
                       msg->data_length,
                       msg->data,
                       outPort,
                       set_Eth_Dst,
                       xid);
    }
    else
    {
        NS_LOG_WARN("This controller can't handle the packet. Unkwnon reason.");
    }

    // All handlers must free the message when everything is ok
    ofl_msg_free((struct ofl_msg_header*)msg, nullptr);
    return 0;
}

ofl_err
WSDNCtrlAbstract::HandleExperimenter(struct ofl_msg_experimenter* msg,
                                     Ptr<const RemoteSwitch> swtch,
                                     uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);
    // --------------------------------------------
    const std::lock_guard<std::mutex> lock(graph_mutex);
    if (changes == -1)
    { // first call
        Simulator::Schedule(Seconds(1), RecalculatePathsPeriodically);
        changes = 0;
    }
    // --------------------------------------------

    NS_LOG_DEBUG("Handle Experimenter");
    // Get the switch datapath ID
    uint64_t dpId = swtch->GetDpId();
    //  [phase 2] : Parse Experimenter
    if (m_experimenterCount[dpId] == 5)
    {
        // reset and sent ECHO request
        m_experimenterCount[dpId] = 0;
        this->SendEchoRequest(swtch, 0);
    }
    else
    {
        m_experimenterCount[dpId]++;
    }

    std::string data((char*)msg->data, msg->data_length);
    // std::cout << msg->data_length << std::endl;
    // std::cout << data << std::endl;
    changes += Py_parse_experimenter(pModule, dpId, data);

    return 0;
}

void
WSDNCtrlAbstract::RecalculatePathsPeriodically()
{
    const std::lock_guard<std::mutex> lock(graph_mutex);
    if (changes > 0)
    {
        RecalculatePaths();
        changes = 0;
    }
    Simulator::Schedule(Seconds(1), RecalculatePathsPeriodically);
}

// -------------- PATH methods -------
int
WSDNCtrlAbstract::CreateFlow(FlowKey_t f)
{
    NS_ASSERT_MSG(!FlowExists(f), "FLOW exists!");
    m_paths[f] = MultiPath();
    return 0;
}

bool
WSDNCtrlAbstract::FlowExists(FlowKey_t f)
{
    return m_paths.find(f) != m_paths.end();
}

/********** Private methods **********/
ofl_err
WSDNCtrlAbstract::HandleFlowRemoved(struct ofl_msg_flow_removed* msg,
                                    Ptr<const RemoteSwitch> swtch,
                                    uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    // Get the switch datapath ID
    uint64_t swDpId = swtch->GetDpId();

    NS_LOG_DEBUG("Flow entry expired. Removing from L3 routing table.");
    auto it = m_experimenterCount.find(swDpId);
    if (it != m_experimenterCount.end())
    {
        Ipv4Address ipDst;

        struct ofl_match_tlv* ethDst =
            oxm_match_lookup(OXM_OF_ETH_DST, (struct ofl_match*)msg->stats->match);
        if (ethDst)
        { // it has match field ETH_DST
            Mac48Address mac48;
            mac48.CopyFrom(ethDst->value);

            for (auto it = m_arpTable.begin(); it != m_arpTable.end(); it++)
            {
                if (it->second == mac48)
                {
                    ipDst = it->first;
                    break;
                }
            }
        }
        else
        {
            struct ofl_match_tlv* tlv_ipDst =
                oxm_match_lookup(OXM_OF_IPV4_DST, (struct ofl_match*)msg->stats->match);
            if (tlv_ipDst)
            { // it has match field IPV4_DST
                ipDst = Get_IPV4_DST((struct ofl_match*)msg->stats->match);
            }
            else
            {
                NS_LOG_INFO("Cant remove, match does not contain destination mac or ipv4 !!");
                return 0;
            }
        }

        L3Table_t* l3Table = &it->second;
        auto itRule = l3Table->find(ipDst);
        if (itRule != l3Table->end())
        {
            int msg_prio = msg->stats->priority;
            int rule_prio = std::get<3>(itRule->second);
            if (msg_prio >= rule_prio)
            {
                l3Table->erase(itRule);
            }
            else
            {
                NS_LOG_INFO("Can't remove flow, rule in routing table is more recent (has a higher "
                            "priority)");
            }
        }
    }

    // All handlers must free the message when everything is ok
    ofl_msg_free_flow_removed(msg, true, nullptr);
    return 0;
}

void
WSDNCtrlAbstract::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch)
{
    NS_LOG_FUNCTION(this << swtch);

    // Get the switch datapath ID
    uint64_t swDpId = swtch->GetDpId();

    // After a successfull handshake, let's install the table-miss entry,
    // setting to 128 bytes the maximum amount of data from a packet that should
    // be sent to the controller.
    DpctlExecute(swDpId,
                 "flow-mod cmd=add,table=0,prio=0 "
                 "apply:output=ctrl:128");
    // Table miss entry.

    // Configure te switch to buffer packets and send only the first 128 bytes
    // of each packet sent to the controller when not using an output action to
    // the OFPP_CONTROLLER logical port.
    DpctlExecute(swDpId, "set-config miss=128");

    // Set experimenter count to zero
    m_experimenterCount[swDpId] = 0;
}

void
WSDNCtrlAbstract::AddFlow(uint64_t dpId,
                          int& prio,
                          std::ostringstream& match,
                          std::ostringstream& actions,
                          uint32_t buffer_id,
                          int idle_timeout,
                          int hard_timeout)
{
    // Send a flow-mod to switch creating this flow. Let's
    // configure the flow entry to Xs idle timeout and to
    // notify the controller when flow expires. (flags=0x0001)

    // NS_ASSERT_MSG(buffer_id == NO_BUFFER, "buffer exists");

    std::ostringstream cmd;
    cmd << "flow-mod cmd=add,table=0,idle=" << idle_timeout << ",hard=" << hard_timeout
        << ",flags=0x0001,prio=" << ++prio;
    if (buffer_id != NO_BUFFER)
    {
        cmd << ",buffer=" << buffer_id; // requires commas
    }

    // cmd << "flow-mod cmd=add,table=0,idle="<< idle_timeout <<",flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=" << src48
    //     << " apply:output=" << inPort;
    cmd << " " << match.str();

    cmd << " "
        << actions.str(); //  << " apply:set_field=eth_dst:" << mac_out << ",output=" << outPort;

    NS_LOG_INFO(cmd.str());
    DpctlExecute(dpId, cmd.str());
}

void
WSDNCtrlAbstract::RemoveFlow(uint64_t dpId,
                             int prio,
                             std::string match,
                             std::string actions,
                             uint32_t buffer_id,
                             int idle_timeout,
                             int hard_timeout)
{
    // Send a flow-mod to switch creating this flow. Let's
    // configure the flow entry to Xs idle timeout and to
    // notify the controller when flow expires. (flags=0x0001)

    // NS_ASSERT_MSG(buffer_id == NO_BUFFER, "buffer exists");

    std::ostringstream cmd;
    cmd << "flow-mod cmd=dels,table=0,idle=" << idle_timeout << ",hard=" << hard_timeout
        << ",flags=0x0001,prio=" << prio;
    if (buffer_id != NO_BUFFER)
    {
        cmd << ",buffer=" << buffer_id; // requires commas
    }

    // cmd << "flow-mod cmd=add,table=0,idle="<< idle_timeout <<",flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=" << src48
    //     << " apply:output=" << inPort;
    cmd << " " << match;

    cmd << " " << actions; //  << " apply:set_field=eth_dst:" << mac_out << ",output=" << outPort;

    NS_LOG_INFO(cmd.str());
    DpctlExecute(dpId, cmd.str());
}

void
WSDNCtrlAbstract::ModifyFlow(uint64_t dpId,
                             int& prio,
                             std::string match,
                             std::string actions,
                             uint32_t buffer_id,
                             int idle_timeout,
                             int hard_timeout)
{
    // Send a flow-mod to switch creating this flow. Let's
    // configure the flow entry to Xs idle timeout and to
    // notify the controller when flow expires. (flags=0x0001)

    // NS_ASSERT_MSG(buffer_id == NO_BUFFER, "buffer exists");

    // TODO cmd=mod ERROR in flow_entry_replace_instructions at sources/udatapath/flow_entry.c:140

    std::ostringstream cmd;
    cmd << "flow-mod cmd=add,table=0,idle=" << idle_timeout << ",hard=" << hard_timeout
        << ",flags=0x0001,prio=" << ++prio;
    if (buffer_id != NO_BUFFER)
    {
        cmd << ",buffer=" << buffer_id; // requires commas
    }

    // cmd << "flow-mod cmd=add,table=0,idle="<< idle_timeout <<",flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=" << src48
    //     << " apply:output=" << inPort;
    cmd << " " << match;

    cmd << " " << actions; //  << " apply:set_field=eth_dst:" << mac_out << ",output=" << outPort;

    NS_LOG_INFO(cmd.str());
    DpctlExecute(dpId, cmd.str());
}

void
WSDNCtrlAbstract::SendPacket_Out(Ptr<const RemoteSwitch> swtch,
                                 uint32_t buffer_id,
                                 uint32_t in_port,
                                 size_t data_length,
                                 uint8_t* data,
                                 uint32_t out_port,
                                 Mac48Address& set_Eth_Dst,
                                 uint32_t xid)

{
    Mac48Address empty_mac;
    // Lets send the packet out to switch.
    struct ofl_msg_packet_out reply;
    reply.header.type = OFPT_PACKET_OUT;
    reply.buffer_id = buffer_id;
    reply.in_port = in_port;
    reply.data_length = 0;
    reply.data = nullptr;

    if (buffer_id == NO_BUFFER)
    {
        // No packet buffer. Send data back to switch
        reply.data_length = data_length;
        reply.data = data;
    }

    // Create output actions
    struct ofl_action_output* a =
        (struct ofl_action_output*)xmalloc(sizeof(struct ofl_action_output));
    a->header.type = OFPAT_OUTPUT;
    a->port = out_port;
    a->max_len = 0;
    struct ofl_action_set_field* b;

    if (set_Eth_Dst != empty_mac)
    {
        b = (struct ofl_action_set_field*)xmalloc(sizeof(struct ofl_action_set_field));
        b->header.type = OFPAT_SET_FIELD;
        b->field = (struct ofl_match_tlv*)malloc(sizeof(struct ofl_match_tlv));

        b->field->header = OXM_OF_ETH_DST;
        int len = ETH_ADDR_LEN;
        b->field->value = (uint8_t*)malloc(len); // uint8_t *value;     /* TLV value */
        set_Eth_Dst.CopyTo(
            b->field->value); // memcpy(b->field->value , buffer, OXM_LENGTH(b->field->header));
        reply.actions_num = 2;
        reply.actions = (struct ofl_action_header**)malloc(sizeof(struct ofl_action_header*) *
                                                           reply.actions_num);
        reply.actions[0] = (struct ofl_action_header*)b;
        reply.actions[1] = (struct ofl_action_header*)a;
    }
    else
    {
        reply.actions_num = 1;
        reply.actions = (struct ofl_action_header**)&a;
    }

    SendToSwitch(swtch, (struct ofl_msg_header*)&reply, xid);
    free(a);
    if (reply.actions_num == 2)
    {
        free(b->field->value);
        free(b->field);
        free(b);
        free(reply.actions);
    }
}

/*------------ Get Methods ------------*/
Mac48Address
WSDNCtrlAbstract::Get_ETH_SRC(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address src48;
    input = oxm_match_lookup(OXM_OF_ETH_SRC, match);
    src48.CopyFrom(input->value);
    return src48;
}

Mac48Address
WSDNCtrlAbstract::Get_ETH_DST(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address dst48;
    input = oxm_match_lookup(OXM_OF_ETH_DST, match);
    dst48.CopyFrom(input->value);
    return dst48;
}

uint32_t
WSDNCtrlAbstract::Get_IN_PORT(struct ofl_match* match)
{
    uint32_t inPort;
    struct ofl_match_tlv* input;
    input = oxm_match_lookup(OXM_OF_IN_PORT, match);
    memcpy(&inPort, input->value, OXM_LENGTH(OXM_OF_IN_PORT));
    return inPort;
}

Ipv4Address
WSDNCtrlAbstract::Get_IPV4_DST(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_DST, match);
}

Ipv4Address
WSDNCtrlAbstract::Get_IPV4_SRC(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_SRC, match);
}

Mac48Address
WSDNCtrlAbstract::Get_ARP_SHA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_SHA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
WSDNCtrlAbstract::Get_ARP_SPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_SPA, match);
}

Mac48Address
WSDNCtrlAbstract::Get_ARP_THA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_THA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
WSDNCtrlAbstract::Get_ARP_TPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_TPA, match);
}

uint8_t
WSDNCtrlAbstract::Get_IP_PROTO(struct ofl_match* match)
{
    uint8_t ip_proto;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_IP_PROTO, match);
    memcpy(&ip_proto, tlv->value, OXM_LENGTH(OXM_OF_IP_PROTO));
    return ip_proto;
}

uint32_t
WSDNCtrlAbstract::Get_UDP_SRC(struct ofl_match* match)
{
    uint32_t UDPPort = 0;
    size_t UDP_portLen = OXM_LENGTH(OXM_OF_UDP_SRC); // (Always 2 bytes)
    struct ofl_match_tlv* udpSrc = oxm_match_lookup(OXM_OF_UDP_SRC, match);
    memcpy(&UDPPort, udpSrc->value, UDP_portLen);
    return UDPPort;
}

uint32_t
WSDNCtrlAbstract::Get_UDP_DST(struct ofl_match* match)
{
    uint32_t UDPPort = 0;
    size_t UDP_portLen = OXM_LENGTH(OXM_OF_UDP_DST); // (Always 2 bytes)
    struct ofl_match_tlv* udpSrc = oxm_match_lookup(OXM_OF_UDP_DST, match);
    memcpy(&UDPPort, udpSrc->value, UDP_portLen);
    return UDPPort;
}

uint16_t
WSDNCtrlAbstract::Get_ETH_TYPE(struct ofl_match* match)
{
    uint16_t ethType;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ETH_TYPE, match);
    memcpy(&ethType, tlv->value, OXM_LENGTH(OXM_OF_ETH_TYPE));
    return ethType;
}

uint16_t
WSDNCtrlAbstract::Get_ARP_TYPE(struct ofl_match* match)
{
    // Get ARP operation
    uint16_t arpOp;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ARP_OP, match);
    memcpy(&arpOp, tlv->value, OXM_LENGTH(OXM_OF_ARP_OP));
    return arpOp;
}

Ipv4Address
WSDNCtrlAbstract::ExtractIpv4Address(uint32_t oxm_of, struct ofl_match* match)
{
    switch (oxm_of)
    {
    case static_cast<uint32_t>(OXM_OF_ARP_SPA):
    case static_cast<uint32_t>(OXM_OF_ARP_TPA):
    case static_cast<uint32_t>(OXM_OF_IPV4_DST):
    case static_cast<uint32_t>(OXM_OF_IPV4_SRC): {
        uint32_t ip;
        int size = OXM_LENGTH(oxm_of);
        struct ofl_match_tlv* tlv = oxm_match_lookup(oxm_of, match);
        memcpy(&ip, tlv->value, size);
        return Ipv4Address(ntohl(ip));
    }
    default:
        NS_ABORT_MSG("Invalid IP field.");
    }
}

} // namespace ns3
#endif // NS3_OFSWITCH13
