#include "node_stats.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("AvBwStatus");

NodeStatistics::NodeStatistics(Ptr<Node> n, Ptr<NetDevice> d, int dev_index)
{
    l3_bytesTotal = 0;
    mac_bytesTotal = 0;
    phy_bytesTotal = 0; //!< total bytes

    // Pkts
    l3_pktsTotal = 0;
    mac_pktsTotal = 0;
    phy_pktsTotal = 0; //!< total pkts udp

    // pkts size
    // l3_pktsSizeSum = 0;
    mac_pktsSizeSum = 0;
    phy_pktsSizeSum = 0;

    // Pkts Flow
    l3_pktsFlow = 0; //!< total pkts flow id=150
    mac_pktsFlow = 0;
    phy_pktsFlow = 0;
    phy_rx_bytes = 0;
    m_node = n;
    device = DynamicCast<WifiNetDevice>(d);

    // now is 0.0 secs
    Time callEndPeriod = Seconds(period) - PicoSeconds(1); // 10^-12 sec
    Simulator::Schedule(callEndPeriod, &NodeStatistics::StateFinalTimer, this);
    // std::cout << "Schedule 1 --" << Seconds(period) << std::endl;
    Simulator::Schedule(Seconds(period), &NodeStatistics::PeriodicallyResetVars, this);

    // ########### Stats node ##############

    // TODO remove unnecessary ones
    // Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/DeviceList/" +
    //                     std::to_string(dev_index) + "/Phy/MonitorSnifferRx",
    //                 MakeCallback(&NodeStatistics::PhyRx, this)); // from node 0 to node 1

    Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/DeviceList/" +
                        std::to_string(dev_index) + "/$ns3::WifiNetDevice/Phy/MonitorSnifferTx",
                    MakeCallback(&NodeStatistics::PhyTx, this));

    // Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/DeviceList/" +
    //                     std::to_string(dev_index) + "/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
    //                 MakeCallback(&NodeStatistics::PhyRx, this));

    // Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/DeviceList/" +
    //                     std::to_string(dev_index) + "/Mac/MacTx",
    //                 MakeCallback(&NodeStatistics::MacTx, this));

    // Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/$ns3::Ipv4L3Protocol/Tx",
    //                 MakeCallback(&NodeStatistics::Ipv4L3Tx, this));

    // Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/$ns3::ArpL3Protocol/Drop",
    //                 MakeCallback(&NodeStatistics::ArpL3DropTx, this));
    // Config::Connect("/NodeList/" + std::to_string(n->GetId()) +
    //                     "/$ns3::Node/$ns3::TrafficControlLayer/TcTx",
    //                 MakeCallback(&NodeStatistics::TcTx, this));

    /* WhiPhy State's Timer */
    Config::Connect("/NodeList/" + std::to_string(n->GetId()) + "/DeviceList/" +
                        std::to_string(dev_index) + "/$ns3::WifiNetDevice/Phy/State/State",
                    MakeCallback(&NodeStatistics::MonitorPhyState, this));
}

bool
NodeStatistics::isFromFlow(std::string s)
{
    if (s.rfind("ns3::UdpHeader") != std::string::npos && s.rfind("> 150") != std::string::npos)
        return true;
    return false;
}

// ----------- RX -----------------
void
NodeStatistics::PhyRx(std::string context,
                      Ptr<const Packet> packet,
                      uint16_t channelFreqMhz,
                      WifiTxVector txVector,
                      MpduInfo aMpdu,
                      SignalNoiseDbm signalNoise,
                      uint16_t staId)
{
    if (Simulator::Now().GetSeconds() <= 21.001)
    {
        std::string s = packet->ToString();
        if (s.rfind(" 10.1.1.1 > 10.1.1.")) // some packet comming from node 0, to which i'm sending
                                            // my flow after
            snr = signalNoise.signal -
                  signalNoise.noise; // the snr should always be the same in static scenarios
    }

    if (Simulator::Now().GetSeconds() >= 20.0)
    {
        phy_rx_bytes += packet->GetSize();
    }
}

// ----------------------------------
// ---------------Tx -----------------
void
NodeStatistics::MacTx(std::string context, Ptr<const Packet> p)
{
    // for PERIOD [20, 21] : before new flow
    // tudo o que vem de camadas superiores
    mac_bytesTotal += p->GetSize();

    if (Simulator::Now().GetSeconds() >= 20)
    {
        std::string s = p->ToString();
        if (s.rfind("ns3::Ipv4Header") !=
            std::string::npos) // s.rfind("ns3::UdpHeader") != std::string::npos && s.rfind("698")
                               // == std::string::npos)
        {
            mac_pktsSizeSum += p->GetSize();
            mac_pktsTotal++;
        }
    }
}

