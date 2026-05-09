
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

#define PERIODIC_CHECK_INTERVAL (Seconds (1))

namespace ns3 {

    //QualityMonitor




//flow monitor
 inline QualityMonitor::FlowStats&
 QualityMonitor::GetStatsForFlow (FlowId flowId)
 {
   NS_LOG_FUNCTION (this);
   FlowStatsContainerI iter;
   iter = m_flowStats.find (flowId);
   if (iter == m_flowStats.end ())
     {
       QualityMonitor::FlowStats &ref = m_flowStats[flowId];
       ref.delaySum = Seconds (0);
       ref.jitterSum = Seconds (0);
       ref.lastDelay = Seconds (0);
       ref.txBytes = 0;
       ref.rxBytes = 0;
       ref.txPackets = 0;
       ref.rxPackets = 0;
       ref.lostPackets = 0;
       ref.timesForwarded = 0;
       ref.delayHistogram.SetDefaultBinWidth (m_delayBinWidth);
       ref.jitterHistogram.SetDefaultBinWidth (m_jitterBinWidth);
       ref.packetSizeHistogram.SetDefaultBinWidth (m_packetSizeBinWidth);
       ref.flowInterruptionsHistogram.SetDefaultBinWidth (m_flowInterruptionsBinWidth);
       return ref;
     }
   else
     {
       return iter->second;
     }
 }


 void
 QualityMonitor::ReportFirstTx (Ptr<FlowProbe> probe, uint32_t flowId, uint32_t packetId, uint32_t packetSize)
 {
   NS_LOG_FUNCTION (this << probe << flowId << packetId << packetSize);
   if (!m_enabled)
     {
       NS_LOG_DEBUG ("FlowMonitor not enabled; returning");
       return;
     }
   Time now = Simulator::Now ();
   TrackedPacket &tracked = m_trackedPackets[std::make_pair (flowId, packetId)];
   tracked.firstSeenTime = now;
   tracked.lastSeenTime = tracked.firstSeenTime;
   tracked.timesForwarded = 0;
   NS_LOG_DEBUG ("ReportFirstTx: adding tracked packet (flowId=" << flowId << ", packetId=" << packetId
                                                                 << ").");

   probe->AddPacketStats (flowId, packetSize, Seconds (0));

   FlowStats &stats = GetStatsForFlow (flowId);
   stats.txBytes += packetSize;
   stats.txPackets++;
   if (stats.txPackets == 1)
     {
       stats.timeFirstTxPacket = now;
     }
   stats.timeLastTxPacket = now;
 }


 void
 QualityMonitor::ReportForwarding (Ptr<FlowProbe> probe, uint32_t flowId, uint32_t packetId, uint32_t packetSize)
 {
   NS_LOG_FUNCTION (this << probe << flowId << packetId << packetSize);
   if (!m_enabled)
     {
       NS_LOG_DEBUG ("FlowMonitor not enabled; returning");
       return;
     }
   std::pair<FlowId, FlowPacketId> key (flowId, packetId);
   TrackedPacketMap::iterator tracked = m_trackedPackets.find (key);
   if (tracked == m_trackedPackets.end ())
     {
       NS_LOG_WARN ("Received packet forward report (flowId=" << flowId << ", packetId=" << packetId
                                                              << ") but not known to be transmitted.");
       return;
     }

   tracked->second.timesForwarded++;
   tracked->second.lastSeenTime = Simulator::Now ();

   Time delay = (Simulator::Now () - tracked->second.firstSeenTime);
   probe->AddPacketStats (flowId, packetSize, delay);
 }


