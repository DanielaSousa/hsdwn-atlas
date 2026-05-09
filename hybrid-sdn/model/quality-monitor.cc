
#include "quality-monitor.h"

#include "ns3/config.h" // config::connect
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-flow-probe.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/ipv6-flow-probe.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/node-list.h"
#include "ns3/node.h"

namespace ns3
{

// namespace{ std::map < std::pair<Ipv4Address, Ipv4Address> , std::vector<RxPacketStats> >
// m_flowsStats;

//     std::map < std::pair<int, int> , PhyPacketStats > lastPacket;

//     //nao tenho garantia de nao ter sido lost, mas se nao há maneira de o distinguir nao consigo
//     fazer mais nada
//     // soluçao -> o max delay < (menor) que o resend no mesmo nó de pacotes exatamento iguais
//     //std::map < Ptr<const Packet> , std::vector<Time> > txPacket; // TODO ir apagando no check
//     for old packets std::map < uint64_t , Time > txPacket;

//     std::map < std::pair<int, int>, Ipv4Address> m_ipv4;
//     std::map <std::pair<Ipv4Address, Ipv4Address>, std::vector< Ptr<const Packet> > >
//     m_lostPackets;

//  }

NS_LOG_COMPONENT_DEFINE("QualityMonitor");

std::pair<int, int>
ParseContext(std::string context)
{
    // esta a dar mallll !!! TODO
    std::string delimiter = "/";
    int tmp_node, tmp_dev;
    // _/NodeList/0/DeviceList/0/Phy/MonitorSnifferRx
    size_t pos = context.find(delimiter);

    context.erase(0, pos + delimiter.length() + 9); // remove _/NodeList/
    pos = context.find(delimiter);
    tmp_node = std::atoi(context.substr(0, pos).c_str());
    // pos = context.find(delimiter);
    context.erase(0, pos + delimiter.length()); //*
    pos = context.find(delimiter);

    // DeviceList "which is the zeroth device installed in the node. "
    context.erase(0, pos + delimiter.length());
    pos = context.find(delimiter);
    tmp_dev = std::atoi(context.substr(0, pos).c_str());
    // while ((pos = context.find(delimiter)) != std::string::npos) {
    //     std::cout << "\n" << context.substr(0, pos);
    //     // tmp_node =
    //     // std::cout << tmp_node << std::endl;
    //     context.erase(0, pos + delimiter.length());
    // }
    // std::cout << tmp_node << " " << tmp_dev << std::endl;
    return std::make_pair(tmp_node, tmp_dev);
}

QualityMonitor::QualityMonitor(/* args */)
//: m_enabled (false)
{
    NS_LOG_FUNCTION(this);
    pesos[0] = 100;
    pesos[1] = 100;
    pesos[2] = 100;
    pesos[3] = 100;
    pesos[4] = 100;
    pesos[5] = 100;
}

QualityMonitor::~QualityMonitor()
{
}

int
QualityMonitor::Install(NodeContainer nodes, int index)
{
    // //install flowmonitors

    // //install energy
    // 0. ns3::SimpleDeviceEnergyModel   TotalEnergyConsumption: Total energy consumption of the
    // radio device.

    // //install traces
    // 1. OLSR 'RoutingTableChanged': The OLSR routing table has changed.
    // 2.

    // Config::ConnectWithoutContext ("/NodeList/0/DeviceList/*/Phy/MonitorSnifferRx", MakeCallback
    // (&QualityMonitor::MonitorSniffRx, this));

    // Config::Connect ("/NodeList/*/DeviceList/*/$ns3::Ipv4L3Protocol/Rx", MakeCallback
    // (&Ipv4FlowProbe::SendOutgoingLogger, Ptr<Ipv4FlowProbe> (this))))

    // Config::Connect ("/NodeList/*/DeviceList/*/Mac/MacTx",MakeCallback
    // (&QualityMonitor::MonitorMacTx, this)); Config::Connect
    // ("/NodeList/*/DeviceList/*/Mac/MacRx",MakeCallback (&QualityMonitor::MonitorMacRx, this));

    // like in flow helper
    //  m_flowClassifier = Create<Ipv4FlowClassifier> ();
    //  m_flowMonitor = m_monitorFactory.Create<FlowMonitor> ();
    //  m_flowMonitor->AddQualityMonitor(this);

    return 0;
}

int
QualityMonitor::Install(Ptr<ns3::Node> node, int index)
{
    NS_LOG_FUNCTION(this << node << index);

    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4Address t = ipv4->GetAddress(index, 0).GetLocal();
    m_ipv4[std::make_pair(node->GetId(), index)] =
        t; // device index =>0 pois é onde tem o mac que é usado pelo ad-hoc net device
    NS_LOG_INFO("Install Node  " << node->GetId() << " Device index " << index << " "
                                 << node->GetDevice(index)->GetAddress() << " ipv4 interface " << 0
                                 << " " << t);

    // install config trace
    Config::Connect("/NodeList/" + std::to_string(node->GetId()) + "/DeviceList/" +
                        std::to_string(index) + "/Phy/MonitorSnifferRx",
                    MakeCallback(&QualityMonitor::MonitorSniffRx, this));
    Config::Connect("/NodeList/" + std::to_string(node->GetId()) + "/DeviceList/" +
                        std::to_string(index) + "/Mac/MacTx",
                    MakeCallback(&QualityMonitor::MonitorMacTx, this));
    Config::Connect("/NodeList/" + std::to_string(node->GetId()) + "/DeviceList/" +
                        std::to_string(index) + "/Mac/MacRx",
                    MakeCallback(&QualityMonitor::MonitorMacRx, this));

    return 0;
}

// Ipv4Address QualityMonitor::GetSourceFromFlowId(FlowId flowid){
//     Ipv4Address sourceAddress  = m_flowClassifier->FindFlow(flowid).sourceAddress;
//     return sourceAddress;
// }

void
QualityMonitor::DoDispose(void)
{
}

void
QualityMonitor::CheckForLostPackets(Time maxDelay)
{
    NS_LOG_FUNCTION(this << maxDelay.As(Time::S));
    Time now = Simulator::Now();

    //    for (TrackedPacketMap::iterator iter = m_trackedPackets.begin ();
    //         iter != m_trackedPackets.end (); )
    //      {
    //        if (now - iter->second.lastSeenTime >= maxDelay)
    //          {
    //            // packet is considered lost, add it to the loss statistics
    //            FlowStatsContainerI flow = m_flowStats.find (iter->first.first);
    //            NS_ASSERT (flow != m_flowStats.end ());
    //            flow->second.lostPackets++;

    //            // we won't track it anymore
    //            m_trackedPackets.erase (iter++);
    //          }
    //        else
    //          {
    //            iter++;
    //          }
    //      }
}

void
QualityMonitor::MonitorMacTx(std::string context, Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << context << packet->GetUid());