void
NodeStatistics::PhyTx(std::string context,
                      Ptr<const Packet> p,
                      uint16_t channelFreqMhz,
                      WifiTxVector txVector,
                      MpduInfo aMpdu,
                      uint16_t staId)
{
    if (Simulator::Now().GetSeconds() >= 20.0)
    {
        phy_bytesTotal += p->GetSize();

        std::string s(p->ToString());
        // if (s.rfind("ns3::Ipv4Header") != std::string::npos &&
        //     std::find(phy_pkts.begin(), phy_pkts.end(), (int)p->GetUid()) == phy_pkts.end())
        if (s.rfind("ns3::Ipv4Header") != std::string::npos)
        {
            // std::cout << p->GetUid() << " : " << s << std::endl;
            if (std::find(phy_pkts.begin(), phy_pkts.end(), (int)p->GetUid()) == phy_pkts.end())
            {
                // for PERIOD [20, 21] : before new flow
                phy_pkts.push_back((int)p->GetUid());
                phy_pktsTotal++;
                NS_ASSERT_MSG(phy_pktsTotal == phy_pkts.size(), "Something went wrong!");

                phy_pktsSizeSum += p->GetSize();
                // std::cout << p->GetUid() << " : " << s << std::endl;

                if (isFromFlow(s))
                {
                    // for PERIOD [21, 22] : after new flow
                    phy_pktsFlow++;
                    phy_bytesFlow += p->GetSize();
                }
            }
        }
    }
}

void
NodeStatistics::Ipv4L3Tx(std::string context,
                         Ptr<const Packet> p,
                         Ptr<Ipv4> ipv4,
                         uint32_t interface)
{
    // for PERIOD [20, 21] : before new flow
    // if (Simulator::Now().GetSeconds() >= 20.0)
    // {
    //     std::string s = p->ToString();
    //     // std::cout << s << std::endl;
    //     if (s.rfind("ns3::Ipv4Header") !=
    //         std::string::npos) //&& s.rfind("698") == std::string::npos)
    //     {
    //         l3_bytesTotal += p->GetSize();
    //         l3_pktsTotal++;
    //         l3_pkts.push_back(s);

    //         // std::cout << mac_bytesTotal << std::endl;
    //     }
    // }
}

void
NodeStatistics::ArpL3DropTx(std::string context, Ptr<const Packet> packet)
{
    // if (context.rfind("/NodeList/1") != std::string::npos)
    // {
    //     l3_bytesTotal -= packet->GetSize();
    //     l3_pktsTotal--;
    //     auto position = std::find(l3_pkts.begin(), l3_pkts.end(), packet->ToString());
    //     if (position != l3_pkts.end())
    //         l3_pkts.erase(position);
    // }
}

void
NodeStatistics::TcTx(std::string context, Ptr<const Packet> p)
{
    if (Simulator::Now().GetSeconds() >= 20.0)
    {
        std::string s = p->ToString();
        // std::cout << s << std::endl;
        //  Everything is Ipv4, but it does not have the header yet
        if (s.rfind("ns3::UdpHeader") != std::string::npos ||
            s.rfind("ns3::TcpHeader") !=
                std::string::npos) //&& s.rfind("698") == std::string::npos)
        {
            l3_bytesTotal += p->GetSize() + 20; // plus ipv4header
            l3_pktsTotal++;
            // std::cout << mac_bytesTotal << std::endl;
        }
    }
}

void
NodeStatistics::RxCallback(std::string path, Ptr<const Packet> packet, const Address& from)
{
    ;
}

int
NodeStatistics::GetAvBw()
{
    return avbw;
}

int
NodeStatistics::GetMaxC()
{
    return maxCapacity;
}

uint8_t
NodeStatistics::GetIdleRatio()
{
    return t_idle;
}

