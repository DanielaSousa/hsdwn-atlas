#include "logging-helper.h"

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/olsr-routing-protocol.h"

namespace ns3
{

// using namespace olsr;

LoggingHelper::LoggingHelper()
{
    m_monitorFactory.SetTypeId("ns3::Logging");
    // TODO create monitor
    m_monitor = m_monitorFactory.Create<Logging>();
}

LoggingHelper::~LoggingHelper()
{
    if (m_monitor)
    {
        m_monitor->Dispose();
        m_monitor = 0;
    }
}

Ptr<Logging>
LoggingHelper::Install(NodeContainer nodes)
{
    for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i)
    {
        Ptr<Node> node = *i;
        if (node->GetObject<Ipv4L3Protocol>())
        {
            Install(node);
        }
    }
    return m_monitor;
}

Ptr<Logging>
LoggingHelper::Install(Ptr<Node> node)
{
    Ptr<Ipv4L3Protocol> ipv4l3 = node->GetObject<Ipv4L3Protocol>();
    if (ipv4l3)
    {
        Ptr<NodeProbe> probe = Create<NodeProbe>(m_monitor, node);
        m_monitor->AddProbe(probe);

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol();
        NS_ASSERT_MSG(ipv4rp, "No routing protocol associated with Ipv4");
        Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting>(ipv4rp);
        NS_ASSERT_MSG(lrp, "No Ipv4ListRouting");
        int16_t priority = 10;
        Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol(0, priority);
        NS_ASSERT_MSG(temp, "No Ipv4RoutingProtocol");
        m_monitor->rtg[node->GetId()] = DynamicCast<olsr::RoutingProtocol>(temp);
        NS_ASSERT_MSG(m_monitor->rtg[node->GetId()], "No olsr::RoutingProtocol");
    }
    return m_monitor;
}

void
LoggingHelper::SaveToFile(Time t, std::string fileName)
{
    m_monitor->log_path = fileName;
    // write header in file
    std::ofstream logfile_full;
    logfile_full.open(fileName, std::ofstream::out);

    logfile_full << "log_id,node_id,timestamp,";
    logfile_full << "packets_rx_total,bytes_rx_total,packets_tx_total,bytes_tx_total,packet_tx_"
                    "forwarded_total,";
    logfile_full
        << "packets_rcv_total,packets_own_total,packets_Tx_Bcast_total,packets_Rx_Bcast_total,";
    logfile_full << "packets_opf_action_normal,packets_opf_rcv_total,packets_opf_own_total,";
    logfile_full << "phy_snr_sum,phy_total_droped,phy_total_listened,";
    logfile_full << "delaySum_total,";
    logfile_full << "n_neighbours,energy,loc_x,loc_y\n";
    logfile_full.close();
    m_monitor->PeriodicSave();
}
}; // namespace ns3