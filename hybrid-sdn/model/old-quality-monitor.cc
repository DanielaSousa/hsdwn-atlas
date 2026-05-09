
#include "quality-monitor.h"

#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-flow-probe.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/ipv6-flow-probe.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/node.h"
#include "ns3/node-list.h"


namespace ns3 {

// Global variables for use in callbacks.
double g_signalDbmAvg;
double g_noiseDbmAvg;
uint32_t g_samples;
int i = 0;

std::pair <int,int> ParseContext(std::string context){
    std::string delimiter = "/";
    size_t pos = context.find(delimiter);
    int tmp_node, tmp_dev;
    // _/NodeList/0/DeviceList/0/Phy/MonitorSnifferRx
    context.erase(0, pos + delimiter.length()); //_
    context.erase(0, pos + delimiter.length()); //NodeList
    tmp_node = std::atoi(context.substr(0, pos).c_str());
    context.erase(0, pos + delimiter.length()); //*
    context.erase(0, pos + delimiter.length()); // DeviceList
    tmp_dev = std::atoi(context.substr(0, pos).c_str());
    // while ((pos = context.find(delimiter)) != std::string::npos) {
    //     std::cout << "\n" << context.substr(0, pos);
    //     // tmp_node =
    //     // std::cout << tmp_node << std::endl;
    //     context.erase(0, pos + delimiter.length());
    // }
    std::cout << tmp_node << " " << tmp_dev << std::endl;
    return std::make_pair(tmp_node,tmp_dev);
}


QualityMonitor::QualityMonitor(/* args */)
  : m_enabled (false)
{
    NS_LOG_FUNCTION (this);
}


QualityMonitor::~QualityMonitor()
{
}

int QualityMonitor::Install(NodeContainer nodes)
{

    // //install flowmonitors

    // //install energy
    // 0. ns3::SimpleDeviceEnergyModel   TotalEnergyConsumption: Total energy consumption of the radio device.

    // //install traces
    // 1. OLSR 'RoutingTableChanged': The OLSR routing table has changed.
    // 2.

    //Config::ConnectWithoutContext ("/NodeList/0/DeviceList/*/Phy/MonitorSnifferRx", MakeCallback (&QualityMonitor::MonitorSniffRx, this));

    m_classifier = Create<Ipv4FlowClassifier> ();

    return 0;
}
int QualityMonitor::Install(Ptr<ns3::Node> node){

    // create classifier
    if(!m_classifier) m_classifier = Create<Ipv4FlowClassifier> ();


    //create ipv4flow probe
    Ptr<Ipv4L3Protocol> m_ipv4 = node->GetObject<Ipv4L3Protocol> ();

   if (!m_ipv4->TraceConnectWithoutContext ("SendOutgoing",
                                            MakeCallback (&QualityMonitor::SendOutgoingLogger, Ptr<QualityMonitor> (this))))
     {
       NS_FATAL_ERROR ("trace fail");
     }
   if (!m_ipv4->TraceConnectWithoutContext ("UnicastForward",
                                            MakeCallback (&QualityMonitor::ForwardLogger, Ptr<QualityMonitor> (this))))
     {
       NS_FATAL_ERROR ("trace fail");
     }
   if (!m_ipv4->TraceConnectWithoutContext ("LocalDeliver",
                                            MakeCallback (&QualityMonitor::ForwardUpLogger, Ptr<QualityMonitor> (this))))
     {
       NS_FATAL_ERROR ("trace fail");
     }

   if (!m_ipv4->TraceConnectWithoutContext ("Drop",
                                            MakeCallback (&QualityMonitor::DropLogger, Ptr<QualityMonitor> (this))))
     {
       NS_FATAL_ERROR ("trace fail");
     }

   std::ostringstream qd;
   qd << "/NodeList/" << node->GetId () << "/$ns3::TrafficControlLayer/RootQueueDiscList/*/Drop";
   Config::ConnectWithoutContextFailSafe (qd.str (), MakeCallback (&QualityMonitor::QueueDiscDropLogger, Ptr<QualityMonitor> (this)));

   // code copied from point-to-point-helper.cc
   std::ostringstream oss;
   oss << "/NodeList/" << node->GetId () << "/DeviceList/*/TxQueue/Drop";
   Config::ConnectWithoutContextFailSafe (oss.str (), MakeCallback (&QualityMonitor::QueueDropLogger, Ptr<QualityMonitor> (this)));
    // install the radiotapheader
    return 0;


}

void
 QualityMonitor::DoDispose (void)
 {
    NS_LOG_FUNCTION (this);
    Simulator::Cancel (m_startEvent);
    Simulator::Cancel (m_stopEvent);
    for (std::list<Ptr<FlowClassifier> >::iterator iter = m_classifiers.begin ();
        iter != m_classifiers.end ();
        iter ++)
        {
        *iter = 0;
        }
    for (uint32_t i = 0; i < m_flowProbes.size (); i++)
        {
        m_flowProbes[i]->Dispose ();
        m_flowProbes[i] = 0;
        }
    Object::DoDispose ();

    //ipv4flowprobe
    m_ipv4 = 0;
    m_classifier = 0;
    FlowProbe::DoDispose();
 }


void
 QualityMonitor::CheckForLostPackets (Time maxDelay)
 {
   NS_LOG_FUNCTION (this << maxDelay.As (Time::S));
   Time now = Simulator::Now ();

   for (TrackedPacketMap::iterator iter = m_trackedPackets.begin ();
        iter != m_trackedPackets.end (); )
     {
       if (now - iter->second.lastSeenTime >= maxDelay)
         {
           // packet is considered lost, add it to the loss statistics
           FlowStatsContainerI flow = m_flowStats.find (iter->first.first);
           NS_ASSERT (flow != m_flowStats.end ());
           flow->second.lostPackets++;

           // we won't track it anymore
           m_trackedPackets.erase (iter++);
         }
       else
         {
           iter++;
         }
     }
 }




void QualityMonitor::MonitorSniffRx (std::string context, Ptr<const Packet> packet,
                     uint16_t channelFreqMhz,
                     WifiTxVector txVector,
                     MpduInfo aMpdu,
                     SignalNoiseDbm signalNoise,
                     uint16_t staId)

{

    std::pair <int,int> tmp = ParseContext(context);
    std::cout << context  << tmp.first << tmp.second << std::endl;


  g_samples++;
  g_signalDbmAvg += ((signalNoise.signal - g_signalDbmAvg) / g_samples);
  g_noiseDbmAvg += ((signalNoise.noise - g_noiseDbmAvg) / g_samples);

    packet->Print(std::cout);
    std::cout << "Signal (dBm) = " << g_signalDbmAvg <<
        " Noise (dBm)= " << g_noiseDbmAvg <<
        " SNR (dB)= " << (g_signalDbmAvg - g_noiseDbmAvg) <<
        std::endl;

    // struct SignalNoiseDbm
    //     {
    //     double signal; ///< signal strength in dBm
    //     double noise;  ///< noise power in dBm
    //     };
    /// RxSignalInfo structure containing info on the received signal
    // struct RxSignalInfo
    // {
    // double snr;  ///< SNR in linear scale
    // double rssi; ///< RSSI in dBm
    // };
}


