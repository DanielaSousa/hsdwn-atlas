/*
* Adapted from the ofswitch13 - LearningController
 */

#ifdef NS3_OFSWITCH13

#include "hybrid-sdn-controller.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("HybridSDNController");
NS_OBJECT_ENSURE_REGISTERED(HybridSDNController);

/********** Public methods ***********/
HybridSDNController::HybridSDNController()
{
    NS_LOG_FUNCTION(this);
}

HybridSDNController::~HybridSDNController()
{
    NS_LOG_FUNCTION(this);
}

TypeId
HybridSDNController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::HybridSDNController")
                            .SetParent<OFSwitch13Controller>()
                            .SetGroupName("HybridSDN")
                            .AddConstructor<HybridSDNController>();
    return tid;
}

void
HybridSDNController::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_learnedInfo.clear();
    OFSwitch13Controller::DoDispose();
}

ofl_err
HybridSDNController::HandlePacketIn(struct ofl_msg_packet_in* msg,
                                             Ptr<const RemoteSwitch> swtch,
                                             uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    uint32_t outPort = OFPP_NORMAL;

    static int prio = 100;
    //uint32_t outPort = OFPP_FLOOD;
    enum ofp_packet_in_reason reason = msg->reason;

    // Get the switch datapath ID
    uint64_t swDpId = swtch->GetDpId();

    NS_LOG_DEBUG (swDpId << prio);
    char* msgStr =
        ofl_structs_match_to_string((struct ofl_match_header*)msg->match,
                                    nullptr);
    NS_LOG_DEBUG("Packet in match: " << msgStr);
    free(msgStr);

    if (reason == OFPR_NO_MATCH)
    {
        // Let's get necessary information (input port and mac address)
        uint32_t inPort;
        size_t portLen = OXM_LENGTH(OXM_OF_IN_PORT); // (Always 4 bytes)
        struct ofl_match_tlv* input =
            oxm_match_lookup(OXM_OF_IN_PORT, (struct ofl_match*)msg->match);
        memcpy(&inPort, input->value, portLen);

        Mac48Address src48;
        struct ofl_match_tlv* ethSrc =
            oxm_match_lookup(OXM_OF_ETH_SRC, (struct ofl_match*)msg->match);
        src48.CopyFrom(ethSrc->value);

        Mac48Address dst48;
        struct ofl_match_tlv* ethDst =
            oxm_match_lookup(OXM_OF_ETH_DST, (struct ofl_match*)msg->match);
        dst48.CopyFrom(ethDst->value);

        /*// Get L2Table for this datapath
        auto it = m_learnedInfo.find(swDpId);
        if (it != m_learnedInfo.end())
        {
            L2Table_t* l2Table = &it->second;

            // Looking for out port based on dst address (except for broadcast)
            if (!dst48.IsBroadcast())
            {
                auto itDst = l2Table->find(dst48);
                if (itDst != l2Table->end())
                {
                    outPort = itDst->second;
                }
                else
                {
                    NS_LOG_DEBUG("No L2 info for mac " << dst48 << ". Flood.");
                }
            }

            // Learning port from source address
            NS_ASSERT_MSG(!src48.IsBroadcast(), "Invalid src broadcast addr");
            auto itSrc = l2Table->find(src48);
            if (itSrc == l2Table->end())
            {
                std::pair<Mac48Address, uint32_t> entry(src48, inPort);
                auto ret = l2Table->insert(entry);
                if (ret.second == false)
                {
                    NS_LOG_ERROR("Can't insert mac48address / port pair");
                }
                else
                {
                    NS_LOG_DEBUG("Learning that mac "
                                 << src48 << " can be found at port "
                                 << inPort);

                    // Send a flow-mod to switch creating this flow. Let's
                    // configure the flow entry to 10s idle timeout and to
                    // notify the controller when flow expires. (flags=0x0001)
                    std::ostringstream cmd;
                    cmd << "flow-mod cmd=add,table=0,idle=10,flags=0x0001"
                        << ",prio=" << ++prio << " eth_dst=" << src48
                        << " apply:output=" << inPort;
                    DpctlExecute(swDpId, cmd.str());
                }
            }
            else
            {
                NS_ASSERT_MSG(itSrc->second == inPort,
                              "Inconsistent L2 switching table");
            }
        }
        else
        {
            NS_LOG_ERROR("No L2 table for this datapath id " << swDpId);
        }*/

        // Lets send the packet out to switch.
        struct ofl_msg_packet_out reply;
        reply.header.type = OFPT_PACKET_OUT;
        reply.buffer_id = msg->buffer_id;
        reply.in_port = inPort;
        reply.data_length = 0;
        reply.data = nullptr;

        if (msg->buffer_id == NO_BUFFER)
        {
            // No packet buffer. Send data back to switch
            reply.data_length = msg->data_length;
            reply.data = msg->data;
        }

        // Create output action
        struct ofl_action_output* a = (struct ofl_action_output*)xmalloc(
            sizeof(struct ofl_action_output));
        a->header.type = OFPAT_OUTPUT;
        a->port = outPort;
        a->max_len = 0;

        reply.actions_num = 1;
        reply.actions = (struct ofl_action_header**)&a;

        SendToSwitch(swtch, (struct ofl_msg_header*)&reply, xid);
        free(a);
    }
    else
    {
        NS_LOG_WARN("This controller can't handle the packet. Unkwnon reason.");
    }

    // All handlers must free the message when everything is ok
    ofl_msg_free((struct ofl_msg_header*)msg, nullptr);
    return 0;
}