    std::pair<int, int> tmp = ParseContext(context);
    NS_ASSERT_MSG(m_ipv4[tmp] != Ipv4Address(),
                  "IPv4 is empty!" << "context " << tmp.first << " " << tmp.second << " : "
                                   << m_ipv4[tmp] << context); // '102.102.102.102'

    NS_LOG_LOGIC("context" << tmp.first << " " << tmp.second << " " << m_ipv4[tmp]);
    NS_LOG_INFO("MonitorMacTx " << packet->GetUid() << " " << tmp.first << tmp.second);
    // TODO pacotes com o mesmo packet structure dá merda
    // tx_blocked.lock();
    txPacket[packet->GetUid()] = Simulator::Now();
    NS_LOG_LOGIC("Inserted packet " << packet->GetUid() << " with time "
                                    << txPacket[packet->GetUid()]);
    // tx_blocked.unlock();
    // if (packet->GetUid() == 316)
    // {
    //     std::cout << "context" << tmp.first << " " << tmp.second << " " << m_ipv4[tmp]
    //               << "Packet UID 316" << Simulator::Now().As(Time::S) << std::endl;
    // }

    // lost packet
    Ptr<Packet> copy = packet->Copy();
    LlcSnapHeader llc;
    copy->RemoveHeader(llc);
    Ipv4Address header_destination;
    Ipv4Address header_source;
    // bool arp_request = false;
    if (llc.GetType() == 0x806)
    {
        ArpHeader h;
        copy->PeekHeader(h);
        // arp_request = h.IsRequest();
        if (h.IsRequest())
        {
            header_destination = h.GetDestinationIpv4Address()
                                     .GetBroadcast(); // solves the problem of bradcast arp request
        }
        else
        {
            header_destination = h.GetDestinationIpv4Address();
        }
        header_source = h.GetSourceIpv4Address();
    }
    else
    {
        Ipv4Header h;
        copy->PeekHeader(h);
        header_destination = h.GetDestination();
        header_source = h.GetSource();
    }
    NS_LOG_INFO("header " << header_source << " -> " << header_destination);

