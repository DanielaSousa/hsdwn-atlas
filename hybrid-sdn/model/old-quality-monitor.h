/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef QUALITY_MONITOR_H
#define QUALITY_MONITOR_H

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/flow-monitor-module.h>

#include <ns3/internet-module.h>

#include <ns3/internet-apps-module.h>
#include "ns3/log.h"

#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-channel.h"
//energy
#include "ns3/config-store-module.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"

#include "ns3/ipv4-flow-probe.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/flow-probe.h"
#include "ns3/flow-id-tag.h"

#include "flow-probe-tag.h"

#include <iostream>
#include <string>

#include <vector>
#include <map>

#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/flow-probe.h"
#include "ns3/flow-classifier.h"
#include "ns3/histogram.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"



/**
 * Flow Monitor module is designed in a modular way. It can be extended by subclassing
 * ns3::FlowProbe and ns3::FlowClassifier. Typically, a subclass of ns3::FlowProbe works
 * by listening to the appropriate class Traces, and then uses its own ns3::FlowClassifier
 * subclass to classify the packets passing though each node.
 *
 */
namespace ns3 {
class AttributeValue;
class Ipv4FlowClassifier;
class Node;

class QualityMonitor : public Object, FlowProbe
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
    int Install(NodeContainer nodes);
    int Install(Ptr<ns3::Node> node);

    //Ptr<FlowMonitor> Install (NodeContainer nodes);
    /**
     * @brief trace function on radiotapheader. If troughput is 0, then the packet gets droped (is nor suppost to reach this device)
     *
     * @param packet the packet received by the mac layer (layer 2)
     * @param channelFreqMhz
     * @param txVector
     * @param aMpdu
     * @param signalNoise
     * @param staId
     */
    void MonitorSniffRx (std::string context, Ptr<const Packet> packet,
                     uint16_t channelFreqMhz,
                     WifiTxVector txVector,
                     MpduInfo aMpdu,
                     SignalNoiseDbm signalNoise,
                     uint16_t staId);

    // -------- from flow monitor--------
    struct FlowStats{
        Time     timeFirstTxPacket;
        Time     timeFirstRxPacket;
        Time     timeLastTxPacket;
        Time     timeLastRxPacket;
        Time     delaySum; // delayCount == rxPackets
        Time     jitterSum; // jitterCount == rxPackets - 1
        Time     lastDelay;

        uint64_t txBytes;
        uint64_t rxBytes;
        uint32_t txPackets;
        uint32_t rxPackets;

        uint32_t lostPackets;

        uint32_t timesForwarded;

        Histogram delayHistogram;
        Histogram jitterHistogram;
        Histogram packetSizeHistogram;

        std::vector<uint32_t> packetsDropped; // packetsDropped[reasonCode] => number of dropped packets

        std::vector<uint64_t> bytesDropped; // bytesDropped[reasonCode] => number of dropped bytes
        Histogram flowInterruptionsHistogram;
    };

   // --- basic methods ---
   static TypeId GetTypeId ();
   virtual TypeId GetInstanceTypeId () const;


   void AddFlowClassifier (Ptr<FlowClassifier> classifier);

   void Start (const Time &time);
   void Stop (const Time &time);
   void StartRightNow ();
   void StopRightNow ();

   // --- methods to be used by the FlowMonitorProbe's only ---
   void AddProbe (Ptr<FlowProbe> probe);

   void ReportFirstTx (Ptr<FlowProbe> probe, FlowId flowId, FlowPacketId packetId, uint32_t packetSize);
   void ReportForwarding (Ptr<FlowProbe> probe, FlowId flowId, FlowPacketId packetId, uint32_t packetSize);
   void ReportLastRx (Ptr<FlowProbe> probe, FlowId flowId, FlowPacketId packetId, uint32_t packetSize);
   void ReportDrop (Ptr<FlowProbe> probe, FlowId flowId, FlowPacketId packetId,
                    uint32_t packetSize, uint32_t reasonCode);