 void
 QualityMonitor::ReportLastRx (Ptr<FlowProbe> probe, uint32_t flowId, uint32_t packetId, uint32_t packetSize)
 {
   NS_LOG_FUNCTION (this << probe << flowId << packetId << packetSize);
   if (!m_enabled)
     {
       NS_LOG_DEBUG ("FlowMonitor not enabled; returning");
       return;
     }
   TrackedPacketMap::iterator tracked = m_trackedPackets.find (std::make_pair (flowId, packetId));
   if (tracked == m_trackedPackets.end ())
     {
       NS_LOG_WARN ("Received packet last-tx report (flowId=" << flowId << ", packetId=" << packetId
                                                              << ") but not known to be transmitted.");
       return;
     }

   Time now = Simulator::Now ();
   Time delay = (now - tracked->second.firstSeenTime);
   probe->AddPacketStats (flowId, packetSize, delay);

   FlowStats &stats = GetStatsForFlow (flowId);
   stats.delaySum += delay;
   stats.delayHistogram.AddValue (delay.GetSeconds ());
   if (stats.rxPackets > 0 )
     {
       Time jitter = stats.lastDelay - delay;
       if (jitter > Seconds (0))
         {
           stats.jitterSum += jitter;
           stats.jitterHistogram.AddValue (jitter.GetSeconds ());
         }
       else
         {
           stats.jitterSum -= jitter;
           stats.jitterHistogram.AddValue (-jitter.GetSeconds ());
         }
     }
   stats.lastDelay = delay;

   stats.rxBytes += packetSize;
   stats.packetSizeHistogram.AddValue ((double) packetSize);
   stats.rxPackets++;
   if (stats.rxPackets == 1)
     {
       stats.timeFirstRxPacket = now;
     }
   else
     {
       // measure possible flow interruptions
       Time interArrivalTime = now - stats.timeLastRxPacket;
       if (interArrivalTime > m_flowInterruptionsMinTime)
         {
           stats.flowInterruptionsHistogram.AddValue (interArrivalTime.GetSeconds ());
         }
     }
   stats.timeLastRxPacket = now;
   stats.timesForwarded += tracked->second.timesForwarded;

   NS_LOG_DEBUG ("ReportLastTx: removing tracked packet (flowId="
                 << flowId << ", packetId=" << packetId << ").");

   m_trackedPackets.erase (tracked); // we don't need to track this packet anymore
 }

 void
 QualityMonitor::ReportDrop (Ptr<FlowProbe> probe, uint32_t flowId, uint32_t packetId, uint32_t packetSize,
                          uint32_t reasonCode)
 {
   NS_LOG_FUNCTION (this << probe << flowId << packetId << packetSize << reasonCode);
   if (!m_enabled)
     {
       NS_LOG_DEBUG ("FlowMonitor not enabled; returning");
       return;
     }

   probe->AddPacketDropStats (flowId, packetSize, reasonCode);

   FlowStats &stats = GetStatsForFlow (flowId);
   stats.lostPackets++;
   if (stats.packetsDropped.size () < reasonCode + 1)
     {
       stats.packetsDropped.resize (reasonCode + 1, 0);
       stats.bytesDropped.resize (reasonCode + 1, 0);
     }
   ++stats.packetsDropped[reasonCode];
   stats.bytesDropped[reasonCode] += packetSize;
   NS_LOG_DEBUG ("++stats.packetsDropped[" << reasonCode<< "]; // becomes: " << stats.packetsDropped[reasonCode]);

   TrackedPacketMap::iterator tracked = m_trackedPackets.find (std::make_pair (flowId, packetId));
   if (tracked != m_trackedPackets.end ())
     {
       // we don't need to track this packet anymore
       // FIXME: this will not necessarily be true with broadcast/multicast
       NS_LOG_DEBUG ("ReportDrop: removing tracked packet (flowId="
                     << flowId << ", packetId=" << packetId << ").");
       m_trackedPackets.erase (tracked);
     }
 }

 const QualityMonitor::FlowStatsContainer&
 QualityMonitor::GetFlowStats () const
 {
   return m_flowStats;
 }


void
 QualityMonitor::NotifyConstructionCompleted ()
 {
   Object::NotifyConstructionCompleted ();
   Simulator::Schedule (PERIODIC_CHECK_INTERVAL, &QualityMonitor::PeriodicCheckForLostPackets, this);
 }

 void
 QualityMonitor::AddProbe (Ptr<FlowProbe> probe)
 {
   m_flowProbes.push_back (probe);
 }


 const QualityMonitor::FlowProbeContainer&
 QualityMonitor::GetAllProbes () const
 {
   return m_flowProbes;
 }


//--------------------------------------------
void
 QualityMonitor::PeriodicCheckForLostPackets ()
 {
   CheckForLostPackets (m_maxPerHopDelay);
   Simulator::Schedule (PERIODIC_CHECK_INTERVAL, &QualityMonitor::PeriodicCheckForLostPackets, this);
 }


//------------------ SIMULATION -----------------
void
QualityMonitor::Start (const Time &time) {
    NS_LOG_FUNCTION (this << time.As (Time::S));
    if (m_enabled)
        {
        NS_LOG_DEBUG ("FlowMonitor already enabled; returning");
        return;
        }
    Simulator::Cancel (m_startEvent);
    NS_LOG_DEBUG ("Scheduling start at " << time.As (Time::S));
    m_startEvent = Simulator::Schedule (time, &QualityMonitor::StartRightNow, this);
}

void
QualityMonitor::Stop (const Time &time) {
    NS_LOG_FUNCTION (this << time.As (Time::S));
    Simulator::Cancel (m_stopEvent);
    NS_LOG_DEBUG ("Scheduling stop at " << time.As (Time::S));
    m_stopEvent = Simulator::Schedule (time, &QualityMonitor::StopRightNow, this);
}


void
QualityMonitor::StartRightNow () {
    NS_LOG_FUNCTION (this);
    if (m_enabled)
        {
        NS_LOG_DEBUG ("FlowMonitor already enabled; returning");
        return;
        }
    m_enabled = true;
}


void
QualityMonitor::StopRightNow () {
    NS_LOG_FUNCTION (this);
    if (!m_enabled)
        {
        NS_LOG_DEBUG ("FlowMonitor not enabled; returning");
        return;
        }
    m_enabled = false;
    CheckForLostPackets (m_maxPerHopDelay);
}

void
 QualityMonitor::AddFlowClassifier (Ptr<FlowClassifier> classifier)
 {
   m_classifiers.push_back (classifier);
 }

