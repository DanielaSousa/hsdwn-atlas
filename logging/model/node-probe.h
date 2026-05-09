#ifndef NODE_NodeProbe_H_
#define NODE_NodeProbe_H_

#include <map>
#include <vector>

#include "ns3/nstime.h"
#include "ns3/object.h"

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/queue-item.h"
#include <mutex>
#include "ns3/yans-wifi-helper.h"

namespace ns3{

class Logging;
class Node;

class NodeProbe: public Object
{

public:
static TypeId GetTypeId (void);

    NodeProbe(Ptr<Logging> monitor, Ptr<Node> node);
    virtual ~NodeProbe();
    Ptr<Node> GetNode();
    void ClearTimestampVariables();
    void initVariables();

protected:
    virtual void DoDispose (void);
    Ptr<Logging> m_monitor;
    Ptr<Node> m_node;

    mutable std::mutex logging_mutex;
    // Thread-safe methods to increase counters
    //phy
    void inc_snr(double snr);
    void inc_phy_listened();
    void inc_phy_droped();

	void inc_packets_created();
    void inc_packets_received();
    void inc_packets_rx();
    void inc_bytes_rx(int size);
    void inc_packets_tx();
    void inc_bytes_tx(int size);
	void inc_packets_repeated_per_timestamp();
	void inc_packets_listened_per_timestamp();

    void inc_tx_broadcast();
    void inc_rx_broadcast();

    void inc_tx_opf_control();
    void inc_rx_opf_control();

    void inc_delay(Time delay);

    //controll
	void inc_control_packets_number_per_timestamp();
	void inc_control_packets_size_per_timestamp(uint32_t packet_size);


    void inc_forward();
    void inc_normal_action();

void set_energy(double remainingEnergy);





private:
    Ptr<Ipv4L3Protocol> m_ipv4; //!< the Ipv4L3Protocol this NodeProbe is bound to
    // falta ARP

  /// Log a packet being sent
  /// \param ipHeader IP header
  /// \param ipPayload IP payload
  /// \param interface outgoing interface
  void SendOutgoingLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);

  /// Log a packet being received by the destination
  /// \param ipHeader IP header
  /// \param ipPayload IP payload
  /// \param interface incoming interface
  void ForwardUpLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);


    void MacTx(Ptr< const Packet > packet);
    void MacRx(Ptr< const Packet > packet);

    void GetEnergy (double oldValue, double remainingEnergy);

    // phy
    void SniffRx (Ptr<const Packet> packet,
                     uint16_t channelFreqMhz,
                     WifiTxVector txVector,
                     MpduInfo aMpdu,
                     SignalNoiseDbm signalNoise,
                     uint16_t staId);
    void PhyRxDrop(Ptr<const Packet> p, WifiPhyRxfailureReason reason);
    void OpfNormalAction(Ptr< const Packet > packet);

    //void MacOpenFlowRx(Ptr< const Packet > packet);
    //void MacBroadcastTx(Ptr< const Packet > packet);

public:
    // variables
    // physical
    int phy_snr_sum;
    uint32_t phy_total_droped;
    uint32_t phy_total_listened;

    // mac layer
	uint32_t packets_tx_total;
    uint32_t packets_tx;
    uint64_t bytes_tx_total; // txBytes: Total number of transmitted bytes for the node
    uint64_t bytes_tx;

    uint32_t packets_rx_total;
    uint32_t packets_rx;
    uint64_t bytes_rx_total; // rxBytes: Total number of received bytes for the node
    uint64_t bytes_rx;

    uint32_t packet_tx_forwarded_total; // number of forwarded operations made
    //TODO nao tenho infrasctrutura para esta medida --> uint32_t packets_repeated_total; // with the trackedPacket

    // ipv4
    uint32_t packets_rcv; // local delivery ipv4L3
	uint32_t packets_rcv_total;

    uint32_t packets_own; // packets created by me : only ipv4L3
	uint32_t packets_own_total;

    uint32_t packets_Tx_Bcast_total; // ipv4 header, in our case this most likely is OLSR
    uint32_t packets_Rx_Bcast_total;

    // openflow
    uint32_t packets_opf_action_normal;
    uint32_t packets_opf_rcv_total; //openflow ipv4 messages
    uint32_t packets_opf_own_total;

    // node
    uint32_t energy;
    uint32_t n_contacts; // neighbour


    // from flow-monitor
    Time     delaySum_total;
    Time     delaySum; // delayCount == rxPackets
    Time     jitterSum; // jitterCount == rxPackets - 1

    /// Contains the last measured delay of a packet
    /// It is stored to measure the packet's Jitter
    Time     lastDelay;


};




};

#endif /* NODE_NodeProbe_H_ */