   //void CheckForLostPackets ();

   void CheckForLostPackets (Time maxDelay);

   // --- methods to get the results ---

   typedef std::map<FlowId, FlowStats> FlowStatsContainer;
   typedef std::map<FlowId, FlowStats>::iterator FlowStatsContainerI;
   typedef std::map<FlowId, FlowStats>::const_iterator FlowStatsContainerCI;
   typedef std::vector< Ptr<FlowProbe> > FlowProbeContainer;
   typedef std::vector< Ptr<FlowProbe> >::iterator FlowProbeContainerI;
   typedef std::vector< Ptr<FlowProbe> >::const_iterator FlowProbeContainerCI;

   const FlowStatsContainer& GetFlowStats () const;

   const FlowProbeContainer& GetAllProbes () const;

   void SerializeToXmlStream (std::ostream &os, uint16_t indent, bool enableHistograms, bool enableProbes);

   std::string SerializeToXmlString (uint16_t indent, bool enableHistograms, bool enableProbes);

   void SerializeToXmlFile (std::string fileName, bool enableHistograms, bool enableProbes);

   //ipv4flowprobe
   enum DropReason
   {
     DROP_NO_ROUTE = 0,

     DROP_TTL_EXPIRE,

     DROP_BAD_CHECKSUM,

     DROP_QUEUE,

     DROP_QUEUE_DISC,

     DROP_INTERFACE_DOWN,
     DROP_ROUTE_ERROR,
     DROP_FRAGMENT_TIMEOUT,
     DROP_INVALID_REASON,
   };

protected:
    virtual void NotifyConstructionCompleted ();
    virtual void DoDispose (void);

private:
    //from flowmonitorhelper
    ObjectFactory m_monitorFactory;
    Ptr<FlowMonitor> m_flowMonitor;
    Ptr<FlowClassifier> m_flowClassifier4;



    // //ipv4flowProbe
    // void SendOutgoingLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);
    // void ForwardLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);
    // void ForwardUpLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);
    // void DropLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload,
    //                     Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t ifIndex);
    // Ptr<Ipv4FlowClassifier> m_classifier;
    // Ptr<Ipv4L3Protocol> m_ipv4;

    // TODO
    // map where we store the packet and the time that is been received, if after 1min is not received in the upper layers, reset

    // FLOW-MONITOR
    struct TrackedPacket
   {
     Time firstSeenTime;
     Time lastSeenTime;
     uint32_t timesForwarded;
   };

   FlowStatsContainer m_flowStats;

   typedef std::map< std::pair<FlowId, FlowPacketId>, TrackedPacket> TrackedPacketMap;
   TrackedPacketMap m_trackedPackets;
   Time m_maxPerHopDelay;
   FlowProbeContainer m_flowProbes;

   // note: this is needed only for serialization
   std::list<Ptr<FlowClassifier> > m_classifiers;

   EventId m_startEvent;
   EventId m_stopEvent;
   bool m_enabled;
   double m_delayBinWidth;
   double m_jitterBinWidth;
   double m_packetSizeBinWidth;
   double m_flowInterruptionsBinWidth;
   Time m_flowInterruptionsMinTime;

   FlowStats& GetStatsForFlow (FlowId flowId);

   void PeriodicCheckForLostPackets ();

   //ipv4flowprobe
   void SendOutgoingLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);
   void ForwardLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);
   void ForwardUpLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload, uint32_t interface);
   void DropLogger (const Ipv4Header &ipHeader, Ptr<const Packet> ipPayload,
                    Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t ifIndex);
   void QueueDropLogger (Ptr<const Packet> ipPayload);
   void QueueDiscDropLogger (Ptr<const QueueDiscItem> item);

   Ptr<Ipv4FlowClassifier> m_classifier;
   Ptr<Ipv4L3Protocol> m_ipv4;



};





}

#endif // QUALITY_MONITOR_H