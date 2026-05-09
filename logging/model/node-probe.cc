
#include "ns3/node-probe.h"

#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/logging.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"

#include <mutex>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NodeProbe");

#define RemoteIP Ipv4Address("1.0.0.2")

TypeId
NodeProbe::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::NodeProbe").SetParent<Object>().SetGroupName("Logging")
        // No AddConstructor because this class has no default constructor.
        ;

    return tid;
}

void
NodeProbe::DoDispose()
{
    m_ipv4 = 0;
    m_monitor = 0;
    m_node = 0;
    Object::DoDispose();
}

/**
 * @brief Construct a new NodeProbe:: NodeProbe object
 * -- camada mac --> tx, rx
 * -- sendOutgoing --> own ip
 * -- local deliver --> recv
 * -- energy --> energy
 * --
 * @param monitor
 * @param node
 */
NodeProbe::NodeProbe(Ptr<Logging> monitor, Ptr<Node> node)
    : m_monitor(monitor),
      m_node(node)
{
    initVariables();

    NS_LOG_FUNCTION(this << node->GetId());

    m_ipv4 = node->GetObject<Ipv4L3Protocol>();

    if (!m_ipv4->TraceConnectWithoutContext(
            "SendOutgoing",
            MakeCallback(&NodeProbe::SendOutgoingLogger, Ptr<NodeProbe>(this))))
    {
        NS_FATAL_ERROR("trace fail");
    }
    // if (!m_ipv4->TraceConnectWithoutContext ("UnicastForward",
    //                                          MakeCallback (&NodeProbe::ForwardLogger,
    //                                          Ptr<NodeProbe> (this))))
    //   {
    //     NS_FATAL_ERROR ("trace fail");
    //   }
    if (!m_ipv4->TraceConnectWithoutContext(
            "LocalDeliver",
            MakeCallback(&NodeProbe::ForwardUpLogger, Ptr<NodeProbe>(this))))
    {
        NS_FATAL_ERROR("trace fail");
    }

    // --> energy
    // ->/NodeList/*/$ns3::BasicEnergySource
    // std::ostringstream trace2;
    // trace2 << "/NodeList/" << node->GetId () << "/$ns3::BasicEnergySource/RemainingEnergy";
    // Config::ConnectWithoutContext(trace2.str(), MakeCallback(&NodeProbe::GetEnergy,
    // Ptr<NodeProbe>(this)));
    // Config::Connect("/NodeList/0/$ns3::BasicEnergySource/RemainingEnergy",
    // MakeCallback(&NodeProbe::GetEnergy, Ptr<NodeProbe>(this)) );

    // ---> phy --> _/NodeList/0/DeviceList/0/Phy/MonitorSnifferRx
    std::ostringstream trace3;
    trace3 << "/NodeList/" << node->GetId()
           << "/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx";
    Config::ConnectWithoutContext(trace3.str(),
                                  MakeCallback(&NodeProbe::SniffRx, Ptr<NodeProbe>(this)));

    std::ostringstream trace4;
    trace4 << "/NodeList/" << node->GetId() << "/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop";
    Config::ConnectWithoutContext(trace4.str(),
                                  MakeCallback(&NodeProbe::PhyRxDrop, Ptr<NodeProbe>(this)));

    // --> MAC
    try
    {
        std::ostringstream mac_oss;
        mac_oss << "/NodeList/" << node->GetId() << "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx";
        Config::ConnectWithoutContext(mac_oss.str(),
                                      MakeCallback(&NodeProbe::MacTx, Ptr<NodeProbe>(this)));

        std::ostringstream trace5;
        trace5 << "/NodeList/" << node->GetId() << "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx";
        Config::ConnectWithoutContext(trace5.str(),
                                      MakeCallback(&NodeProbe::MacRx, Ptr<NodeProbe>(this)));
        // Note: tem mais info, como o type and others

        // openflow
        std::ostringstream trace6;
        trace6 << "/NodeList/" << node->GetId()
               << "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacOpenFlowNormalAction";
        Config::ConnectWithoutContext(
            trace6.str(),
            MakeCallback(&NodeProbe::OpfNormalAction, Ptr<NodeProbe>(this)));

        // std::ostringstream trace1;
        // trace1 << "/NodeList/" << node->GetId () <<
        // "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacOpenFlowRx";
        // Config::ConnectWithoutContext(trace1.str() , MakeCallback(&NodeProbe::MacOpenFlowRx ,
        // Ptr<NodeProbe>(this) ) );

        // std::ostringstream trace2;
        // trace2 << "/NodeList/" << node->GetId () <<
        // "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacBroadcastTx";
        // Config::ConnectWithoutContext(trace2.str() , MakeCallback(&NodeProbe::MacBroadcastTx ,
        // Ptr<NodeProbe>(this) ) );
    }
    catch (...)
    {
        ;
    }
}

