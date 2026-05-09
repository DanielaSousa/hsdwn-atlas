// FlowProbeTag class implementation //
 
#include "flow-probe-tag.h"


namespace ns3 {

TypeId
FlowProbeTag::GetTypeId (void)
{
static TypeId tid = TypeId ("ns3::FlowProbeTag")
    .SetParent<Tag> ()
    .SetGroupName ("FlowMonitor")
    .AddConstructor<FlowProbeTag> ();
return tid;
}

TypeId
FlowProbeTag::GetInstanceTypeId (void) const
{
return GetTypeId ();
}

uint32_t
FlowProbeTag::GetSerializedSize (void) const
{
return 4 + 4 + 4 + 8;
}

void
FlowProbeTag::Serialize (TagBuffer buf) const
{
buf.WriteU32 (m_flowId);
buf.WriteU32 (m_packetId);
buf.WriteU32 (m_packetSize);

uint8_t tBuf[4];
m_src.Serialize (tBuf);
buf.Write (tBuf, 4);
m_dst.Serialize (tBuf);
buf.Write (tBuf, 4);
}

void
FlowProbeTag::Deserialize (TagBuffer buf)
{
m_flowId = buf.ReadU32 ();
m_packetId = buf.ReadU32 ();
m_packetSize = buf.ReadU32 ();

uint8_t tBuf[4];
buf.Read (tBuf, 4);
m_src = Ipv4Address::Deserialize (tBuf);
buf.Read (tBuf, 4);
m_dst = Ipv4Address::Deserialize (tBuf);
}

void
FlowProbeTag::Print (std::ostream &os) const
{
os << "FlowId=" << m_flowId;
os << " PacketId=" << m_packetId;
os << " PacketSize=" << m_packetSize;
}

FlowProbeTag::FlowProbeTag ()
: Tag ()
{
}

FlowProbeTag::FlowProbeTag (uint32_t flowId, uint32_t packetId, uint32_t packetSize, Ipv4Address src, Ipv4Address dst)
: Tag (), m_flowId (flowId), m_packetId (packetId), m_packetSize (packetSize), m_src (src), m_dst (dst)
{
}

void
FlowProbeTag::SetFlowId (uint32_t id)
{
m_flowId = id;
}

void
FlowProbeTag::SetPacketId (uint32_t id)
{
m_packetId = id;
}

void
FlowProbeTag::SetPacketSize (uint32_t size)
{
m_packetSize = size;
}

uint32_t
FlowProbeTag::GetFlowId (void) const
{
return m_flowId;
}

uint32_t
FlowProbeTag::GetPacketId (void) const
{
return m_packetId;
}

uint32_t
FlowProbeTag::GetPacketSize (void) const
{
return m_packetSize;
}

bool
FlowProbeTag::IsSrcDstValid (Ipv4Address src, Ipv4Address dst) const
{
return ((m_src == src) && (m_dst == dst));
}

}