/*
 * Adapted from the ofswitch13 - LearningController
 Simple controller where the OPF_NORMAL is used for every packet.
 NO PYTHON module is used!
 No ramifications with utils
 NO EXPERIMENTER
 NO FLOW REMOVED
 NO FLOW INSERTED
 */

#ifdef NS3_OFSWITCH13

#include "ofpp-python-ctrl.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OfppPythonController");
NS_OBJECT_ENSURE_REGISTERED(OfppPythonController);

/********** Public methods ***********/
OfppPythonController::OfppPythonController(std::string abspath)
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

    NS_LOG_INFO("Starting clock on python module");
    CallPythonWithTime(pModule);

    std::cout << " ----- END config RYU controller " << std::endl;
}

OfppPythonController::~OfppPythonController()
{
    NS_LOG_FUNCTION(this);
    Py_Finalize();
}

TypeId
OfppPythonController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OfppPythonController")
                            .SetParent<OFSwitch13Controller>()
                            .SetGroupName("OFSwitch13")
                            .AddConstructor<OfppPythonController>();
    return tid;
}

void
OfppPythonController::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_learnedInfo.clear();
    OFSwitch13Controller::DoDispose();
}

ofl_err
OfppPythonController::HandlePacketIn(struct ofl_msg_packet_in* msg,
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
    std::cout << "\n -------- \nPacket in match [" << dpId << "]: " << msgStr << std::endl;
    free(msgStr);

    Mac48Address set_Eth_Dst;
    std::ostringstream actions, match;

    if (reason == OFPR_NO_MATCH)
    {
        // Let's get necessary information (input port and mac address)
        uint32_t inPort = Get_IN_PORT((struct ofl_match*)msg->match);

        Mac48Address src48 = Get_ETH_SRC((struct ofl_match*)msg->match);

        Mac48Address dst48 = Get_ETH_DST((struct ofl_match*)msg->match);

        // Get L3Table for this datapath
        auto it = m_learnedInfo.find(dpId);
        if (it != m_learnedInfo.end())
        {
            L3Table_t* l3Table = &it->second;
            NS_ASSERT_MSG(l3Table, "TODO");

            //----------------------------------------------
            // Parse Packet
            uint16_t ethType = Get_ETH_TYPE((struct ofl_match*)msg->match);

            // Check ARP
            if (ethType == 2054) // ARP packet
            {
                // -- RULE : update tables, send to NORMAL port

                uint16_t arpOp = Get_ARP_TYPE((struct ofl_match*)msg->match);

                Ipv4Address reqIp = Get_ARP_SPA((struct ofl_match*)msg->match);
                Mac48Address reqMac = Get_ARP_SHA((struct ofl_match*)msg->match);
                // arp fields need commas instead of spaces!!
                match << "eth_type=0x0806" << ",arp_sha=" << reqMac << ",arp_op=" << arpOp
                      << ",arp_spa=" << reqIp;
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
                    // TODO
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
                        int index =
                            Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
                        NS_LOG_DEBUG("[Py_node] Update entry2: index="
                                     << index << " (ip:" << str1.str() << ",mac:" << str2.str()
                                     << ")");
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

                // add to all sdns devices
                for (auto it = m_switches.begin(); it != m_switches.end(); it++)
                {
                    AddFlow(*it, prioCounter, match, actions, msg->buffer_id);
                }

                // send packet_out is below
            }
            else if (ethType == 0x800)
            {
                // -- RULE : OLSR --> update tables, send to NORMAL port

                Ipv4Address srcIp = Get_IPV4_SRC((struct ofl_match*)msg->match);
                Ipv4Address dstIp = Get_IPV4_DST((struct ofl_match*)msg->match);
                uint8_t ip_proto = Get_IP_PROTO((struct ofl_match*)msg->match);

                // 1. Check if OLSR
                if (ip_proto == 17 && Get_UDP_DST((struct ofl_match*)msg->match) == 698)
                { // OLSR
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
                        int index =
                            Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), str2.str());
                        NS_LOG_DEBUG("[Py_node] Update entry3: index="
                                     << index << " (ip:" << str1.str() << ",mac:" << str2.str()
                                     << ")");
                    }
                    else
                    {
                        NS_ASSERT_MSG(it->second == src48,
                                      "Inconsistent ARP table" << it->second << " | " << src48);
                    }

                    // 2.  FLOW MOD
                    match << "eth_src=" << src48 << ",eth_type=0x0800,ip_proto=17,udp_src=698";
                    actions << "apply:output=normal";

                    AddFlow(dpId, prioCounter, match, actions, msg->buffer_id);
                }
                else if (dst48.IsBroadcast())
                { // BROADCAST
                    // -- RULE : Bcast --> update tables, output FLOOD
                    outPort = OFPP_FLOOD;
                    match << "in_port=" << inPort << ",eth_src=" << src48
                          << ",eth_type=0x0800,eth_dst=" << dst48;
                    actions << "apply:output=flood";
                    // TODO can i retrieve info?
                }
                else
                { // 3. Chek other Ipv4 packets
                    std::stringstream str1, str2;
                    str1 << srcIp;
                    str2 << dstIp;
                    int srcIp_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), "");
                    NS_LOG_DEBUG("[Py_node]  Update entry4: index=" << srcIp_index << " (ip:"
                                                                    << str1.str() << ")");
                    int dstIp_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str2.str(), "");
                    NS_LOG_DEBUG("[Py_node]  Update entry5: index=" << dstIp_index << " (ip:"
                                                                    << str2.str() << ")");

                    // [phase 3] : search Path
                    outPort = OFPP_NORMAL;

                    match << "in_port=" << inPort << ",eth_src=" << src48
                          << ",eth_type=0x0800,eth_dst=" << dst48 << ",ip_dst=" << dstIp
                          << ",ip_src=" << srcIp; // needs commas

                    // -- Rule 1: Destination node is unknown
                    if (dstIp_index == -1)
                    {
                        actions << "apply:output=normal";
                        outPort = OFPP_NORMAL;
                        // NO ADD_FLOW : wait, because next time this node may be in the tables
                    }
                    else
                    {
                        int my_index =
                            Py_node(pModule, NODE_CODE_EXISTS, dpId, inPort, "", ""); // EXISTS
                        NS_ASSERT_MSG(my_index >= 0, "Index out of bounds!");
                        NS_LOG_DEBUG("[Py_node]  EXISTS entry: index=" << my_index
                                                                       << " (dpIp:" << dpId << ")");

                        int sameDev = Py_check_same_dev(pModule, my_index, dstIp_index);
                        NS_LOG_DEBUG("\tsame Dev  " << sameDev);
                        // --  Rule 2: i'm already in the destination device
                        if (sameDev ||
                            (ip_proto != 17 && ip_proto != 6)) // same | not udp and not tcp
                        {
                            actions << "apply:output=normal";
                            outPort = OFPP_NORMAL;
                            // AddFlow(dpId, prioCounter, match, actions, msg->buffer_id); //
                            // FOREVER
                            AddFlow(dpId, prioCounter, match, actions, NO_BUFFER);
                            // TODO entrar para as routing tables???
                        }
                        else
                        {
                            // -- Rule 3: Get Path
                            // 2. path = net.get_path(my_index, dst_index)
                            uint32_t src_port = Get_UDP_DST((struct ofl_match*)msg->match);

                            int service = src_port;

                            std::vector<std::string> res = Py_get_path(pModule,
                                                                       my_index,
                                                                       dstIp_index,
                                                                       service,
                                                                       srcIp_index,
                                                                       src48);

                            std::tuple<std::string, int> pkt_out_res =
                                parse_packet_out(res[0]); // mac, port
                            std::string next_mac = std::get<0>(pkt_out_res);
                            int outPort_tmp = std::get<1>(pkt_out_res);

                            actions.clear();

                            if (outPort_tmp == -1 || next_mac == "")
                            {
                                actions << "apply:output=normal";
                                outPort = OFPP_NORMAL;
                            }
                            else
                            {
                                /* Packet_out cannot set a field */
                                outPort = outPort_tmp;
                                set_Eth_Dst = Mac48Address(next_mac.c_str());

                                actions << "write:set_field=eth_dst:" << next_mac
                                        << ",output=" << outPort_tmp;

                                std::cout << " \n Packet OUT --> " << actions.str() << std::endl;
                                // actions << "apply:output=normal";
                                // outPort = OFPP_NORMAL;
                            }
                            res.erase(res.begin());

                            std::string delimiter = "&";

                            for (auto& s : res)
                            {
                                auto pos1 = s.find(delimiter);
                                std::string token = s.substr(0, pos1);
                                int _dpId = std::stoi(token);
                                std::string _cmd = s.substr(pos1 + delimiter.length(), s.length());
                                std::cout << "[" << _dpId << "]Command : " << _cmd << std::endl;
                                // Add Flow mod and route
                                DpctlExecute(_dpId, _cmd);
                            }
                        }
                    }
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
    std::cout << "\n -------- END Packet in " << std::endl;
    return 0;
} // namespace ns3