void
NodeStatistics::PeriodicallyResetVars()
{
    // 0. assign avbw (get value)
    pktRequired = false;
    int capacity = 0;
    double time_tx, time_rx, time_idle, time_busy;
    time_tx = timers[WifiPhyState::TX].GetSeconds();
    time_rx = timers[WifiPhyState::RX].GetSeconds();
    time_idle = timers[WifiPhyState::IDLE].GetSeconds();
    time_busy = timers[WifiPhyState::CCA_BUSY].GetSeconds();

    t_idle = time_idle * 100;

    NS_LOG_DEBUG("[" + std::to_string(m_node->GetId()) + "]" << time_tx << " " << time_rx << " "
                                                             << time_idle << " " << time_busy);
    NS_ASSERT_MSG(time_tx + time_rx + time_idle + time_busy > 0.9,
                  time_tx << " " << time_rx << " " << time_idle << " " << time_busy);

    if (time_tx < 0.05)
    {
        capacity = maxCapacity * 0.9;
    }
    else if ((time_idle <= 0.15) || (time_idle <= 0.2 && time_busy >= 0.05))
    {
        capacity = maxCapacity * 0.9 * 0.8 * 0.5;
    }
    else
    {
        capacity = (int)(phy_bytesTotal * 8 / (time_tx * period)); // eq3
        pktRequired = true;
        maxCapacity = capacity * (540.0 / 582.0); // does capacity change with distance??
        NS_LOG_DEBUG("Max Capacity " << maxCapacity << "\n");
        // if (capacity * (540.0 / 582.0) > maxCapacity && capacity * (540.0 / 582.0) < 27000000)
        //     maxCapacity = capacity * (540.0 / 582.0);
    }
    avbw = capacity * time_idle;
    NS_LOG_DEBUG("[" + std::to_string(m_node->GetId()) +
                     "]    AvBw |   phy_B   | Capa    |   Idle time(s) \n"
                 << "   " << avbw << ", " << phy_bytesTotal << ", " << capacity << ", " << time_idle
                 << "\n");
    // 1. reset values
    l3_bytesTotal = 0;
    mac_bytesTotal = 0;
    phy_bytesTotal = 0; //!< total bytes

    // Pkts
    l3_pktsTotal = 0;
    mac_pktsTotal = 0;
    phy_pktsTotal = 0; //!< total pkts udp

    // pkts size
    // l3_pktsSizeSum = 0;
    mac_pktsSizeSum = 0;
    phy_pktsSizeSum = 0;

    // Pkts Flow
    l3_pktsFlow = 0; //!< total pkts flow id=150
    mac_pktsFlow = 0;
    phy_pktsFlow = 0;
    phy_rx_bytes = 0;
    phy_bytesFlow = 0;
    phy_pkts.clear();

    // 2. reset timers and counters
    timers.clear();
    NS_ASSERT_MSG(avbw > 0, "AVbw is zero " << capacity << " " << time_idle);

    // 3. schedule next reset
    Simulator::Schedule(Seconds(period), &NodeStatistics::PeriodicallyResetVars, this);
}

void
NodeStatistics::MonitorPhyState(std::string context, Time start, Time duration, WifiPhyState state)
{
    Time now = Simulator::Now();
    NS_LOG_DEBUG("[" + std::to_string(m_node->GetId()) + "]:" << now << "- MonitorPhyState (start) "
                                                              << start << " , (duration) "
                                                              << duration << " -- " << state);

    lastDuration = duration;
    lastStart = start;
    Time event_end = start + duration;

    double delta_ns = std::fmod(now.GetNanoSeconds(),
                                period * 1000000000); // Note: remainder can give negative values
    Time delta = Time(NanoSeconds(delta_ns));
    Time p_t0 = now - delta;

    // NS_ASSERT_MSG(event_end >= p_t0,
    //               "Event does not happen inside this period!" << event_end << p_t0);
    if (event_end < p_t0)
    {
        NS_LOG_DEBUG("Event does not happen inside this period!" << event_end << p_t0);
        return;
    }

    if (start < p_t0) // start before begining of this period
    {
        duration = delta;
        // std::cout << "update duration " << p_t0 << " " << duration << std::endl; // TODO
    }
    if (event_end > p_t0 + Time(Seconds(period)))
    {
        // NS_ASSERT_MSG(event_end <= p_t0 + Time(Seconds(period)),
        //               "Event does not happen inside this period!" << event_end << p_t0
        //                                                           << Time(Seconds(period)));

        duration = duration -
                   Time(NanoSeconds(std::fmod(event_end.GetNanoSeconds(), period * 1000000000)));
    }
    timers[state] += duration;

    /*if (m_node->GetId() == 0)
    {
        for (const auto& pair : timers)
        {
            NS_LOG_DEBUG(pair.first << " -> " << pair.second);
        }
    }*/
}

void
NodeStatistics::StateFinalTimer()
{ // (condition) ? expressionTrue : expressionFalse;
    Time now = Simulator::Now();

    auto curr_state = GetState();

    Time lastEnd = lastDuration + lastStart;

    Time duration = now - lastEnd;

    NS_LOG_DEBUG("[" + std::to_string(m_node->GetId()) + "]:"
                 << now << "- StateFinalTimer " << curr_state << " , (duration) " << duration);

    if (duration.GetSeconds() > period)
    {
        duration = Time(Seconds(period));
    }
    timers[curr_state] += duration;
    /*if (m_node->GetId() == 0)
    {
        for (const auto& pair : timers)
        {
            NS_LOG_DEBUG(pair.first << " -> " << pair.second);
        }
    }*/

    Simulator::Schedule(Seconds(period), &NodeStatistics::StateFinalTimer, this);
}

WifiPhyState
NodeStatistics::GetState()
{
    WifiPhyState currentState;
    PointerValue ptr;

    Ptr<WifiPhy> phy = DynamicCast<WifiPhy>(device->GetPhy());
    phy->GetAttribute("State", ptr);
    Ptr<WifiPhyStateHelper> state = DynamicCast<WifiPhyStateHelper>(ptr.Get<WifiPhyStateHelper>());
    currentState = state->GetState();
    NS_LOG_DEBUG("[" + std::to_string(m_node->GetId()) + "]" << "GetState" << currentState);
    return currentState;
}

} // namespace ns3