
#include "ns3/logging.h"

#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/olsr-module.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Logging");

NS_OBJECT_ENSURE_REGISTERED(Logging);

using namespace olsr;

TypeId
Logging::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::Logging")
                            .SetParent<Object>()
                            .SetGroupName("Logging")
                            .AddConstructor<Logging>();
    return tid;
}

TypeId
Logging::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

Logging::Logging()
{
}

void
Logging::NotifyConstructionCompleted()
{
    Object::NotifyConstructionCompleted();
    // Simulator::Schedule (PERIODIC_CHECK_INTERVAL, &Logging::PeriodicCheckForLostPackets, this);
}

void
Logging::saveNodeStats(Ptr<NodeProbe> p, int seconds)
{
    std::ofstream logfile_full;
    logfile_full.open(log_path, std::ios_base::app | std::ofstream::out); // append or overwrite
    // log_id, node_id, timestamp,
    logfile_full << log_id << ",";
    logfile_full << p->GetNode()->GetId() << ",";
    logfile_full << seconds << ",";

    // packets_rx_total, bytes_rx_total, packets_tx_total, bytes_tx_total, packet_tx_forwarded_total
    logfile_full << p->packets_rx_total << ',';
    logfile_full << p->bytes_rx_total << ",";
    logfile_full << p->packets_tx_total << ",";
    logfile_full << p->bytes_tx_total << ',';
    logfile_full << p->packet_tx_forwarded_total << ',';

    // packets_rcv_total,packets_own_total,packets_Tx_Bcast_total,packets_Rx_Bcast_total
    logfile_full << p->packets_rcv_total << ",";
    logfile_full << p->packets_own_total << ",";
    logfile_full << p->packets_Tx_Bcast_total << ",";
    logfile_full << p->packets_Rx_Bcast_total << ",";

    // packets_opf_action_normal,packets_opf_rcv_total,packets_opf_own_total
    logfile_full << p->packets_opf_action_normal << ",";
    logfile_full << p->packets_opf_rcv_total << ",";
    logfile_full << p->packets_opf_own_total << ",";

    // phy_snr_sum,phy_total_droped,phy_total_listened
    logfile_full << p->phy_snr_sum << ",";
    logfile_full << p->phy_total_droped << ",";
    logfile_full << p->phy_total_listened << ",";

    // delaySum_total,contacts,energy
    logfile_full << p->delaySum_total.ToDouble(Time::Unit::S) << ","; //!!

    // get n_neighbours
    int n = 0;
    if (rtg[p->GetNode()->GetId()])
    {
        n = rtg[p->GetNode()->GetId()]->GetNeighbors().size();
    }

    logfile_full << n << ",";

    // energy
    logfile_full << p->energy;

    // location
    Vector pos;
    Ptr<Node> node = p->GetNode();
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    pos = mob->GetPosition();
    logfile_full << "," << pos.x << "," << pos.y;

    // endline
    logfile_full << "\n";
    logfile_full.close();

    /* clear variables */
    p->ClearTimestampVariables();
}

void
Logging::SaveToFile()
{
    // FOR EACH PROBE:
    for (auto i = m_probes.begin(); i != m_probes.end(); ++i)
    {
        Ptr<NodeProbe> p = (*i);

        // save node stats
        // saveNodeStats( (*i), Simulator::Now().GetSeconds());
        saveNodeStats((*i), Simulator::Now().ToInteger(Time::Unit::S));
        // std::cout << "TIME UNIT " <<Simulator::Now().ToInteger(Time::Unit::S) << std::endl;
    }
    log_id++;
}

void
Logging::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    //   Simulator::Cancel (m_startEvent);
    //   Simulator::Cancel (m_stopEvent);

    for (uint32_t i = 0; i < m_probes.size(); i++)
    {
        m_probes[i]->Dispose();
        m_probes[i] = 0;
    }
    // Object::DoDispose ();
}

void
Logging::AddProbe(Ptr<NodeProbe> p)
{
    m_probes.push_back(p);
}

void
Logging::PeriodicSave()
{
    SaveToFile();
    // #define PERIODIC_CHECK_INTERVAL (Seconds (1))
    Simulator::Schedule(Seconds(1.0), &Logging::PeriodicSave, this);
}

}; // namespace ns3