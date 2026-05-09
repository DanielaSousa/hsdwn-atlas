/*
dcsousa@ua.pt
*/

#ifndef HYBRID_SDN_CONTROLLER_H
#define HYBRID_SDN_CONTROLLER_H

#include <ns3/ofswitch13-controller.h>

namespace ns3
{

/**
 * \ingroup ofswitch13
 * \brief An Learning OpenFlow 1.3 controller (works as L2 switch)
 */
class HybridSDNController : public OFSwitch13Controller
{
  public:
    HybridSDNController();           //!< Default constructor
    ~HybridSDNController() override; //!< Dummy destructor.

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

    ofl_err HandleExperimenter (struct ofl_msg_experimenter *msg, Ptr<const RemoteSwitch> swtch,
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
    /** Map saving <IPv4 address / MAC address> */
    typedef std::map<Ipv4Address, Mac48Address> IpMacMap_t;
    IpMacMap_t m_arpTable; //!< ARP resolution table.

    /**
     * \name L2 switching structures
     */
    //\{
    /** L2SwitchingTable: map MacAddress to port */
    typedef std::map<Mac48Address, uint32_t> L2Table_t;

    /** Map datapathID to L2SwitchingTable */
    typedef std::map<uint64_t, L2Table_t> DatapathMap_t;

    /** Switching information for all dapataths */
    DatapathMap_t m_learnedInfo;
    //\}
};

} // namespace ns3
#endif /* OFSWITCH13_LEARNING_CONTROLLER_H */
