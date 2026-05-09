 /* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef FLOW_PROBE_TAG_H
#define FLOW_PROBE_TAG_H

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

#include <iostream>
#include <string>


namespace ns3 {

 class FlowProbeTag : public Tag
 {
 public:
   static TypeId GetTypeId (void);
   virtual TypeId GetInstanceTypeId (void) const;
   virtual uint32_t GetSerializedSize (void) const;
   virtual void Serialize (TagBuffer buf) const;
   virtual void Deserialize (TagBuffer buf);
   virtual void Print (std::ostream &os) const;
   FlowProbeTag ();
   FlowProbeTag (uint32_t flowId, uint32_t packetId, uint32_t packetSize, Ipv4Address src, Ipv4Address dst);
   void SetFlowId (uint32_t flowId);
   void SetPacketId (uint32_t packetId);
   void SetPacketSize (uint32_t packetSize);
   uint32_t GetFlowId (void) const;
   uint32_t GetPacketId (void) const;
   uint32_t GetPacketSize (void) const;
   bool IsSrcDstValid (Ipv4Address src, Ipv4Address dst) const;
 private:
   uint32_t m_flowId;      
   uint32_t m_packetId;    
   uint32_t m_packetSize;  
   Ipv4Address m_src;      
   Ipv4Address m_dst;      
 };

}
#endif