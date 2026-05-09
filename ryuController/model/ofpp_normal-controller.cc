/*
 * Adapted from the ofswitch13 - LearningController
 */

#ifdef NS3_OFSWITCH13

#include "ofpp_normal-controller.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OfppNormalController");
NS_OBJECT_ENSURE_REGISTERED(OfppNormalController);

/********** Public methods ***********/
OfppNormalController::OfppNormalController(std::string abspath)
{
    NS_LOG_FUNCTION(this << abspath);
    const wchar_t* argv[] = {L"python", nullptr};
    Py_Initialize();
    PySys_SetArgv(1, (wchar_t**)argv);

    // PyRun_SimpleString("import OfppNormalController\n"
    //                     "OfppNormalController.start();\n");

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
    pFunc = PyObject_GetAttrString(pModule, "start");
    if (!pFunc)
    {
        Py_Finalize();
        exit(-2);
    }
    PyObject* args = PyTuple_Pack(1, PyUnicode_FromString(abspath.c_str()));
    PyObject* myResult = PyObject_CallObject(pFunc, args);
    if (!myResult)
        NS_LOG_ERROR("something went wrong when calling function start");

    int result = (int)PyLong_AsLong(myResult);
    if (result != 0)
        throw std::runtime_error("No config files found!"); // raise exception

    // pFunc = PyObject_GetAttrString(pModule, "myabs");

    // // PyObject * OfppNormalController = PyImport_ImportModule("OfppNormalController");
    // // PyObject* myFunction = PyObject_GetAttrString(OfppNormalController,(char*)"myabs");

    // PyObject* args = PyTuple_Pack(1, PyFloat_FromDouble(2.0));
    // PyObject* myResult = PyObject_CallObject(pFunc, args);
    // double result = PyFloat_AsDouble(myResult);
    // std::cout << " ----- Result: " << result << std::endl;
    NS_LOG_INFO("Starting clock on python module");
    CallPythonWithTime(pModule);
}

OfppNormalController::~OfppNormalController()
{
    NS_LOG_FUNCTION(this);
    Py_Finalize();
}

TypeId
OfppNormalController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OfppNormalController")
                            .SetParent<OFSwitch13Controller>()
                            .SetGroupName("OFSwitch13")
                            .AddConstructor<OfppNormalController>();
    return tid;
}

void
OfppNormalController::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_learnedInfo.clear();
    OFSwitch13Controller::DoDispose();
}