 void
 QualityMonitor::SerializeToXmlStream (std::ostream &os, uint16_t indent, bool enableHistograms, bool enableProbes)
 {
   NS_LOG_FUNCTION (this << indent << enableHistograms << enableProbes);
   CheckForLostPackets (m_maxPerHopDelay);

   os << std::string ( indent, ' ' ) << "<FlowMonitor>\n";
   indent += 2;
   os << std::string ( indent, ' ' ) << "<FlowStats>\n";
   indent += 2;
   for (FlowStatsContainerCI flowI = m_flowStats.begin ();
        flowI != m_flowStats.end (); flowI++)
     {
       os << std::string ( indent, ' ' );
 #define ATTRIB(name) << " " # name "=\"" << flowI->second.name << "\""
       os << "<Flow flowId=\"" << flowI->first << "\""
       ATTRIB (timeFirstTxPacket)
       ATTRIB (timeFirstRxPacket)
       ATTRIB (timeLastTxPacket)
       ATTRIB (timeLastRxPacket)
       ATTRIB (delaySum)
       ATTRIB (jitterSum)
       ATTRIB (lastDelay)
       ATTRIB (txBytes)
       ATTRIB (rxBytes)
       ATTRIB (txPackets)
       ATTRIB (rxPackets)
       ATTRIB (lostPackets)
       ATTRIB (timesForwarded)
       << ">\n";
 #undef ATTRIB

       indent += 2;
       for (uint32_t reasonCode = 0; reasonCode < flowI->second.packetsDropped.size (); reasonCode++)
         {
           os << std::string ( indent, ' ' );
           os << "<packetsDropped reasonCode=\"" << reasonCode << "\""
           << " number=\"" << flowI->second.packetsDropped[reasonCode]
           << "\" />\n";
         }
       for (uint32_t reasonCode = 0; reasonCode < flowI->second.bytesDropped.size (); reasonCode++)
         {
           os << std::string ( indent, ' ' );
           os << "<bytesDropped reasonCode=\"" << reasonCode << "\""
           << " bytes=\"" << flowI->second.bytesDropped[reasonCode]
           << "\" />\n";
         }
       if (enableHistograms)
         {
           flowI->second.delayHistogram.SerializeToXmlStream (os, indent, "delayHistogram");
           flowI->second.jitterHistogram.SerializeToXmlStream (os, indent, "jitterHistogram");
           flowI->second.packetSizeHistogram.SerializeToXmlStream (os, indent, "packetSizeHistogram");
           flowI->second.flowInterruptionsHistogram.SerializeToXmlStream (os, indent, "flowInterruptionsHistogram");
         }
       indent -= 2;

       os << std::string ( indent, ' ' ) << "</Flow>\n";
     }
   indent -= 2;
   os << std::string ( indent, ' ' ) << "</FlowStats>\n";

   for (std::list<Ptr<FlowClassifier> >::iterator iter = m_classifiers.begin ();
       iter != m_classifiers.end ();
       iter ++)
     {
       (*iter)->SerializeToXmlStream (os, indent);
     }

   if (enableProbes)
     {
       os << std::string ( indent, ' ' ) << "<FlowProbes>\n";
       indent += 2;
       for (uint32_t i = 0; i < m_flowProbes.size (); i++)
         {
           m_flowProbes[i]->SerializeToXmlStream (os, indent, i);
         }
       indent -= 2;
       os << std::string ( indent, ' ' ) << "</FlowProbes>\n";
     }

   indent -= 2;
   os << std::string ( indent, ' ' ) << "</FlowMonitor>\n";
 }