ofl_err
OfppPythonController::HandleExperimenter(struct ofl_msg_experimenter* msg,
                                         Ptr<const RemoteSwitch> swtch,
                                         uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);
    NS_LOG_DEBUG("Handle Experimenter");

    // Get the switch datapath ID
    uint64_t dpId = swtch->GetDpId();

    m_switches.insert(dpId);

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
    // for (auto i = 0; i < (int)msg->data_length; i++)
    // {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0')
    //               << (int)static_cast<unsigned char>(msg->data[i]) << ' ';
    // }
    // std::cout << std::endl;

    int period;
    int type = (int)msg->exp_type;
    if (type == 0)
        period = 1;
    else
        period = 0;

    std::vector<std::string> res = Py_parse_experimenter(pModule, dpId, data, period);
    std::string delimiter = "&";

    for (auto& s : res)
    {
        auto pos1 = s.find(delimiter);
        std::string token = s.substr(0, pos1);
        int _dpId = std::stoi(token);
        std::string _cmd = s.substr(pos1 + delimiter.length(), s.length());
        // Add Flow mod and route
        DpctlExecute(_dpId, _cmd);
    }

    ofl_msg_free_experimenter(msg, nullptr);
    return 0;
}

ofl_err
OfppPythonController::HandleFlowRemoved(struct ofl_msg_flow_removed* msg,
                                        Ptr<const RemoteSwitch> swtch,
                                        uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    char* msgStr =
        ofl_structs_match_to_string((struct ofl_match_header*)msg->stats->match, nullptr);
    NS_LOG_DEBUG("Packet in match: " << msgStr);
    std::cout << "\n -------- \tFlow remove match: " << msgStr << std::endl;
    free(msgStr);

    // Mac48Address src48 = Get_ETH_SRC((struct ofl_match*)msg->match);
    Ipv4Address srcIp = Get_IPV4_SRC((struct ofl_match*)msg->stats->match);
    Ipv4Address dstIp = Get_IPV4_DST((struct ofl_match*)msg->stats->match);

    uint64_t dpId = swtch->GetDpId();
    // uint32_t inPort = Get_IN_PORT((struct ofl_match*)msg->stats->match);
    int my_index = Py_node(pModule, NODE_CODE_EXISTS, dpId, -1, "", ""); // EXISTS
    std::stringstream str1, str2;
    str1 << srcIp;
    str2 << dstIp;
    int srcIp_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str1.str(), "");
    NS_LOG_DEBUG("[Py_node]  Update entry4: index=" << srcIp_index << " (ip:" << str1.str() << ")");
    int dstIp_index = Py_node(pModule, NODE_CODE_UPDATE, -1, -1, str2.str(), "");
    NS_LOG_DEBUG("[Py_node]  Update entry5: index=" << dstIp_index << " (ip:" << str2.str() << ")");
    uint32_t src_port = Get_UDP_DST((struct ofl_match*)msg->stats->match);

    int service = src_port;

    if (msg->reason == OFPRR_IDLE_TIMEOUT)
    {
        std::cout << "Py_process_idle_timeout" << std::endl;
        Py_process_idle_timeout(pModule, my_index, dstIp_index, service, srcIp_index);
    }
    else if (msg->reason == OFPRR_HARD_TIMEOUT)
    {
        std::cout << "Py_process_hard_timeout" << std::endl;
        Py_process_hard_timeout(pModule, my_index, dstIp_index, service, srcIp_index);
    }
    else
    {
        std::cout << "---- Other reason:" << msg->reason << " -----------" << std::endl;
    }

    // All handlers must free the message when everything is ok
    ofl_msg_free_flow_removed(msg, true, nullptr);

    return 0;
}