ofl_err
OfppNormalController::HandlePacketIn(struct ofl_msg_packet_in* msg,
                                     Ptr<const RemoteSwitch> swtch,
                                     uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

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
    std::ostringstream actions, match;

    if (reason == OFPR_NO_MATCH)
    {
        // Let's get necessary information (input port and mac address)
        uint32_t inPort = Get_IN_PORT((struct ofl_match*)msg->match);

        Mac48Address src48 = Get_ETH_SRC((struct ofl_match*)msg->match);

        // Mac48Address dst48 = Get_ETH_DST((struct ofl_match*)msg->match);

        // Get L3Table for this datapath
        auto it = m_learnedInfo.find(dpId);
        if (it != m_learnedInfo.end())
        {
            L3Table_t* l3Table = &it->second;
            NS_ASSERT_MSG(l3Table, "TODO");

            match << "eth_src=" << src48;
            actions << " apply:output=normal";
            outPort = OFPP_NORMAL;

            AddFlow(dpId, prioCounter, match, actions, msg->buffer_id); // FOREVER
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
OfppNormalController::HandleExperimenter(struct ofl_msg_experimenter* msg,
                                         Ptr<const RemoteSwitch> swtch,
                                         uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);
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
    int period;
    int type = (int)msg->exp_type;
    if (type == 0)
        period = 1;
    else
        period = 0;

    Py_parse_experimenter(pModule, dpId, data, period);

    ofl_msg_free_experimenter(msg, nullptr);
    return 0;
}

ofl_err
OfppNormalController::HandleFlowRemoved(struct ofl_msg_flow_removed* msg,
                                        Ptr<const RemoteSwitch> swtch,
                                        uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    // Get the switch datapath ID
    uint64_t swDpId = swtch->GetDpId();

    NS_LOG_DEBUG("Flow entry expired. Removing from L3 routing table.");
    auto it = m_learnedInfo.find(swDpId);
    if (it != m_learnedInfo.end())
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

/********** Private methods **********/
void
OfppNormalController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch)
{
    NS_LOG_FUNCTION(this << swtch);

    // Get the switch datapath ID
    uint64_t swDpId = swtch->GetDpId();

    // After a successfull handshake, let's install the table-miss entry,
    // setting to 128 bytes the maximum amount of data from a packet that should
    // be sent to the controller.
    DpctlExecute(swDpId,
                 "flow-mod cmd=add,table=0,prio=0 "
                 "apply:output=normal");
    // Table miss entry.

    // Configure te switch to buffer packets and send only the first 128 bytes
    // of each packet sent to the controller when not using an output action to
    // the OFPP_CONTROLLER logical port.
    DpctlExecute(swDpId, "set-config miss=128");

    // Create an empty L2SwitchingTable and insert it into m_learnedInfo
    L3Table_t l3Table;
    std::pair<uint64_t, L3Table_t> entry(swDpId, l3Table);
    auto ret = m_learnedInfo.insert(entry);
    if (ret.second == false)
    {
        NS_LOG_ERROR("Table exists for this datapath.");
    }
    // Set experimenter count to zero
    m_experimenterCount[swDpId] = 0;
}

void
OfppNormalController::AddFlow(uint64_t dpId,
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
OfppNormalController::RemoveFlow(uint64_t dpId,
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
OfppNormalController::ModifyFlow(uint64_t dpId,
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
OfppNormalController::SendPacket_Out(Ptr<const RemoteSwitch> swtch,
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

/*
void
OfppNormalController::RecalculatePathsWithoutSpread(uint64_t dpId) // int index_dst
{
    NS_LOG_FUNCTION(this << dpId);
    // Get my index from python module
    int my_index = Py_node(pModule, 0, dpId, -1, "", ""); // EXISTS
    NS_LOG_INFO("PyNode " << dpId << " --> " << my_index);
    if (m_learnedInfo[dpId].begin() == m_learnedInfo[dpId].end())
    {
        NS_LOG_INFO("Routing table is empty");
    }

    // For Each Rule:
    for (L3Table_t::iterator it = m_learnedInfo[dpId].begin(); it != m_learnedInfo[dpId].end();)
    {
        Ipv4Address key = it->first; // IPV4
        // Get destination index from python module
        std::stringstream str;
        str << key;
        int dst = Py_node(pModule, 0, -1, -1, str.str(), ""); // EXISTS
        NS_LOG_INFO("PyNode " << key << " --> " << dst);
        ForwardingRule_t rule =
            (it->second); // dstMac, out_port, match.str(), prioCounter, actions.str()
        // Get Path from python module
        std::tuple<std::string, int> res = Py_get_next_hop(pModule, my_index, dst);
        std::string next_mac = std::get<0>(res);
        int out_port = std::get<1>(res);

        if (out_port == -1 || next_mac == "")
        {
            NS_LOG_INFO("---------- NO Path from " << my_index << " to " << dst
                                                   << " --> Removing Flow");
            // TODO quando nao há path: remove rule and flow from switch
            // FLOW REMOVE
            // flows in m_learnedInfo always has idle = 10
            RemoveFlow(dpId,
                       std::get<3>(rule),
                       std::get<2>(rule),
                       std::get<4>(rule),
                       NO_BUFFER,
                       30);

            it = m_learnedInfo[dpId].erase(it);
        }
        else
        {
            ++it;

            NS_LOG_INFO("Py_get_next_hop " << my_index << " - " << dst << " : (" << next_mac << ","
                                           << out_port << ")");

            Mac48Address dstMac = Mac48Address(next_mac.c_str());
            if (out_port != -1 && dstMac != std::get<0>(rule))
            {
                NS_LOG_DEBUG("next macs : " << dstMac << " - " << std::get<0>(rule));
                // Add Flow mod
                std::ostringstream match, actions;
                match << std::get<2>(rule);
                actions << "write:set_field=eth_dst:" << dstMac << ",output=" << out_port;
                ModifyFlow(dpId, prioCounter, match.str(), actions.str());
                // change rule
                m_learnedInfo[dpId][key] =
                    std::make_tuple(dstMac, out_port, match.str(), prioCounter, actions.str());
            }
        }
    }
}

void
OfppNormalController::RecalculatePathsToIp(uint64_t dpId, Ipv4Address ip)
{
    NS_LOG_FUNCTION(this << dpId << ip);

    int my_index = Py_node(pModule, 0, dpId, -1, "", ""); // EXISTS
    NS_LOG_INFO("PyNode " << dpId << " --> " << my_index);

    std::stringstream str;
    str << ip;
    int dst_index = Py_node(pModule, 0, -1, -1, str.str(), ""); // EXISTS
    NS_LOG_INFO("PyNode " << ip << " --> " << dst_index);

    ForwardingRule_t rule = m_learnedInfo[dpId][ip]; // tuple
    // Get Path from python module
    std::tuple<std::string, int> res = Py_get_next_hop(pModule, my_index, dst_index);
    std::string next_mac = std::get<0>(res);
    int out_port = std::get<1>(res);
    NS_LOG_INFO("Py_get_next_hop " << my_index << " - " << dst_index << " : (" << next_mac << ","
                                   << out_port << ")");
    Mac48Address dstMac = Mac48Address(next_mac.c_str());
    if (out_port != -1 && dstMac != std::get<0>(rule))
    {
        // Add Flow mod
        std::ostringstream match, actions;
        match << std::get<2>(rule);
        actions << "write:set_field=eth_dst:" << dstMac << ",output=" << out_port;
        ModifyFlow(dpId, prioCounter, match.str(), actions.str());
        // change rule
        m_learnedInfo[dpId][ip] =
            std::make_tuple(dstMac, out_port, match.str(), prioCounter, actions.str());
    }
}*/

/*------------ Get Methods ------------*/
Mac48Address
OfppNormalController::Get_ETH_SRC(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address src48;
    input = oxm_match_lookup(OXM_OF_ETH_SRC, match);
    src48.CopyFrom(input->value);
    return src48;
}

Mac48Address
OfppNormalController::Get_ETH_DST(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address dst48;
    input = oxm_match_lookup(OXM_OF_ETH_DST, match);
    dst48.CopyFrom(input->value);
    return dst48;
}

uint32_t
OfppNormalController::Get_IN_PORT(struct ofl_match* match)
{
    uint32_t inPort;
    struct ofl_match_tlv* input;
    input = oxm_match_lookup(OXM_OF_IN_PORT, match);
    memcpy(&inPort, input->value, OXM_LENGTH(OXM_OF_IN_PORT));
    return inPort;
}

Ipv4Address
OfppNormalController::Get_IPV4_DST(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_DST, match);
}

Ipv4Address
OfppNormalController::Get_IPV4_SRC(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_SRC, match);
}

Mac48Address
OfppNormalController::Get_ARP_SHA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_SHA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
OfppNormalController::Get_ARP_SPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_SPA, match);
}

Mac48Address
OfppNormalController::Get_ARP_THA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_THA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
OfppNormalController::Get_ARP_TPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_TPA, match);
}