 std::string
 QualityMonitor::SerializeToXmlString (uint16_t indent, bool enableHistograms, bool enableProbes)
 {
   NS_LOG_FUNCTION (this << indent << enableHistograms << enableProbes);
   std::ostringstream os;
   SerializeToXmlStream (os, indent, enableHistograms, enableProbes);
   return os.str ();
 }


 void
 QualityMonitor::SerializeToXmlFile (std::string fileName, bool enableHistograms, bool enableProbes)
 {
   NS_LOG_FUNCTION (this << fileName << enableHistograms << enableProbes);
   std::ofstream os (fileName.c_str (), std::ios::out|std::ios::binary);
   os << "<?xml version=\"1.0\" ?>\n";
   SerializeToXmlStream (os, 0, enableHistograms, enableProbes);
   os.close ();
 }

TypeId
 QualityMonitor::GetInstanceTypeId (void) const
 {
   return GetTypeId ();
 }


 //QualityMonitor
 /* static */
 TypeId
 QualityMonitor::GetTypeId (void)
 {
   static TypeId tid = TypeId ("ns3::QualityMonitor")
     .SetParent<FlowProbe> ()
     .SetGroupName ("FlowMonitor")
     // No AddConstructor because this class has no default constructor.
     ;

   return tid;
 }

