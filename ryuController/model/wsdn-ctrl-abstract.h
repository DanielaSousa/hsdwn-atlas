/*
// Abstract class for a WSDN controller
*/

#ifndef WSDN_CTRL_ABSTRACT_H
#define WSDN_CTRL_ABSTRACT_H

#include "path.h"
#include "utils.h"

#include "ns3/ipv4-address.h"
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-controller.h>

#include <Python.h>
#include <mutex>
#include <tuple>
#include <vector>

namespace ns3
{

/**
 * \ingroup ofswitch13
 * \brief An Learning OpenFlow 1.3 controller (works as L2 switch)
 */
class WSDNCtrlAbstract : public OFSwitch13Controller
{
  public:
    WSDNCtrlAbstract(std::string ryu_config =
                         "/home/phd/Documents/PhD/RYU/ryu_config.txt"); //!< Default constructor
    ~WSDNCtrlAbstract() override;                                       //!< Dummy destructor.

    struct pOutRule
    {
        uint32_t out_port;
        Mac48Address set_Eth_Dst;
    };

    /**
     * Register this type.
     * \return The object TypeId.
     */
    static TypeId GetTypeId();

    /** Destructor implementation */
    void DoDispose() override;

    virtual pOutRule Handle_Ipv4(uint64_t dpId,
                                 uint8_t ip_proto,
                                 struct ofl_msg_packet_in* msg,
                                 Mac48Address src48,
                                 Ipv4Address srcIp,
                                 Mac48Address dst48,
                                 Ipv4Address dstIp);

    void Handle_OLSR(uint64_t dpId,
                     struct ofl_msg_packet_in* msg,
                     Mac48Address src48,
                     Ipv4Address srcIp);
    void Handle_ARP(uint64_t dpId, struct ofl_msg_packet_in* msg);
    void Handle_ARP_Normal(uint64_t dpId, struct ofl_msg_packet_in* msg);

    /**
     * Handle packet-in messages sent from switch to this controller. Look for
     * L2 switching information, update the structures and send a packet-out
     * back.
     *
     * \param msg The packet-in message.
     * \param swtch The switch information.
     * \param xid Transaction id.
     * \return 0 if everything's ok, otherwise an error number.
     */
    ofl_err HandlePacketIn(struct ofl_msg_packet_in* msg,
                           Ptr<const RemoteSwitch> swtch,
                           uint32_t xid) override;

    ofl_err HandleExperimenter(struct ofl_msg_experimenter* msg,
                               Ptr<const RemoteSwitch> swtch,
                               uint32_t xid) override;

    /**
     * Handle flow removed messages sent from switch to this controller. Look
     * for L2 switching information and removes associated entry.
     *
     * \param msg The flow removed message.
     * \param swtch The switch information.
     * \param xid Transaction id.
     * \return 0 if everything's ok, otherwise an error number.
     */
    ofl_err HandleFlowRemoved(struct ofl_msg_flow_removed* msg,
                              Ptr<const RemoteSwitch> swtch,
                              uint32_t xid) override;

    virtual void RecalculatePaths();
    virtual int getServiceType(ofl_msg_packet_in* msg);

  protected:
    // Inherited from OFSwitch13Controller
    void HandshakeSuccessful(Ptr<const RemoteSwitch> swtch) override;

    int CreateFlow(FlowKey_t f);
    bool FlowExists(FlowKey_t f);

  private:
    /**
     * @brief Function to help add a flow the hybrid openflow switch
     *
     * @param dpId
     * @param priority
     * @param match
     * @param actions
     * @param buffer_id
     * @param idle_timeout
     * @param hard_timeout
     */
    void AddFlow(uint64_t dpId,
                 int& priority,
                 std::ostringstream& match,
                 std::ostringstream& actions,
                 uint32_t buffer_id = NO_BUFFER,
                 int idle_timeout = 65535,
                 int hard_timeout = 65535);

    void RemoveFlow(uint64_t dpId,
                    int priority,
                    std::string match,
                    std::string actions,
                    uint32_t buffer_id = NO_BUFFER,
                    int idle_timeout = 65535,
                    int hard_timeout = 65535);

    void ModifyFlow(uint64_t dpId,
                    int& priority,
                    std::string match,
                    std::string actions,
                    uint32_t buffer_id = NO_BUFFER,
                    int idle_timeout = 65535,
                    int hard_timeout = 65535);

    /**
     * @brief
     * @param swtch
     * @param buffer_id
     * @param in_port
     * @param data_length only if NO_BUFFER
     * @param data only if NO_BUFFER
     * @param out_port
     * @param set_Eth_Dst can be null if no eth_dst is to be set
     */
    void SendPacket_Out(Ptr<const RemoteSwitch> swtch,
                        uint32_t buffer_id,
                        uint32_t in_port,
                        size_t data_length,
                        uint8_t* data,
                        uint32_t out_port,
                        Mac48Address& set_Eth_Dst,
                        uint32_t xid);

    Mac48Address Get_ETH_SRC(struct ofl_match* match);
    Mac48Address Get_ETH_DST(struct ofl_match* match);
    uint32_t Get_IN_PORT(struct ofl_match* match);
    Ipv4Address Get_IPV4_DST(struct ofl_match* match);
    Ipv4Address Get_IPV4_SRC(struct ofl_match* match);
    Mac48Address Get_ARP_SHA(struct ofl_match* match);
    Ipv4Address Get_ARP_SPA(struct ofl_match* match);
    Mac48Address Get_ARP_THA(struct ofl_match* match);
    Ipv4Address Get_ARP_TPA(struct ofl_match* match);
    uint16_t Get_ARP_TYPE(struct ofl_match* match);
    Ipv4Address ExtractIpv4Address(uint32_t oxm_of, struct ofl_match* match);

    uint8_t Get_IP_PROTO(struct ofl_match* match);
    uint32_t Get_UDP_SRC(struct ofl_match* match);
    uint32_t Get_UDP_DST(struct ofl_match* match);
    uint16_t Get_ETH_TYPE(struct ofl_match* match);

    void RecalculatePathsPeriodically();

    /** Map saving <IPv4 address / MAC address> */
    typedef std::map<Ipv4Address, Mac48Address> IpMacMap_t;
    IpMacMap_t m_arpTable; //!< ARP resolution table.

    std::map<FlowKey_t, MultiPath> m_paths; // m_paths[flow_key] = MultiPath()

    std::map<uint64_t, int> m_experimenterCount;

    PyObject* pModule;
    int prioCounter = 0;

    std::ostringstream actions, match;
    uint32_t outPort = OFPP_NORMAL;
    int changes = -1;

    std::mutex graph_mutex;
};

} // namespace ns3
#endif /* OFSWITCH13_LEARNING_CONTROLLER_H */