ofl_err HybridSDNController::HandleExperimenter (struct ofl_msg_experimenter *msg, Ptr<const RemoteSwitch> swtch,
    uint32_t xid)
    {
        NS_LOG_FUNCTION(this << swtch << xid);
        NS_LOG_DEBUG("Handle Experimenter");
        return 0;
    }

ofl_err
HybridSDNController::HandleFlowRemoved(
    struct ofl_msg_flow_removed* msg,
    Ptr<const RemoteSwitch> swtch,
    uint32_t xid)
{
    NS_LOG_FUNCTION(this << swtch << xid);

    // Get the switch datapath ID
    uint64_t swDpId = swtch->GetDpId();

    NS_LOG_DEBUG("Flow entry expired. Removing from L2 switch table.");
    auto it = m_learnedInfo.find(swDpId);
    if (it != m_learnedInfo.end())
    {
        Mac48Address mac48;
        struct ofl_match_tlv* ethSrc =
            oxm_match_lookup(OXM_OF_ETH_DST,
                             (struct ofl_match*)msg->stats->match);
        mac48.CopyFrom(ethSrc->value);

        L2Table_t* l2Table = &it->second;
        auto itSrc = l2Table->find(mac48);
        if (itSrc != l2Table->end())
        {
            l2Table->erase(itSrc);
        }
    }

    // All handlers must free the message when everything is ok
    ofl_msg_free_flow_removed(msg, true, nullptr);
    return 0;
}

/********** Private methods **********/
void
HybridSDNController::HandshakeSuccessful(Ptr<const RemoteSwitch> swtch)
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

    // Configure te switch to buffer packets and send only the first 128 bytes
    // of each packet sent to the controller when not using an output action to
    // the OFPP_CONTROLLER logical port.
    DpctlExecute(swDpId, "set-config miss=128");

    // Create an empty L2SwitchingTable and insert it into m_learnedInfo
    L2Table_t l2Table;
    std::pair<uint64_t, L2Table_t> entry(swDpId, l2Table);
    auto ret = m_learnedInfo.insert(entry);
    if (ret.second == false)
    {
        NS_LOG_ERROR("Table exists for this datapath.");
    }
}

} // namespace ns3
#endif // NS3_OFSWITCH13