/********** Private methods **********/
void
OfppPythonController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch)
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
    // DpctlExecute(swDpId, "flow-mod cmd=add,table=0,prio=1 eth_type=0x0806
    // apply:output=normal"); DpctlExecute(swDpId, "flow-mod cmd=add,table=0,prio=2
    // eth_type=0x0800 apply:output=normal");
}

void
OfppPythonController::AddFlow(uint64_t dpId,
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

    // WARNING: buffer_id for the purpose of this function should never be set in the stream!! :bug
    // on the ofsoftswitch13

    std::ostringstream cmd;
    // cmd << "flow-mod cmd=add,table=0,flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=ff:ff:ff:ff:ff:ff apply:output=1";
    // DpctlExecute(dpId, cmd.str());

    cmd.str("");
    cmd.clear();
    cmd << "flow-mod cmd=add,table=0,idle=" << idle_timeout << ",hard=" << hard_timeout
        << ",flags=0x0001,prio=" << ++prio;

    // cmd << "flow-mod cmd=add,table=0,idle="<< idle_timeout <<",flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=" << src48
    //     << " apply:output=" << inPort;
    cmd << " " << match.str();

    cmd << " "
        << actions.str(); //  << " apply:set_field=eth_dst:" << mac_out << ",output=" << outPort;

    std::cout << cmd.str() << std::endl;

    NS_LOG_INFO(cmd.str());
    DpctlExecute(dpId, cmd.str());
}