    // lostPacket p = {packet, Time()};
    // only unicast packets
    // if(!header_destination.IsBroadcast() && !arp_request )
    if (!header_destination.IsBroadcast())
        m_lostPackets[std::make_pair(header_source, header_destination)].push_back(packet);

    // if (header_destination == Ipv4Address("10.1.1.10") && header_source ==
    // Ipv4Address("10.1.1.2"))
    // {
    //     packet->Print(std::cout);
    //     std::cout << std::endl;
    // }
}

void
QualityMonitor::MonitorMacRx(std::string context, Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << context << packet->GetUid());

    std::lock_guard<std::mutex> lock(rx_blocked); // RIA

    Time now = Simulator::Now();
    std::pair<int, int> tmp = ParseContext(context);

    // packet->Print(std::cout); std:: cout << std::endl;
    NS_ASSERT_MSG(m_ipv4[tmp] != Ipv4Address(), "IPv4 is empty!"); // '102.102.102.102'

    NS_LOG_INFO("MonitorMacRx " << packet->GetUid() << " " << tmp.first << tmp.second << " "
                                << m_ipv4[tmp]);
    // TODO tenho o problema de saber se é wifi or not
    Ptr<Packet> copy = packet->Copy();
    LlcSnapHeader llc;

    copy->RemoveHeader(llc);
    Ipv4Address header_destination;
    Ipv4Address header_source;
    // bool arp_request = false;
    if (llc.GetType() == 0x806)
    {
        ArpHeader h;
        copy->PeekHeader(h);
        // arp_request = h.IsRequest();
        if (h.IsRequest())
        {
            header_destination = h.GetDestinationIpv4Address()
                                     .GetBroadcast(); // solves the problem of bradcast arp request
        }
        else
        {
            header_destination = h.GetDestinationIpv4Address();
        }
        header_source = h.GetSourceIpv4Address();
    }
    else
    {
        Ipv4Header h;
        copy->PeekHeader(h);
        header_destination = h.GetDestination();
        header_source = h.GetSource();
    }

    NS_LOG_INFO("header " << header_source << " -> " << header_destination);

    // unicast lost packets
    // if ( header_destination == m_ipv4[tmp] && !arp_request){ //is for me??
    if (header_destination == m_ipv4[tmp])
    {
        // find packet in vector and erase it
        auto k = m_lostPackets[std::make_pair(header_source, header_destination)];
        for (auto it = k.begin(); it != k.end();)
        {
            if ((*it)->GetUid() == packet->GetUid())
            {
                it = k.erase(it);
                NS_LOG_LOGIC("Erasing packet " << (*it)->GetUid() << " from m_lostPackets");
                break;
            }
            else
            {
                ++it;
            }
        }
    }

    Time t1;

    if (txPacket.find(packet->GetUid()) != txPacket.end())
    {
        t1 = txPacket[packet->GetUid()];
        // if(header_destination == m_ipv4[tmp] && !arp_request){ // se for para mim podes apagar do
        // tx
        if (header_destination == m_ipv4[tmp])
        {
            txPacket.erase(packet->GetUid());
            NS_LOG_LOGIC("Erasing packet " << packet->GetUid() << " from txPacket");
        }
    }
    else
    { // packet from legacy node -> ignore
        NS_ASSERT_MSG(lastPacket.find(tmp) != lastPacket.end(),
                      "Cannot retrieve SNR, packet skiped phy layer on RX!");
        m_flowsStats[std::make_pair(header_source, m_ipv4[tmp])].push_back(
            {lastPacket[tmp], now, Time(1), packet->GetSize(), header_source, header_destination});
        NS_LOG_INFO("Insert in Flow " << header_source << "->" << m_ipv4[tmp] << "Packet "
                                      << packet->GetUid() << ":" << header_source << "->"
                                      << header_destination << " " << m_flowsStats.size() << " "
                                      << txPacket.size());
        return;
    }
    // else
    // {
    //     NS_LOG_ERROR("ERROR is empty " << txPacket.size() << " - PacketUID: "
    //                                    << packet->GetUid()); // está sempre a entrar aqui!!!
    //     std::cout << "ERROR is empty: device (nodeId, interface)-> " << tmp.first << " "
    //               << tmp.second << ": " << m_ipv4[tmp] << " - PacketUID: " << packet->GetUid()
    //               << "time: " << Simulator::Now().As(Time::S) << std::endl;
    //     // TODO
    //     exit(-1);
    //     return;
    // }

    NS_ASSERT_MSG(t1 > Time(0), "Sending Time equals to zero!");
    RxPacketStats stats =
        {lastPacket[tmp], now, now - t1, packet->GetSize(), header_source, header_destination};

    NS_LOG_INFO(lastPacket[tmp].snr << " " << now.GetSeconds() << " delay "
                                    << (now - t1).GetSeconds() << " " << packet->GetSize() << " "
                                    << header_source << " " << header_destination);

    m_flowsStats[std::make_pair(header_source, m_ipv4[tmp])].push_back(stats);

    NS_LOG_INFO("Insert in Flow " << header_source << "->" << m_ipv4[tmp] << "Packet "
                                  << packet->GetUid() << ":" << header_source << "->"
                                  << header_destination << " " << m_flowsStats.size() << " "
                                  << txPacket.size());
}