// PHYSICAL LAYER
void
NodeProbe::SniffRx(Ptr<const Packet> packet,
                   uint16_t channelFreqMhz,
                   WifiTxVector txVector,
                   MpduInfo aMpdu,
                   SignalNoiseDbm signalNoise,
                   uint16_t staId)

{
    double snr =
        signalNoise.signal - signalNoise.noise; ///< SNR in linear scale,///< signal strength in dBm
    inc_snr(snr);
    inc_phy_listened();
}

void
NodeProbe::GetEnergy(double oldValue, double remainingEnergy)
{
    set_energy(remainingEnergy);
}

void
NodeProbe::PhyRxDrop(Ptr<const Packet> p, WifiPhyRxfailureReason reason)
{
    inc_phy_droped();
}

void
NodeProbe::OpfNormalAction(Ptr<const Packet> packet)
{
    inc_normal_action();
}

NodeProbe::~NodeProbe()
{
    // TODO
}

Ptr<Node>
NodeProbe::GetNode()
{
    return m_node;
}

void
NodeProbe::MacRx(Ptr<const Packet> packet)
{
    inc_bytes_rx(packet->GetSize());
    inc_packets_rx();

    uint64_t packetId = packet->GetUid();
    Logging::TrackedPacketMap::iterator tracked = m_monitor->m_trackedPackets.find(packetId);
    if (tracked == m_monitor->m_trackedPackets.end())
    {
        NS_LOG_WARN("Received packet forward report (nodeId="
                    << m_node->GetId() << ", packetId=" << packetId
                    << ") but not known to be transmitted.");
        return;
    }

    tracked->second.lastSeenTime = Simulator::Now();

    Time delay = (Simulator::Now() - tracked->second.firstSeenTime);
    // NodeProbe->AddPacketStats (flowId, packetSize, delay);
    inc_delay(delay);

    // TODO inc_broadcasted
}

void
NodeProbe::ForwardUpLogger(const Ipv4Header& ipHeader,
                           Ptr<const Packet> ipPayload,
                           uint32_t interface)
{
    if (ipHeader.GetSource() == RemoteIP)
    {
        // std::cout << " RemoteIP " <<ipHeader <<std::endl;
        inc_rx_opf_control();
        return;
    }
    if (ipHeader.GetDestination().IsSubnetDirectedBroadcast(Ipv4Mask("/24")))
    {
        inc_rx_broadcast();
    }
    else
    {
        // report received  <<
        inc_packets_received();
        // std::cout << " ForwardUpLogger " <<ipHeader <<std::endl;
    }
}

void
NodeProbe::SendOutgoingLogger(const Ipv4Header& ipHeader,
                              Ptr<const Packet> ipPayload,
                              uint32_t interface)
{
    if (ipHeader.GetDestination() == RemoteIP)
    {
        // std::cout << " RemoteIP " <<ipHeader <<std::endl;
        inc_tx_opf_control();
        return;
    }

    if (ipHeader.GetDestination().IsSubnetDirectedBroadcast(Ipv4Mask("/24")))
    {
        inc_tx_broadcast();
    }
    else
    {
        // report own
        inc_packets_created();
        // std::cout << " SendOutgoingLogger " <<ipHeader <<std::endl;
    }
}

// void NodeProbe::MacOpenFlowRx(Ptr< const Packet > packet){
//   // inc_bytes_rx(packet->GetSize());
//   // inc_packets_rx();

// }

// void
// NodeProbe::MacBroadcastTx(Ptr< const Packet > packet){
//   inc_tx_broadcast();
// }