uint8_t
OfppNormalController::Get_IP_PROTO(struct ofl_match* match)
{
    uint8_t ip_proto;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_IP_PROTO, match);
    memcpy(&ip_proto, tlv->value, OXM_LENGTH(OXM_OF_IP_PROTO));
    return ip_proto;
}

uint32_t
OfppNormalController::Get_UDP_SRC(struct ofl_match* match)
{
    uint32_t UDPPort = 0;
    size_t UDP_portLen = OXM_LENGTH(OXM_OF_UDP_SRC); // (Always 2 bytes)
    struct ofl_match_tlv* udpSrc = oxm_match_lookup(OXM_OF_UDP_SRC, match);
    memcpy(&UDPPort, udpSrc->value, UDP_portLen);
    return UDPPort;
}

uint16_t
OfppNormalController::Get_ETH_TYPE(struct ofl_match* match)
{
    uint16_t ethType;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ETH_TYPE, match);
    memcpy(&ethType, tlv->value, OXM_LENGTH(OXM_OF_ETH_TYPE));
    return ethType;
}

uint16_t
OfppNormalController::Get_ARP_TYPE(struct ofl_match* match)
{
    // Get ARP operation
    uint16_t arpOp;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ARP_OP, match);
    memcpy(&arpOp, tlv->value, OXM_LENGTH(OXM_OF_ARP_OP));
    return arpOp;
}

Ipv4Address
OfppNormalController::ExtractIpv4Address(uint32_t oxm_of, struct ofl_match* match)
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