void
QualityMonitor::MonitorSniffRx(std::string context,
                               Ptr<const Packet> packet,
                               uint16_t channelFreqMhz,
                               WifiTxVector txVector,
                               MpduInfo aMpdu,
                               SignalNoiseDbm signalNoise,
                               uint16_t staId)

{
    NS_LOG_FUNCTION(this << context << packet->GetUid());
    // packet->Print(std::cout ); std::cout <<std::endl;

    std::pair<int, int> tmp = ParseContext(context);
    NS_LOG_INFO("context" << tmp.first << " " << tmp.second << " " << m_ipv4[tmp]);
    /**
     double signal; ///< signal strength in dBm
     double noise;  ///< noise power in dBm
     double snr;  ///< SNR in linear scale
     double rssi; ///< RSSI in dBm
     */
    PhyPacketStats phy = {signalNoise.signal,
                          signalNoise.noise,
                          signalNoise.signal - signalNoise.noise,
                          Simulator::Now()};

    NS_LOG_INFO(signalNoise.signal << " " << signalNoise.noise << " "
                                   << signalNoise.signal - signalNoise.noise);

    lastPacket[std::make_pair(tmp.first, tmp.second)] = phy;
}

// void QualityMonitor::AddStats(uint32_t flowId, uint32_t packetId, uint32_t packetSize,Time delay
// ){
//     //std::vector < PacketStats > tmp = m_trackedPackets[flowId];

// }

std::map<Ipv4Address, std::vector<MyFlowStats>>
QualityMonitor::GetStats(Ipv4Address src)
{
    std::map<Ipv4Address, std::vector<MyFlowStats>> stats;
    for (auto kt = m_ipv4.begin(); kt != m_ipv4.end(); ++kt)
    {
        if (kt->second != src)
        {
            MyFlowStats t = GetFlowStats(src, kt->second);
            if (t.valid)
            {
                stats[src].push_back(t);
            }
        }
    }

    return stats;
}