void
NodeProbe::MacTx(Ptr<const Packet> packet)
{
    inc_bytes_tx(packet->GetSize());
    inc_packets_tx();

    uint64_t packetId = packet->GetUid();
    // if not found in tacked packet entao é porque é a primeira vez
    if (m_monitor->m_trackedPackets.find(packetId) == m_monitor->m_trackedPackets.end())
    {
        // not found
        Time now = Simulator::Now();
        std::lock_guard<std::mutex> guard(m_monitor->monitor_mutex);

        Logging::TrackedPacket& tracked = m_monitor->m_trackedPackets[packetId];
        tracked.firstSeenTime = now;
        tracked.lastSeenTime = tracked.firstSeenTime;
        tracked.timesForwarded = 0;
        // unlock
        NS_LOG_DEBUG("ReportFirstTx: adding tracked packet (nodeId="
                     << m_node->GetId() << ", packetId=" << packetId << ").");
    }
    else
    { // Report Forwarding
        // found
        Logging::TrackedPacketMap::iterator tracked = m_monitor->m_trackedPackets.find(packetId);
        if (tracked == m_monitor->m_trackedPackets.end())
        {
            NS_LOG_WARN("Received packet forward report (nodeId="
                        << m_node->GetId() << ", packetId=" << packetId
                        << ") but not known to be transmitted.");
            return;
        }
        std::lock_guard<std::mutex> guard(m_monitor->monitor_mutex);
        tracked->second.timesForwarded++;
        inc_forward();
    }
}

// Thread-safe methods to increase counters
void
NodeProbe::inc_normal_action()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_opf_action_normal++;
}

void
NodeProbe::inc_snr(double snr)
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    phy_snr_sum += (int)snr;
}

void
NodeProbe::inc_phy_listened()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    phy_total_listened++;
}

void
NodeProbe::set_energy(double remainingEnergy)
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    energy = (int)remainingEnergy;
}

void
NodeProbe::inc_phy_droped()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    phy_total_droped++;
}

void
NodeProbe::inc_forward()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packet_tx_forwarded_total++; // packets
}

void
NodeProbe::inc_delay(Time delay)
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    delaySum += delay;
    delaySum_total += delay;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_packets_created()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_own_total++;
    packets_own++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_packets_received()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_rcv++;
    packets_rcv_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_tx_broadcast()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_Tx_Bcast_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_tx_opf_control()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_opf_rcv_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_rx_opf_control()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_opf_own_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_rx_broadcast()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_Rx_Bcast_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_packets_rx()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_rx++;
    packets_rx_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_bytes_rx(int size)
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    bytes_rx += size;
    bytes_rx_total += size;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_packets_tx()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_tx++;
    packets_tx_total++;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::inc_bytes_tx(int size)
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    bytes_tx += size;
    bytes_tx_total += size;
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::ClearTimestampVariables()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    packets_tx = 0;
    bytes_tx = 0;
    packets_rx = 0;
    bytes_rx = 0;
    // control_packets_number_per_timestamp = 0;
    // control_packets_size_per_timestamp = 0;
    n_contacts = 0;  // neighbour
    packets_rcv = 0; // local delivery ipv4L3
    packets_own = 0; // packets created by me : only ipv4L3
    energy = 0;
    // from flow-monitor
    delaySum = Time("0ms"); // delayCount == rxPackets
    // pthread_mutex_unlock(&logging_mutex);
}

void
NodeProbe::initVariables()
{
    std::lock_guard<std::mutex> guard(logging_mutex);
    phy_snr_sum = 0;
    phy_total_droped = 0;
    phy_total_listened = 0;

    // mac layer
    packets_tx_total = 0;
    bytes_tx_total = 0; // txBytes: Total number of transmitted bytes for the node
    packets_rx_total = 0;
    bytes_rx_total = 0; // rxBytes: Total number of received bytes for the node
    packets_tx = 0;
    bytes_tx = 0;
    packets_rx = 0;
    bytes_rx = 0;

    packet_tx_forwarded_total = 0; // number of forwarded operations made
    // TODO nao tenho infrasctrutura para esta medida --> uint32_t packets_repeated_total; // with
    // the trackedPacket

    // ipv4
    packets_rcv_total = 0;
    packets_own_total = 0; // packets created by me : only ipv4L3
    packets_rcv = 0;       // local delivery ipv4L3
    packets_own = 0;       // packets created by me : only ipv4L3

    packets_Tx_Bcast_total = 0;
    packets_Rx_Bcast_total = 0;

    // openflow

    packets_opf_action_normal = 0;
    packets_opf_rcv_total = 0; // openflow ipv4 messages
    packets_opf_own_total = 0;

    // node
    energy = 0;
    n_contacts = 0; // neighbour

    // from flow-monitor
    delaySum_total = Time("0ms");

    // from flow-monitor
    delaySum = Time("0ms"); // delayCount == rxPackets
}

}; // namespace ns3