 void
 QualityMonitor::SendOutgoingLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface)
 {
   FlowId flowId;
   FlowPacketId packetId;

   if (!m_ipv4->IsUnicast(ipHeader.GetDestination ()))
     {
       // we are not prepared to handle broadcast yet
       return;
     }

   FlowProbeTag fTag;
   bool found = ipPayload->FindFirstMatchingByteTag (fTag);
   if (found)
     {
       return;
     }

   if (m_classifier->Classify (ipHeader, ipPayload, &flowId, &packetId))
     {
       uint32_t size = (ipPayload->GetSize () + ipHeader.GetSerializedSize ());
       NS_LOG_DEBUG ("ReportFirstTx ("<<this<<", "<<flowId<<", "<<packetId<<", "<<size<<"); "
                                      << ipHeader << *ipPayload);
       //m_flowMonitor->ReportFirstTx (this, flowId, packetId, size);
       this->ReportFirstTx (this, flowId, packetId, size);

       // tag the packet with the flow id and packet id, so that the packet can be identified even
       // when Ipv4Header is not accessible at some non-IPv4 protocol layer
       FlowProbeTag fTag (flowId, packetId, size, ipHeader.GetSource (), ipHeader.GetDestination ());
       ipPayload->AddByteTag (fTag);
     }
 }

 void
 QualityMonitor::ForwardLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface)
 {
   FlowProbeTag fTag;
   bool found = ipPayload->FindFirstMatchingByteTag (fTag);

   if (found)
     {
       if (!ipHeader.IsLastFragment () || ipHeader.GetFragmentOffset () != 0)
         {
           NS_LOG_WARN ("Not counting fragmented packets");
           return;
         }
       if (!fTag.IsSrcDstValid (ipHeader.GetSource (), ipHeader.GetDestination ()))
         {
           NS_LOG_LOGIC ("Not reporting encapsulated packet");
           return;
         }

       FlowId flowId = fTag.GetFlowId ();
       FlowPacketId packetId = fTag.GetPacketId ();

       uint32_t size = (ipPayload->GetSize () + ipHeader.GetSerializedSize ());
       NS_LOG_DEBUG ("ReportForwarding ("<<this<<", "<<flowId<<", "<<packetId<<", "<<size<<");");
       //m_flowMonitor->ReportForwarding (this, flowId, packetId, size);
       this->ReportForwarding (this, flowId, packetId, size);
     }
 }

 void
 QualityMonitor::ForwardUpLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface)
 {
   FlowProbeTag fTag;
   bool found = ipPayload->FindFirstMatchingByteTag (fTag);

   if (found)
     {
       if (!fTag.IsSrcDstValid (ipHeader.GetSource (), ipHeader.GetDestination ()))
         {
           NS_LOG_LOGIC ("Not reporting encapsulated packet");
           return;
         }

       FlowId flowId = fTag.GetFlowId ();
       FlowPacketId packetId = fTag.GetPacketId ();

       uint32_t size = (ipPayload->GetSize () + ipHeader.GetSerializedSize ());
       NS_LOG_DEBUG ("ReportLastRx ("<<this<<", "<<flowId<<", "<<packetId<<", "<<size<<"); "
                                      << ipHeader << *ipPayload);
       //m_flowMonitor->ReportLastRx (this, flowId, packetId, size);
       this->ReportLastRx (this, flowId, packetId, size);
     }
 }

 void
 QualityMonitor::DropLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload,
                            Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t ifIndex)
 {
 #if 0
   switch (reason)
     {
     case Ipv4L3Protocol::DROP_NO_ROUTE:
       break;

     case Ipv4L3Protocol::DROP_TTL_EXPIRED:
     case Ipv4L3Protocol::DROP_BAD_CHECKSUM:
       Ipv4Address addri = m_ipv4->GetAddress (ifIndex);
       Ipv4Mask maski = m_ipv4->GetNetworkMask (ifIndex);
       Ipv4Address bcast = addri.GetSubnetDirectedBroadcast (maski);
       if (ipHeader.GetDestination () == bcast) // we don't want broadcast packets
         {
           return;
         }
     }
 #endif

   FlowProbeTag fTag;
   bool found = ipPayload->FindFirstMatchingByteTag (fTag);

   if (found)
     {
       FlowId flowId = fTag.GetFlowId ();
       FlowPacketId packetId = fTag.GetPacketId ();

       uint32_t size = (ipPayload->GetSize () + ipHeader.GetSerializedSize ());
       NS_LOG_DEBUG ("Drop ("<<this<<", "<<flowId<<", "<<packetId<<", "<<size<<", " << reason
                             << ", destIp=" << ipHeader.GetDestination () << "); "
                             << "HDR: " << ipHeader << " PKT: " << *ipPayload);

       DropReason myReason;


       switch (reason)
         {
         case Ipv4L3Protocol::DROP_TTL_EXPIRED:
           myReason = DROP_TTL_EXPIRE;
           NS_LOG_DEBUG ("DROP_TTL_EXPIRE");
           break;
         case Ipv4L3Protocol::DROP_NO_ROUTE:
           myReason = DROP_NO_ROUTE;
           NS_LOG_DEBUG ("DROP_NO_ROUTE");
           break;
         case Ipv4L3Protocol::DROP_BAD_CHECKSUM:
           myReason = DROP_BAD_CHECKSUM;
           NS_LOG_DEBUG ("DROP_BAD_CHECKSUM");
           break;
         case Ipv4L3Protocol::DROP_INTERFACE_DOWN:
           myReason = DROP_INTERFACE_DOWN;
           NS_LOG_DEBUG ("DROP_INTERFACE_DOWN");
           break;
         case Ipv4L3Protocol::DROP_ROUTE_ERROR:
           myReason = DROP_ROUTE_ERROR;
           NS_LOG_DEBUG ("DROP_ROUTE_ERROR");
           break;
         case Ipv4L3Protocol::DROP_FRAGMENT_TIMEOUT:
           myReason = DROP_FRAGMENT_TIMEOUT;
           NS_LOG_DEBUG ("DROP_FRAGMENT_TIMEOUT");
           break;

         default:
           myReason = DROP_INVALID_REASON;
           NS_FATAL_ERROR ("Unexpected drop reason code " << reason);
         }

       m_flowMonitor->ReportDrop (this, flowId, packetId, size, myReason);
       this->ReportDrop (this, flowId, packetId, size, myReason);
     }
 }

 void
 QualityMonitor::QueueDropLogger (Ptr<const Packet> ipPayload)
 {
   FlowProbeTag fTag;
   bool tagFound = ipPayload->FindFirstMatchingByteTag (fTag);

   if (!tagFound)
     {
       return;
     }

   FlowId flowId = fTag.GetFlowId ();
   FlowPacketId packetId = fTag.GetPacketId ();
   uint32_t size = fTag.GetPacketSize ();

   NS_LOG_DEBUG ("Drop ("<<this<<", "<<flowId<<", "<<packetId<<", "<<size<<", " << DROP_QUEUE
                         << "); ");

   m_flowMonitor->ReportDrop (this, flowId, packetId, size, DROP_QUEUE);
   this->ReportDrop (this, flowId, packetId, size, DROP_QUEUE);
 }

 void
 QualityMonitor::QueueDiscDropLogger (Ptr<const QueueDiscItem> item)
 {
   FlowProbeTag fTag;
   bool tagFound = item->GetPacket ()->FindFirstMatchingByteTag (fTag);

   if (!tagFound)
     {
       return;
     }

   FlowId flowId = fTag.GetFlowId ();
   FlowPacketId packetId = fTag.GetPacketId ();
   uint32_t size = fTag.GetPacketSize ();

   NS_LOG_DEBUG ("Drop ("<<this<<", "<<flowId<<", "<<packetId<<", "<<size<<", " << DROP_QUEUE_DISC
                         << "); ");

   m_flowMonitor->ReportDrop (this, flowId, packetId, size, DROP_QUEUE_DISC);
   this->ReportDrop (this, flowId, packetId, size, DROP_QUEUE_DISC);
 }

}