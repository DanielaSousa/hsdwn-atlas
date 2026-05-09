/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef QUALITY_MONITOR_H
#define QUALITY_MONITOR_H

#include "ns3/log.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include <ns3/core-module.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/internet-module.h>
#include <ns3/network-module.h>
// energy

#include "ns3/config-store-module.h"
#include "ns3/energy-module.h"
#include "ns3/event-id.h"
#include "ns3/flow-classifier.h"
#include "ns3/flow-id-tag.h"
#include "ns3/flow-probe.h"
#include "ns3/histogram.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-flow-probe.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/wifi-radio-energy-model-helper.h"

#include <iostream>
#include <map>
#include <mutex> // std::mutex
#include <string>
#include <vector>

/**
 * Flow Monitor module is designed in a modular way. It can be extended by subclassing
 * ns3::FlowProbe and ns3::FlowClassifier. Typically, a subclass of ns3::FlowProbe works
 * by listening to the appropriate class Traces, and then uses its own ns3::FlowClassifier
 * subclass to classify the packets passing though each node.
 *
 */
namespace ns3
{

struct MyFlowStats
{
    bool valid = false;
    Time avgDelay;
    double avgRssi = 0.0; // in dBm
    double avgSnr = 0.0;
    double avgThroughput = 0.0; // bps //https://www.pcwdld.com/network-throughput

    // unicast
    int n_pktLost = 0;
    int u_rxbytes = 0;
    int u_rxPackets = 0;
    int u_txPackets = 0;
    double u_delivery_ratio = 0.0;
    Ipv4Address src;
    Ipv4Address dst;
};

class AttributeValue;
class Ipv4FlowClassifier;
class Node;

class QualityMonitor
{
  public:
    QualityMonitor(/* args */);
    ~QualityMonitor();

    /**
     * @brief Install a quality monitor on each device
     *
     * @param nodes
     * @return int error or success
     */
    int Install(NodeContainer nodes, int index);
    int Install(Ptr<ns3::Node> node, int index);

    // Ptr<FlowMonitor> Install (NodeContainer nodes);
    /**
     * @brief trace function on radiotapheader. If troughput is 0, then the packet gets droped (is
     * nor suppost to reach this device)
     *
     * @param packet the packet received by the mac layer (layer 2)
     * @param channelFreqMhz
     * @param txVector
     * @param aMpdu
     * @param signalNoise
     * @param staId
     */
    void MonitorSniffRx(std::string context,
                        Ptr<const Packet> packet,
                        uint16_t channelFreqMhz,
                        WifiTxVector txVector,
                        MpduInfo aMpdu,
                        SignalNoiseDbm signalNoise,
                        uint16_t staId);

    // --- basic methods ---
    //    static TypeId GetTypeId ();
    //    virtual TypeId GetInstanceTypeId () const;

    void CheckForLostPackets(Time maxDelay);

    void AddStats(uint32_t flowId, uint32_t packetId, uint32_t packetSize, Time delay);
    void ErasePacket();

    void CheckForOldPackets(Time maxDelay);

    // Ipv4FlowClassifier::FiveTuple GetFlowId();
    Ipv4Address GetSourceFromFlowId(FlowId flowid);

    // from ipv4flowprobe
    //  void ReportFirstTx( Ptr<Node> node, uint32_t interface, const Ipv4Header &ipHeader,
    //  Ptr<const Packet> ipPayload); void ReportRx( Ptr<Node> node, uint32_t interface, const
    //  Ipv4Header &ipHeader, Ptr<const Packet> ipPayload); void ReportLastRx( Ptr<Node> node,
    //  uint32_t interface, const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload);

    void MonitorMacTx(std::string context, Ptr<const Packet> packet);
    void MonitorMacRx(std::string context, Ptr<const Packet> packet);
    /**
     * @brief Get the Stats object of every flow starting in node n
     *
     * @param n src of a flow
     * @return std::map<Ipv4Address, std::vector <MyFlowStats>>
     */
    std::map<Ipv4Address, std::vector<MyFlowStats>> GetStats(Ipv4Address n);

    /**
     * @brief Get the Flow Stats object of a flow from src to dst
     *
     * @param src
     * @param dst
     * @return MyFlowStats
     */
    MyFlowStats GetFlowStats(Ipv4Address src, Ipv4Address dst);
    int pesos[6];

  protected:
    virtual void DoDispose(void);

  private:
    /// Structure to represent a single tracked packet data

    struct TxPacketStats
    {
        int PacketId;
        Time firstTime; //!< absolute time when the packet was received by the node
    };

    struct PhyPacketStats
    {
        // physical measurments
        double rssi;  // in dBm
        double noise; // in dBm
        double snr;   // in dBm
        // double sinr;

        // ns3::Packet lastpacketreceived;
        Time phyTime; // absolute time when the packet arrived at the phy layer
    };

    struct RxPacketStats
    {
        PhyPacketStats phy;
        Time receptionTime;  //!< absolute time when the packet was received by the node
        Time delay;          // delay
        uint32_t packetSize; // the size in bytes of the packet
        Ipv4Address src;
        Ipv4Address dst;
    };

    std::map<std::pair<int, int>, PhyPacketStats> lastPacket;

    // nao tenho garantia de nao ter sido lost, mas se nao há maneira de o distinguir nao consigo
    // fazer mais nada
    //  soluçao -> o max delay < (menor) que o resend no mesmo nó de pacotes exatamento iguais
    // std::map < Ptr<const Packet> , std::vector<Time> > txPacket; // TODO ir apagando no check for
    // old packets
    std::map<uint64_t, Time> txPacket;

    std::map<std::pair<Ipv4Address, Ipv4Address>, std::vector<RxPacketStats>> m_flowsStats;

    Time m_max_delay = Time("10s"); // TODO Ver isto
    Time m_timeslot = Time("10s");  // TODO Ver isto

    // std::mutex tx_blocked;
    std::mutex rx_blocked;

    //< node, device >
    std::map<std::pair<int, int>, Ipv4Address> m_ipv4;

    // only unicast packets
    struct lostPacket
    {
        Ptr<const Packet> p;
        Time reception;
    };

    bool lost_blocked = false;
    // packets that where lost between src-> dst : only unicast
    std::map<std::pair<Ipv4Address, Ipv4Address>, std::vector<Ptr<const Packet>>> m_lostPackets;
};

} // namespace ns3

#endif // QUALITY_MONITOR_H