void
OfppPythonController::RemoveFlow(uint64_t dpId,
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

    // cmd << "flow-mod cmd=add,table=0,idle="<< idle_timeout <<",flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=" << src48
    //     << " apply:output=" << inPort;
    cmd << " " << match;

    cmd << " " << actions; //  << " apply:set_field=eth_dst:" << mac_out << ",output=" << outPort;

    NS_LOG_INFO(cmd.str());
    DpctlExecute(dpId, cmd.str());
}

void
OfppPythonController::ModifyFlow(uint64_t dpId,
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

    // TODO cmd=mod ERROR in flow_entry_replace_instructions at
    // sources/udatapath/flow_entry.c:140

    std::ostringstream cmd;
    cmd << "flow-mod cmd=add,table=0,idle=" << idle_timeout << ",hard=" << hard_timeout
        << ",flags=0x0001,prio=" << ++prio;

    // cmd << "flow-mod cmd=add,table=0,idle="<< idle_timeout <<",flags=0x0001"
    //     << ",prio=" << ++prio << " eth_dst=" << src48
    //     << " apply:output=" << inPort;
    cmd << " " << match;

    cmd << " " << actions; //  << " apply:set_field=eth_dst:" << mac_out << ",output=" << outPort;

    NS_LOG_INFO(cmd.str());
    DpctlExecute(dpId, cmd.str());
}