/**
 * @brief
 *
 * @param src the switch ipv4
 * @param dst neighbours
 * @return MyFlowStats
 */
MyFlowStats
QualityMonitor::GetFlowStats(Ipv4Address src, Ipv4Address dst)
{
    NS_LOG_FUNCTION(this << "GetFlowStats" << src << " " << dst << " " << m_flowsStats.size() << " "
                         << txPacket.size());

    std::lock_guard<std::mutex> lock(rx_blocked);

    MyFlowStats stats;

    // TODO check if m_flow stats has any packets
    std::vector<Ptr<const Packet>> v =
        m_lostPackets[std::make_pair(src, dst)]; // unicast, so conta como perdido se ele nao chegar
                                                 // ao destination

    auto it = m_flowsStats.find(
        std::make_pair(dst, src)); // 10.1.1.3 to others, mas quero sempre é o que me chegou a mim
    // auto it2 = m_flowsStats.find(std::make_pair(src, dst)); //este está mal

    // stats
    stats.src = src;
    stats.dst = dst;
    stats.n_pktLost = v.size(); // unicast

    // se nao houver lost packets é diferente de nao ter recebido nada
    if (it == m_flowsStats.end())
    {
        NS_LOG_DEBUG("NO STATS !");
        // std::cout << "NO STATS|" << v.size() <<std::endl;
        return stats;
        // NO packets utilized this flow
    }

    std::vector<RxPacketStats> r = it->second;
    NS_ASSERT_MSG(r.size() != 0, "Flow has no packets received!");

    // r.insert(r.end(), it2->second.begin(), it2->second.end() );

    stats.u_txPackets = v.size();
    stats.u_rxPackets = 0;

    // TODO update restantes
    // Time avgDelay;
    // double avgRssi; //in dBm
    // double avgSnr;
    // int rxbytes;
    // double avgThroughput;
    for (auto kt = r.begin(); kt != r.end(); ++kt)
    { // std::cout << src << kt->dst << std::endl;
        if (!kt->dst.IsBroadcast() && kt->dst == src)
        { // unicast + era para mim !kt->dst.IsBroadcast() &&
            stats.u_txPackets++;
            stats.u_rxPackets++;
            stats.u_rxbytes += kt->packetSize;
        }
        stats.avgDelay += kt->delay;
        // std::cout << "packet(" << kt->src << " - " << kt->dst << "): " << kt->delay << std::endl;
        stats.avgRssi += kt->phy.rssi;
        stats.avgSnr += kt->phy.snr;
        stats.avgThroughput +=
            kt->packetSize * 8 / kt->delay.GetSeconds(); // TODO ver se isto dá zero ou nao
        // std::cout  << " SECONDS" << kt->delay.GetSeconds() << std::endl;
    }
    // std::cout << stats.avgDelay << " " << r.size() << std::endl;
    stats.avgDelay = stats.avgDelay / r.size();
    stats.avgRssi = stats.avgRssi / r.size();
    stats.avgSnr = stats.avgSnr / r.size();
    stats.avgThroughput = stats.avgThroughput / r.size();
    if (stats.u_txPackets > 0)
    {
        stats.u_delivery_ratio = (double)stats.u_rxPackets / stats.u_txPackets;
        // std::cout << "u_rxPackets " <<  stats.u_rxPackets<< " u_tx_packets " <<stats.u_txPackets
        // << std::endl;
    }
    else
    {
        // stats.u_delivery_ratio = 0.0;
        stats.u_delivery_ratio = 1.0;
    }

    NS_LOG_INFO("delay " << stats.avgDelay.GetSeconds() << " sec, RSSI " << stats.avgRssi
                         << " dbm, SNR " << stats.avgSnr << " dbm, throughput "
                         << stats.avgThroughput << " bps, delivery " << stats.u_delivery_ratio * 100
                         << " %% \n");

    stats.valid = true;
    return stats;
}

void
QualityMonitor::ErasePacket()
{
}