 TypeId
 QualityMonitor::GetTypeId (void)
 {
   static TypeId tid = TypeId ("ns3::FlowMonitor")
     .SetParent<Object> ()
     .SetGroupName ("FlowMonitor")
     .AddConstructor<FlowMonitor> ()
     .AddAttribute ("MaxPerHopDelay", ("The maximum per-hop delay that should be considered.  "
                                       "Packets still not received after this delay are to be considered lost."),
                    TimeValue (Seconds (10.0)),
                    MakeTimeAccessor (&FlowMonitor::m_maxPerHopDelay),
                    MakeTimeChecker ())
     .AddAttribute ("StartTime", ("The time when the monitoring starts."),
                    TimeValue (Seconds (0.0)),
                    MakeTimeAccessor (&FlowMonitor::Start),
                    MakeTimeChecker ())
     .AddAttribute ("DelayBinWidth", ("The width used in the delay histogram."),
                    DoubleValue (0.001),
                    MakeDoubleAccessor (&FlowMonitor::m_delayBinWidth),
                    MakeDoubleChecker <double> ())
     .AddAttribute ("JitterBinWidth", ("The width used in the jitter histogram."),
                    DoubleValue (0.001),
                    MakeDoubleAccessor (&FlowMonitor::m_jitterBinWidth),
                    MakeDoubleChecker <double> ())
     .AddAttribute ("PacketSizeBinWidth", ("The width used in the packetSize histogram."),
                    DoubleValue (20),
                    MakeDoubleAccessor (&FlowMonitor::m_packetSizeBinWidth),
                    MakeDoubleChecker <double> ())
     .AddAttribute ("FlowInterruptionsBinWidth", ("The width used in the flowInterruptions histogram."),
                    DoubleValue (0.250),
                    MakeDoubleAccessor (&FlowMonitor::m_flowInterruptionsBinWidth),
                    MakeDoubleChecker <double> ())
     .AddAttribute ("FlowInterruptionsMinTime", ("The minimum inter-arrival time that is considered a flow interruption."),
                    TimeValue (Seconds (0.5)),
                    MakeTimeAccessor (&FlowMonitor::m_flowInterruptionsMinTime),
                    MakeTimeChecker ())
   ;
   return tid;
 }

}