void
OfppPythonController::SendPacket_Out(Ptr<const RemoteSwitch> swtch,
                                     uint32_t buffer_id,
                                     uint32_t in_port,
                                     size_t data_length,
                                     uint8_t* data,
                                     uint32_t out_port,
                                     Mac48Address& set_Eth_Dst,
                                     uint32_t xid)

{
    Mac48Address empty_mac;
    // output port normal cannot be set with set field at the same time
    NS_ASSERT_MSG(!(out_port == OFPP_NORMAL && set_Eth_Dst != empty_mac),
                  "When out_port is OFPP_NORMAL, Set Eth Field cannot be implemented.");
    std::cout << " \n Packet OUT --> " << out_port << " " << set_Eth_Dst << std::endl;

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
        std::cout << "SET Eth FIELD\n";
    }
    else
    {
        reply.actions_num = 1;
        reply.actions = (struct ofl_action_header**)&a;
    }
    std::cout << "\n -------- \n BUFFER: " << reply.buffer_id << std::endl;

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
OfppPythonController::Get_ETH_SRC(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address src48;
    input = oxm_match_lookup(OXM_OF_ETH_SRC, match);
    src48.CopyFrom(input->value);
    return src48;
}

Mac48Address
OfppPythonController::Get_ETH_DST(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address dst48;
    input = oxm_match_lookup(OXM_OF_ETH_DST, match);
    dst48.CopyFrom(input->value);
    return dst48;
}

uint32_t
OfppPythonController::Get_IN_PORT(struct ofl_match* match)
{
    uint32_t inPort;
    struct ofl_match_tlv* input;
    input = oxm_match_lookup(OXM_OF_IN_PORT, match);
    memcpy(&inPort, input->value, OXM_LENGTH(OXM_OF_IN_PORT));
    return inPort;
}

Ipv4Address
OfppPythonController::Get_IPV4_DST(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_DST, match);
}

Ipv4Address
OfppPythonController::Get_IPV4_SRC(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_SRC, match);
}

Mac48Address
OfppPythonController::Get_ARP_SHA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_SHA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
OfppPythonController::Get_ARP_SPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_SPA, match);
}

Mac48Address
OfppPythonController::Get_ARP_THA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_THA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
OfppPythonController::Get_ARP_TPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_TPA, match);
}

uint8_t
OfppPythonController::Get_IP_PROTO(struct ofl_match* match)
{
    uint8_t ip_proto;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_IP_PROTO, match);
    memcpy(&ip_proto, tlv->value, OXM_LENGTH(OXM_OF_IP_PROTO));
    return ip_proto;
}

uint32_t
OfppPythonController::Get_UDP_DST(struct ofl_match* match)
{
    uint32_t UDPPort = 0;
    size_t UDP_portLen = OXM_LENGTH(OXM_OF_UDP_DST); // (Always 2 bytes)
    struct ofl_match_tlv* udpSrc = oxm_match_lookup(OXM_OF_UDP_DST, match);
    memcpy(&UDPPort, udpSrc->value, UDP_portLen);
    return UDPPort;
}

uint16_t
OfppPythonController::Get_ETH_TYPE(struct ofl_match* match)
{
    uint16_t ethType;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ETH_TYPE, match);
    memcpy(&ethType, tlv->value, OXM_LENGTH(OXM_OF_ETH_TYPE));
    return ethType;
}

uint16_t
OfppPythonController::Get_ARP_TYPE(struct ofl_match* match)
{
    // Get ARP operation
    uint16_t arpOp;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ARP_OP, match);
    memcpy(&arpOp, tlv->value, OXM_LENGTH(OXM_OF_ARP_OP));
    return arpOp;
}

Ipv4Address
OfppPythonController::ExtractIpv4Address(uint32_t oxm_of, struct ofl_match* match)
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
