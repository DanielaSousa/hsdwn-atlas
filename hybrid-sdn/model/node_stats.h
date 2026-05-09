/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef AVBW_MONITOR_H
#define AVBW_MONITOR_H

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/pointer.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include <ns3/wifi-net-device.h>

#include <cmath>
#include <iostream>
#include <unordered_map>

namespace ns3
{
// NS_LOG_COMPONENT_DEFINE("NodeStatistics");

/** Node statistics */
class NodeStatistics
{
  public:
    /**
     * Constructor
     * \param aps AP devices
     * \param stas STA devices
     * \param n node associated with these stats
     */
    NodeStatistics(Ptr<Node> n, Ptr<NetDevice> device, int dev_index);
    //~NodeStatistics();

    /**
     * RX callback
     * \param path path
     * \param packet received packet
     * \param from sender
     */
    void RxCallback(std::string path, Ptr<const Packet> packet, const Address& from);

    void MacTx(std::string context, Ptr<const Packet> p);
    void Ipv4L3Tx(std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface);
    void PhyTx(std::string context,
               Ptr<const Packet> packet,
               uint16_t channelFreqMhz,
               WifiTxVector txVector,
               MpduInfo aMpdu,
               uint16_t staId);

    void PhyRx(std::string context,
               Ptr<const Packet> packet,
               uint16_t channelFreqMhz,
               WifiTxVector txVector,
               MpduInfo aMpdu,
               SignalNoiseDbm signalNoise,
               uint16_t staId);

    void ArpL3DropTx(std::string context, Ptr<const Packet> packet);
    /**
     * Set node position
     * \param node the node
     * \param position the position
     */
    void SetPosition(Ptr<Node> node, Vector position);
    /**
     * Advance node position
     * \param node the node
     * \param stepsSize the size of a step
     * \param stepsTime the time interval between steps
     */
    void AdvancePosition(Ptr<Node> node, int stepsSize, int stepsTime);
    /**
     * Get node position
     * \param node the node
     * \return the position
     */
    Vector GetPosition(Ptr<Node> node);
    /**
     * \return the gnuplot 2d dataset
     */
    Gnuplot2dDataset GetDatafile();

    bool isFromFlow(std::string s);
    void TcTx(std::string context, Ptr<const Packet> p);

    /**
     * @brief reset vars every period
     *
     */
    void PeriodicallyResetVars();

    /**
     * @brief Get the Av Bw in bps
     *
     * @param datarate of flow in layer 3 (ipv4)
     * @return int
     */
    int GetAvBw();
    int GetMaxC();

    uint8_t GetIdleRatio();

    void MonitorPhyState(std::string context, Time start, Time duration, WifiPhyState state);

  private:
    void StateFinalTimer();
    WifiPhyState GetState();

  private:
    Gnuplot2dDataset m_output; //!< gnuplot 2d dataset
    Ptr<Node> m_node;
    Ptr<WifiNetDevice> device;

    // Bytes
    uint32_t l3_bytesTotal;
    uint32_t mac_bytesTotal;
    uint32_t phy_bytesTotal; //!< total bytes

    // Pkts
    uint32_t l3_pktsTotal;
    uint32_t mac_pktsTotal;
    uint32_t phy_pktsTotal; //!< total pkts udp

    // pkts size
    // uint32_t l3_pktsSizeSum; //!< equal to l3_bytesTotal
    uint32_t mac_pktsSizeSum;
    uint32_t phy_pktsSizeSum;

    // Pkts Flow
    uint32_t l3_pktsFlow; //!< total pkts flow id=150
    uint32_t mac_pktsFlow;
    uint32_t phy_pktsFlow;
    uint32_t phy_bytesFlow;

    uint32_t phy_rx_bytes;

    bool pktRequired = false;
    int avbw = 0;               // bps
    int maxCapacity = 25000000; // bps (25Mbps)

    uint8_t t_idle = 0;

    std::vector<int> phy_pkts;
    int snr;

  public:
    Time lastDuration;
    Time lastStart;
    std::unordered_map<WifiPhyState, Time> timers;
    double period = 1.0;
};

} // namespace ns3
#endif