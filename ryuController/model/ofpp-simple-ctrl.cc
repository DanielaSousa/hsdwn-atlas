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

#include "ofpp-simple-ctrl.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OfppSimpleController");
NS_OBJECT_ENSURE_REGISTERED(OfppSimpleController);

/********** Public methods ***********/
OfppSimpleController::OfppSimpleController()
{
}

OfppSimpleController::~OfppSimpleController()
{
    NS_LOG_FUNCTION(this);
}

TypeId
OfppSimpleController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OfppSimpleController")
                            .SetParent<OFSwitch13Controller>()
                            .SetGroupName("OFSwitch13")
                            .AddConstructor<OfppSimpleController>();
    return tid;
}

void
OfppSimpleController::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_learnedInfo.clear();
    OFSwitch13Controller::DoDispose();
}

ofl_err
OfppSimpleController::HandlePacketIn(struct ofl_msg_packet_in* msg,
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
            actions << "apply:output=normal";
            outPort = OFPP_NORMAL;
            AddFlow(dpId, prioCounter, match, actions, NO_BUFFER);
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
} // namespace ns3

ofl_err
OfppSimpleController::HandleExperimenter(struct ofl_msg_experimenter* msg,
                                         Ptr<const RemoteSwitch> swtch,
                                         uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);
    NS_LOG_DEBUG("Handle Experimenter");
    ofl_msg_free_experimenter(msg, nullptr);
    return 0;
}

ofl_err
OfppSimpleController::HandleFlowRemoved(struct ofl_msg_flow_removed* msg,
                                        Ptr<const RemoteSwitch> swtch,
                                        uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    // All handlers must free the message when everything is ok
    ofl_msg_free_flow_removed(msg, true, nullptr);
    return 0;
}

/********** Private methods **********/
void
OfppSimpleController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch)
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
    // DpctlExecute(swDpId, "flow-mod cmd=add,table=0,prio=1 eth_type=0x0806 apply:output=normal");
    // DpctlExecute(swDpId, "flow-mod cmd=add,table=0,prio=2 eth_type=0x0800 apply:output=normal");
}

void
OfppSimpleController::AddFlow(uint64_t dpId,
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
OfppSimpleController::RemoveFlow(uint64_t dpId,
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
OfppSimpleController::ModifyFlow(uint64_t dpId,
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
OfppSimpleController::SendPacket_Out(Ptr<const RemoteSwitch> swtch,
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
OfppSimpleController::Get_ETH_SRC(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address src48;
    input = oxm_match_lookup(OXM_OF_ETH_SRC, match);
    src48.CopyFrom(input->value);
    return src48;
}

Mac48Address
OfppSimpleController::Get_ETH_DST(struct ofl_match* match)
{
    struct ofl_match_tlv* input;
    Mac48Address dst48;
    input = oxm_match_lookup(OXM_OF_ETH_DST, match);
    dst48.CopyFrom(input->value);
    return dst48;
}

uint32_t
OfppSimpleController::Get_IN_PORT(struct ofl_match* match)
{
    uint32_t inPort;
    struct ofl_match_tlv* input;
    input = oxm_match_lookup(OXM_OF_IN_PORT, match);
    memcpy(&inPort, input->value, OXM_LENGTH(OXM_OF_IN_PORT));
    return inPort;
}

Ipv4Address
OfppSimpleController::Get_IPV4_DST(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_DST, match);
}

Ipv4Address
OfppSimpleController::Get_IPV4_SRC(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_IPV4_SRC, match);
}

Mac48Address
OfppSimpleController::Get_ARP_SHA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_SHA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
OfppSimpleController::Get_ARP_SPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_SPA, match);
}

Mac48Address
OfppSimpleController::Get_ARP_THA(struct ofl_match* match)
{
    struct ofl_match_tlv* tlv;
    Mac48Address srcMac;
    tlv = oxm_match_lookup(OXM_OF_ARP_THA, match);
    srcMac.CopyFrom(tlv->value);
    return srcMac;
}

Ipv4Address
OfppSimpleController::Get_ARP_TPA(struct ofl_match* match)
{
    return ExtractIpv4Address(OXM_OF_ARP_TPA, match);
}

uint8_t
OfppSimpleController::Get_IP_PROTO(struct ofl_match* match)
{
    uint8_t ip_proto;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_IP_PROTO, match);
    memcpy(&ip_proto, tlv->value, OXM_LENGTH(OXM_OF_IP_PROTO));
    return ip_proto;
}

uint32_t
OfppSimpleController::Get_UDP_SRC(struct ofl_match* match)
{
    uint32_t UDPPort = 0;
    size_t UDP_portLen = OXM_LENGTH(OXM_OF_UDP_SRC); // (Always 2 bytes)
    struct ofl_match_tlv* udpSrc = oxm_match_lookup(OXM_OF_UDP_SRC, match);
    memcpy(&UDPPort, udpSrc->value, UDP_portLen);
    return UDPPort;
}

uint16_t
OfppSimpleController::Get_ETH_TYPE(struct ofl_match* match)
{
    uint16_t ethType;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ETH_TYPE, match);
    memcpy(&ethType, tlv->value, OXM_LENGTH(OXM_OF_ETH_TYPE));
    return ethType;
}

uint16_t
OfppSimpleController::Get_ARP_TYPE(struct ofl_match* match)
{
    // Get ARP operation
    uint16_t arpOp;
    struct ofl_match_tlv* tlv;
    tlv = oxm_match_lookup(OXM_OF_ARP_OP, match);
    memcpy(&arpOp, tlv->value, OXM_LENGTH(OXM_OF_ARP_OP));
    return arpOp;
}

Ipv4Address
OfppSimpleController::ExtractIpv4Address(uint32_t oxm_of, struct ofl_match* match)
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
