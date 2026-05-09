/*
dcsousa@ua.pt
*/

#ifndef OFPP_NORMAL_CONTROLLER_H
#define OFPP_NORMAL_CONTROLLER_H

#include "utils.h"

#include "ns3/ipv4-address.h"
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-controller.h>
#include <ns3/ofswitch13-module.h>

#include <Python.h>
#include <tuple>
#include <vector>

namespace ns3
{

/**
 * \ingroup ofswitch13
 * \brief An Learning OpenFlow 1.3 controller (works as L2 switch)
 */
class OfppNormalController : public OFSwitch13Controller
{
  public:
    OfppNormalController(std::string abspath = ""); //!< Default constructor
    ~OfppNormalController() override;               //!< Dummy destructor.

    /**
     * Register this type.
     * \return The object TypeId.
     */
    static TypeId GetTypeId();

    /** Destructor implementation */
    void DoDispose() override;

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

  protected:
    // Inherited from OFSwitch13Controller
    void HandshakeSuccessful(Ptr<const RemoteSwitch> swtch) override;

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
    uint16_t Get_ETH_TYPE(struct ofl_match* match);

    /**
     * @brief Recalculate paths
     *
     * @param dpId
     * @param ip_dst
     */
    // void RecalculatePathsWithoutSpread(uint64_t dpId);
    // void RecalculatePathsToIp(uint64_t dpId, Ipv4Address ip);

    /** Map saving <IPv4 address / MAC address> */
    typedef std::map<Ipv4Address, Mac48Address> IpMacMap_t;
    IpMacMap_t m_arpTable; //!< ARP resolution table.

    // /**
    //  * \name L2 switching structures
    //  */
    // //\{
    // /** L2SwitchingTable: map MacAddress to port */
    // typedef std::map<Mac48Address, uint32_t> L2Table_t;

    // /** Map datapathID to L2SwitchingTable */
    // typedef std::map<uint64_t, L2Table_t> DatapathMap_t;

    // /** Switching information for all dapataths */
    // DatapathMap_t m_learnedInfo;
    // //\}

    /**
     * \name L3 routing structures
     */
    //\{
    typedef std::tuple<Mac48Address, uint32_t, std::string, int, std::string>
        ForwardingRule_t; // dstMac, out_port, match.str(), prioCounter, actions.str()
    /** L3RoutingTable: map IPv4 to (next MacAddress, my port) */
    typedef std::map<Ipv4Address, ForwardingRule_t> L3Table_t;

    /** Map datapathID to L3RoutingTable */
    typedef std::map<uint64_t, L3Table_t> DatapathMap_t;

    /** Routing information for all dapataths */
    DatapathMap_t m_learnedInfo; // routes: {dpid: {IP_dst: (next_mac, outPort, match}}

    //\}
    typedef std::map<uint64_t, int> CountMap_t;
    CountMap_t m_experimenterCount;

    PyObject* pModule;
    int prioCounter = 0;
};

} // namespace ns3
#endif /* OFSWITCH13_LEARNING_CONTROLLER_H */