void
QualityMonitor::CheckForOldPackets(Time maxTime)
{
    std::lock_guard<std::mutex> lock(rx_blocked);
    Time now = Simulator::Now();

    // rx_blocked.lock();
    for (auto it = m_flowsStats.begin(); it != m_flowsStats.end();)
    {
        size_t tmp_size = it->second.size();
        for (size_t i = 0; i < tmp_size; i++)
        {
            if (!it->second.front().receptionTime.IsZero() &&
                ((now - it->second.front().receptionTime) > m_timeslot ||
                 (now - it->second.front().phy.phyTime) > m_timeslot))
            {
                // erase
                it->second.erase(it->second.begin());
                NS_LOG_INFO("ERASE " << it->first << " vector size " << it->second.size());
            }
            else
            {
                break; // newer packets after this
            }
        }
        // vector size is zero, delete key
        if (it->second.size() == 0)
        {
            it = m_flowsStats.erase(it);
        }
        else
        {
            ++it;
        }
    }
    // rx_blocked.unlock();

    // tx_blocked.lock();
    for (auto it = txPacket.begin(); it != txPacket.end();)
    { // std::map < uint64_t , Time
        if (!it->second.IsZero() && (now - it->second) > m_max_delay)
        {
            // erase
            it = txPacket.erase(it); // or "it = m.erase(it)" since C++11
            // if (it->first == 316)
            // {
            //     NS_LOG_WARN("Erased packet (m_max_delay) " << it->first << " now " << now
            //                                                << " time: " << it->second);
            // }
        }
        else
        {
            ++it;
        }
        //}
    }
    // tx_blocked.unlock();
}

//  TypeId
//  QualityMonitor::GetTypeId (void)
//  {
//    static TypeId tid = TypeId ("ns3::FlowMonitor")
//      .SetParent<Object> ()
//      .SetGroupName ("FlowMonitor")
//      .AddConstructor<FlowMonitor> ()
//      .AddAttribute ("MaxPerHopDelay", ("The maximum per-hop delay that should be considered.  "
//                                        "Packets still not received after this delay are to be
//                                        considered lost."),
//                     TimeValue (Seconds (10.0)),
//                     MakeTimeAccessor (&FlowMonitor::m_maxPerHopDelay),
//                     MakeTimeChecker ())
//      .AddAttribute ("StartTime", ("The time when the monitoring starts."),
//                     TimeValue (Seconds (0.0)),
//                     MakeTimeAccessor (&FlowMonitor::Start),
//                     MakeTimeChecker ())
//      .AddAttribute ("DelayBinWidth", ("The width used in the delay histogram."),
//                     DoubleValue (0.001),
//                     MakeDoubleAccessor (&FlowMonitor::m_delayBinWidth),
//                     MakeDoubleChecker <double> ())
//      .AddAttribute ("JitterBinWidth", ("The width used in the jitter histogram."),
//                     DoubleValue (0.001),
//                     MakeDoubleAccessor (&FlowMonitor::m_jitterBinWidth),
//                     MakeDoubleChecker <double> ())
//      .AddAttribute ("PacketSizeBinWidth", ("The width used in the packetSize histogram."),
//                     DoubleValue (20),
//                     MakeDoubleAccessor (&FlowMonitor::m_packetSizeBinWidth),
//                     MakeDoubleChecker <double> ())
//      .AddAttribute ("FlowInterruptionsBinWidth", ("The width used in the flowInterruptions
//      histogram."),
//                     DoubleValue (0.250),
//                     MakeDoubleAccessor (&FlowMonitor::m_flowInterruptionsBinWidth),
//                     MakeDoubleChecker <double> ())
//      .AddAttribute ("FlowInterruptionsMinTime", ("The minimum inter-arrival time that is
//      considered a flow interruption."),
//                     TimeValue (Seconds (0.5)),
//                     MakeTimeAccessor (&FlowMonitor::m_flowInterruptionsMinTime),
//                     MakeTimeChecker ())
//    ;
//    return tid;
//  }

} // namespace ns3