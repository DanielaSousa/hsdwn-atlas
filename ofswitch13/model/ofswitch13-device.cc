/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Jerez Chaves <ljerezchaves@gmail.com>
 */

#include "ofswitch13-device.h"

#include "ofswitch13-port.h"

#include <ns3/arp-header.h>
#include <ns3/arp-l3-protocol.h>
#include <ns3/ethernet-header.h>
#include <ns3/ethernet-trailer.h>
#include <ns3/llc-snap-header.h>
#include <ns3/object-vector.h>
#include <ns3/wifi-net-device.h>

#include <stdio.h>
#include <time.h>

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                                                      \
    if (m_dpId)                                                                                    \
    {                                                                                              \
        std::clog << "[dp " << m_dpId << "] ";                                                     \
    }

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OFSwitch13Device");
NS_OBJECT_ENSURE_REGISTERED(OFSwitch13Device);

// Initializing OFSwitch13Device static members.
uint64_t OFSwitch13Device::m_globalDpId = 0;
uint64_t OFSwitch13Device::m_globalPktId = 0;
OFSwitch13Device::DpIdDevMap_t OFSwitch13Device::m_globalSwitchMap;

/********** Public methods **********/
OFSwitch13Device::OFSwitch13Device()
    : m_dpId(0),
      m_datapath(nullptr),
      m_cpuConsumed(0),
      m_cpuTokens(0),
      m_cFlowMod(0),
      m_cGroupMod(0),
      m_cMeterMod(0),
      m_cPacketIn(0),
      m_cPacketOut(0)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("OpenFlow version: " << OFP_VERSION);

    m_dpId = ++m_globalDpId;
    NS_LOG_DEBUG("New datapath ID " << m_dpId);
    OFSwitch13Device::RegisterDatapath(m_dpId, Ptr<OFSwitch13Device>(this));
}

OFSwitch13Device::~OFSwitch13Device()
{
    NS_LOG_FUNCTION(this);
}

TypeId
OFSwitch13Device::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OFSwitch13Device")
            .SetParent<Object>()
            .SetGroupName("OFSwitch13")
            .AddConstructor<OFSwitch13Device>()
            .AddAttribute("CpuCapacity",
                          "CPU processing capacity (in terms of throughput).",
                          DataRateValue(DataRate("100Gb/s")),
                          MakeDataRateAccessor(&OFSwitch13Device::m_cpuCapacity),
                          MakeDataRateChecker())
            .AddAttribute("DatapathId",
                          "The unique identification of this OpenFlow switch.",
                          TypeId::ATTR_GET,
                          UintegerValue(0),
                          MakeUintegerAccessor(&OFSwitch13Device::m_dpId),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("FlowTableSize",
                          "The maximum number of entries allowed on each flow table.",
                          UintegerValue(FLOW_TABLE_MAX_ENTRIES),
                          MakeUintegerAccessor(&OFSwitch13Device::SetDftFlowTableSize,
                                               &OFSwitch13Device::GetDftFlowTableSize),
                          MakeUintegerChecker<uint32_t>(0, FLOW_TABLE_MAX_ENTRIES))
            .AddAttribute("GroupTableSize",
                          "The maximum number of entries allowed on group table.",
                          UintegerValue(GROUP_TABLE_MAX_ENTRIES),
                          MakeUintegerAccessor(&OFSwitch13Device::SetGroupTableSize,
                                               &OFSwitch13Device::GetGroupTableSize),
                          MakeUintegerChecker<uint32_t>(0, GROUP_TABLE_MAX_ENTRIES))
            .AddAttribute("MeterTableSize",
                          "The maximum number of entries allowed on meter table.",
                          UintegerValue(METER_TABLE_MAX_ENTRIES),
                          MakeUintegerAccessor(&OFSwitch13Device::SetMeterTableSize,
                                               &OFSwitch13Device::GetMeterTableSize),
                          MakeUintegerChecker<uint32_t>(0, METER_TABLE_MAX_ENTRIES))
            .AddAttribute("PipelineTables",
                          "The number of pipeline flow tables.",
                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                          UintegerValue(64),
                          MakeUintegerAccessor(&OFSwitch13Device::m_numPipeTabs),
                          MakeUintegerChecker<uint32_t>(1, (OFPTT_MAX + 1)))
            .AddAttribute("PortList",
                          "The list of ports associated to this switch.",
                          ObjectVectorValue(),
                          MakeObjectVectorAccessor(&OFSwitch13Device::m_ports),
                          MakeObjectVectorChecker<OFSwitch13Port>())
            .AddAttribute("TcamDelay",
                          "Average time to perform a TCAM operation in pipeline.",
                          TimeValue(MicroSeconds(20)),
                          MakeTimeAccessor(&OFSwitch13Device::m_tcamDelay),
                          MakeTimeChecker(Time(0)))
            .AddAttribute("TimeoutInterval",
                          "The interval between timeout operations on datapath.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&OFSwitch13Device::m_timeout),
                          MakeTimeChecker(MilliSeconds(1), MilliSeconds(1000)))

            .AddTraceSource("BufferExpire",
                            "Trace source indicating an expired packet in buffer.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_bufferExpireTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("BufferRetrieve",
                            "Trace source indicating a packet retrieved from buffer.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_bufferRetrieveTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("BufferSave",
                            "Trace source indicating a packet saved into buffer.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_bufferSaveTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("MeterDrop",
                            "Trace source indicating a packet dropped by meter band.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_meterDropTrace),
                            "ns3::OFSwitch13Device::MeterDropTracedCallback")
            .AddTraceSource("OverloadDrop",
                            "Trace source indicating a packet dropped by CPU "
                            "overloaded processing capacity.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_loadDropTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TableDrop",
                            "Trace source indicating an unmatched packet dropped by "
                            "a flow table without a table-miss entry.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_tableDropTrace),
                            "ns3::OFSwitch13Device::TableDropTracedCallback")
            .AddTraceSource("PipelinePacket",
                            "Trace source indicating a packet sent to pipeline.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_pipePacketTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("DatapathTimeout",
                            "Trace source indicating a datapath timeout operation.",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_datapathTimeoutTrace),
                            "ns3::OFSwitch13Device::DeviceTracedCallback")

            .AddTraceSource("CpuLoad",
                            "Traced value indicating the avg CPU processing load"
                            " (periodically updated on datapath timeout operation).",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_cpuLoad),
                            "ns3::TracedValueCallback::DataRate")
            .AddTraceSource("GroupEntries",
                            "Traced value indicating the number of group entries"
                            " (periodically updated on datapath timeout operation).",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_groupEntries),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("MeterEntries",
                            "Traced value indicating the number of meter entries"
                            " (periodically updated on datapath timeout operation).",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_meterEntries),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("PipelineDelay",
                            "Traced value indicating the avg pipeline lookup delay"
                            " (periodically updated on datapath timeout operation).",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_pipeDelay),
                            "ns3::TracedValueCallback::Time")
            .AddTraceSource("SumFlowEntries",
                            "Traced value indicating the total number of flow entries"
                            " (periodically updated on datapath timeout operation).",
                            MakeTraceSourceAccessor(&OFSwitch13Device::m_sumFlowEntries),
                            "ns3::TracedValueCallback::Uint32");
    return tid;
}

uint64_t
OFSwitch13Device::GetDatapathId() const
{
    return m_dpId;
}

uint64_t
OFSwitch13Device::GetDpId() const
{
    return m_dpId;
}

uint64_t
OFSwitch13Device::GetFlowModCounter() const
{
    return m_cFlowMod;
}

uint64_t
OFSwitch13Device::GetGroupModCounter() const
{
    return m_cGroupMod;
}

uint64_t
OFSwitch13Device::GetMeterModCounter() const
{
    return m_cMeterMod;
}

uint64_t
OFSwitch13Device::GetPacketInCounter() const
{
    return m_cPacketIn;
}

uint64_t
OFSwitch13Device::GetPacketOutCounter() const
{
    return m_cPacketOut;
}

uint32_t
OFSwitch13Device::GetBufferEntries() const
{
    return m_bufferPkts.size();
}

uint32_t
OFSwitch13Device::GetBufferSize() const
{
    return m_bufferSize;
}

double
OFSwitch13Device::GetBufferUsage() const
{
    if (GetBufferSize() == 0)
    {
        return 0.0;
    }
    return static_cast<double>(GetBufferEntries()) / static_cast<double>(GetBufferSize());
}

DataRate
OFSwitch13Device::GetCpuCapacity() const
{
    return m_cpuCapacity;
}

DataRate
OFSwitch13Device::GetCpuLoad() const
{
    return m_cpuLoad;
}

double
OFSwitch13Device::GetCpuUsage() const
{
    if (GetCpuCapacity().GetBitRate() == 0)
    {
        return 0.0;
    }
    return static_cast<double>(GetCpuLoad().GetBitRate()) /
           static_cast<double>(GetCpuCapacity().GetBitRate());
}

Time
OFSwitch13Device::GetDatapathTimeout() const
{
    return m_timeout;
}

uint32_t
OFSwitch13Device::GetDftFlowTableSize() const
{
    return m_flowTabSize;
}

uint32_t
OFSwitch13Device::GetFlowTableEntries(uint8_t tableId) const
{
    NS_ASSERT_MSG(m_datapath, "No datapath defined yet.");
    NS_ASSERT_MSG(tableId < GetNPipelineTables(), "Invalid table ID.");
    return m_datapath->pipeline->tables[tableId]->stats->active_count;
}

uint32_t
OFSwitch13Device::GetFlowTableSize(uint8_t tableId) const
{
    NS_ASSERT_MSG(m_datapath, "No datapath defined yet.");
    NS_ASSERT_MSG(tableId < GetNPipelineTables(), "Invalid table ID.");
    return m_datapath->pipeline->tables[tableId]->features->max_entries;
}

double
OFSwitch13Device::GetFlowTableUsage(uint8_t tableId) const
{
    if (GetFlowTableSize(tableId) == 0)
    {
        return 0.0;
    }
    return static_cast<double>(GetFlowTableEntries(tableId)) /
           static_cast<double>(GetFlowTableSize(tableId));
}

uint32_t
OFSwitch13Device::GetGroupTableEntries() const
{
    NS_ASSERT_MSG(m_datapath, "No datapath defined yet.");
    return m_datapath->groups->entries_num;
}

uint32_t
OFSwitch13Device::GetGroupTableSize() const
{
    return m_groupTabSize;
}

double
OFSwitch13Device::GetGroupTableUsage() const
{
    if (GetGroupTableSize() == 0)
    {
        return 0.0;
    }
    return static_cast<double>(GetGroupTableEntries()) / static_cast<double>(GetGroupTableSize());
}

uint32_t
OFSwitch13Device::GetMeterTableEntries() const
{
    NS_ASSERT_MSG(m_datapath, "No datapath defined yet.");
    return m_datapath->meters->entries_num;
}

uint32_t
OFSwitch13Device::GetMeterTableSize() const
{
    return m_meterTabSize;
}

double
OFSwitch13Device::GetMeterTableUsage() const
{
    if (GetMeterTableSize() == 0)
    {
        return 0.0;
    }
    return static_cast<double>(GetMeterTableEntries()) / static_cast<double>(GetMeterTableSize());
}

uint32_t
OFSwitch13Device::GetNControllers() const
{
    return m_controllers.size();
}

uint32_t
OFSwitch13Device::GetNPipelineTables() const
{
    return m_numPipeTabs;
}

uint32_t
OFSwitch13Device::GetNSwitchPorts() const
{
    return m_ports.size();
}

Time
OFSwitch13Device::GetPipelineDelay() const
{
    return m_pipeDelay;
}

uint32_t
OFSwitch13Device::GetSumFlowEntries() const
{
    uint32_t flowEntries = 0;
    for (size_t i = 0; i < GetNPipelineTables(); i++)
    {
        flowEntries += GetFlowTableEntries(i);
    }
    return flowEntries;
}

struct datapath*
OFSwitch13Device::GetDatapathStruct()
{
    return m_datapath;
}

Ptr<OFSwitch13Port>
OFSwitch13Device::AddSwitchPort(Ptr<NetDevice> portDevice)
{
    NS_LOG_FUNCTION(this << portDevice);

    // NS_LOG_INFO ("Adding port addr " << portDevice->GetAddress ());
    NS_ABORT_MSG_IF(GetNSwitchPorts() >= DP_MAX_PORTS, "No more ports allowed.");

    // Create the OpenFlow port for this device.
    Ptr<OFSwitch13Port> ofPort;
    ofPort = CreateObject<OFSwitch13Port>(m_datapath, portDevice, this);

    // Save port in port list (assert port no and vector index).
    m_ports.emplace_back(ofPort);
    NS_ASSERT((m_ports.size() == ofPort->GetPortNo()) && (m_ports.size() == m_datapath->ports_num));

    std::cout << "Adding port addr " << portDevice->GetAddress() << "Number " << ofPort->GetPortNo()
              << std::endl;

    std::cout << "-----------------------" << std::endl;
    std::cout << "Node Id: " << portDevice->GetNode()->GetId() << " , dpId: " << m_datapath->id
              << " , ipv4: " << portDevice->GetAddress() << std::endl;
    std::cout << "-----------------------" << std::endl;

    // TODO connect callback
    Ptr<CsmaNetDevice> csmaDev = portDevice->GetObject<CsmaNetDevice>();
    Ptr<WifiNetDevice> wifiDev = portDevice->GetObject<WifiNetDevice>();
    if (csmaDev)
    {
        // TODO
    }
    else if (wifiDev)
    {
        m_workflowCallback.push_back(MakeCallback(&WifiNetDevice::WorkflowMonitor, wifiDev));
    }

    return ofPort;
}

Ptr<OFSwitch13Port>
OFSwitch13Device::GetSwitchPort(uint32_t no) const
{
    NS_LOG_FUNCTION(this << no);
    NS_LOG_DEBUG("OFSwitch13Device::GetSwitchPort " << no << " " << m_ports.size());

    // Assert port no (starts at 1).
    NS_ASSERT_MSG(no > 0 && no <= m_ports.size(), "Port is out of range.");
    return m_ports.at(no - 1);
}

void
OFSwitch13Device::ReceiveFromSwitchPort(Ptr<Packet> packet,
                                        uint32_t portNo,
                                        uint16_t protocol,
                                        const Address& from,
                                        const Address& to,
                                        uint64_t tunnelId)
{
    // ns3::packet
    NS_LOG_FUNCTION(this << packet << portNo << tunnelId);

    // Check the packet for conformance to CPU processing capacity.
    uint32_t pktSizeBits = packet->GetSize() * 8;
    if (m_cpuTokens < pktSizeBits)
    {
        // Packet will be dropped. Increase counter and fire drop trace source.
        NS_LOG_DEBUG("Drop packet due to CPU overloaded capacity.");
        m_loadDropTrace(packet);
        return;
    }

    // Consume tokens, fire trace source and schedule the packet to the pipeline.
    m_cpuTokens -= pktSizeBits;
    m_cpuConsumed += pktSizeBits;
    m_pipePacketTrace(packet);
    m_pipeDelay = Seconds(0); // to be the same as OSI layers
    Simulator::Schedule(m_pipeDelay,
                        &OFSwitch13Device::SendToPipeline,
                        this,
                        packet,
                        portNo,
                        protocol,
                        from,
                        to,
                        tunnelId);
}

void
OFSwitch13Device::StartControllerConnection(Address ctrlAddr)
{
    NS_LOG_FUNCTION(this << ctrlAddr);

    NS_ASSERT(!ctrlAddr.IsInvalid());
    NS_ASSERT_MSG(InetSocketAddress::IsMatchingType(ctrlAddr),
                  "Invalid address type (only IPv4 supported by now).");
    NS_ASSERT_MSG(!GetRemoteController(ctrlAddr), "Controller address already in use.");
    NS_LOG_INFO(" --- start --- " << ctrlAddr);
    // std::cout << " --- start --- " << ctrlAddr << std::endl;

    // Start a TCP connection to this target controller.
    int error = 0;
    TypeId tcpFact = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> ctrlSocket = Socket::CreateSocket(GetObject<Node>(), tcpFact);
    ctrlSocket->SetAttribute("SegmentSize", UintegerValue(8900));

    error = ctrlSocket->Bind();
    if (error)
    {
        NS_LOG_ERROR("Error binding socket " << error);
        return;
    }

    error = ctrlSocket->Connect(InetSocketAddress::ConvertFrom(ctrlAddr));
    if (error)
    {
        NS_LOG_ERROR("Error connecting socket " << error);
        return;
    }

    ctrlSocket->SetConnectCallback(MakeCallback(&OFSwitch13Device::SocketCtrlSucceeded, this),
                                   MakeCallback(&OFSwitch13Device::SocketCtrlFailed, this));

    // Create a RemoteController object for this controller and save it.
    Ptr<RemoteController> remoteCtrl = Create<RemoteController>();
    remoteCtrl->m_address = ctrlAddr;
    remoteCtrl->m_socket = ctrlSocket;
    m_controllers.emplace_back(remoteCtrl);
}

// ofsoftswitch13 overriding and callback functions.
void
OFSwitch13Device::SendPacketToController(struct pipeline* pl,
                                         struct packet* pkt,
                                         uint8_t tableId,
                                         uint8_t reason)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pl->dp->id);
    int error =
        dev->SendPacketInMessage(pkt, tableId, reason, dev->m_datapath->config.miss_send_len);
    NS_ASSERT_MSG(error == 0, "ERROR " << error);
}

int
OFSwitch13Device::SendOpenflowBufferToRemote(struct ofpbuf* buffer, struct remote* remote)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(remote->dp->id);
    Ptr<Packet> packet = PacketFromBuffer(buffer);
    Ptr<RemoteController> remoteCtrl = dev->GetRemoteController(remote);

    ofpbuf_delete(buffer);
    return dev->SendToController(packet, remoteCtrl);
}

void
OFSwitch13Device::DpActionsOutputPort(struct packet* pkt,
                                      uint32_t outPort,
                                      uint32_t outQueue,
                                      uint16_t maxLength,
                                      uint64_t cookie)
{
    // nota como é static nao dá para fazer o append e por isso dá erro no ns_log_info

    // NS_LOG_INFO( "OFSwitch13Device::DpActionsOutputPort : outPort " << outPort );
    // std::cout << "OFSwitch13Device::DpActionsOutputPort :  " << pkt->ns3_uid << " outPort "
    //           << outPort << std::endl;
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    switch (outPort)
    {
    case (OFPP_TABLE): {
        if (pkt->packet_out)
        {
            // Makes sure packet cannot be resubmit to pipeline again setting
            // packet_out to false. Also, pipeline_process_packet takes
            // ownership of the packet, we need a copy.
            struct packet* pkt_copy = packet_clone(pkt);
            pkt_copy->packet_out = false;
            pipeline_process_packet(pkt_copy->dp->pipeline, pkt_copy);
        }
        break;
    }
    case (OFPP_IN_PORT): {
        // NS_LOG_INFO( "OFPP_IN_PORT SendToSwitchPort " );
        dev->SendToSwitchPort(pkt, pkt->in_port, outQueue);
        // std::cout << "\n\nDpActionsOutputPort - OFPP_IN_PORT (buffer size):" << pkt->buffer->size
        //           << std::endl;
        break;
    }
    case (OFPP_ANY): {
        // NS_LOG_INFO( "OFPP_any SendToSwitchPort " );
        dev->SendToSwitchPort(pkt, pkt->out_port, outQueue);
        break;
    }
    case (OFPP_CONTROLLER): {
        // std::cout << "\n\nDpActionsOutputPort - OFPP_CONTROLLER (buffer size):" <<
        // pkt->buffer->size
        //           << std::endl;
        dev->SendPacketInMessage(pkt,
                                 pkt->table_id,
                                 pkt->handle_std->table_miss ? OFPR_NO_MATCH : OFPR_ACTION,
                                 maxLength,
                                 cookie);

        break;
    }
    case (OFPP_FLOOD):
        /*Optional:FLOOD:Represents flooding using the normal pipeline of the switch (see 5.1).
        Canbe used only as an output port, in general will send the packet out all standard ports,
        but not tothe ingress port, nor ports that are inOFPPS_BLOCKEDstate.  The switch may also
        use the packetVLAN ID to select which ports to flood*/
    case (OFPP_ALL): {
        struct sw_port* p;
        LIST_FOR_EACH(p, struct sw_port, node, &pkt->dp->port_list)
        {
            // TODO
            // if ((p->stats->port_no == pkt->in_port)
            //     || (outPort == OFPP_FLOOD && p->conf->config & OFPPC_NO_FWD))
            //   {
            //     std::cout << "continue " << std::endl;
            //     continue;
            //   }
            // NS_LOG_INFO( "OFPP_FLOOD SendToSwitchPort " << p->stats->port_no );
            dev->SendToSwitchPort(pkt, p->stats->port_no);
        }
        break;
    }
    case (OFPP_NORMAL):
        /*Optional:NORMAL:Represents the traditional non-OpenFlow pipeline of the switch (see 5.1).
        Can be used only as an output port and processes the packet using the normal pipeline.
        If theswitch cannot forward packets from the OpenFlow pipeline to the normal pipeline, it
        must indicatethat it does not support this action.*/
        /*normal Subjects the packet to the device’s normal L2/L3 process‐
                       ing. This action  is  not  implemented  by  all  OpenFlow
                       switches, and each switch implements it differently.*/

        // NS_LOG_INFO( "OFPP_NORMAL send to legacy L3/L2 switchng " );
    case (OFPP_LOCAL):
        /*Optional:LOCAL:Represents the switch’s local networking stack and its management stack.
        Can be used as an ingress port or as an output port.  The local port enables remote entities
        tointeract with the switch and its network services via the OpenFlow network,  rather than
        via aseparate control network.  With a suitable set of default flow entries it can be used
        to implementan in-band controller connection.*/
        /*from ovs:
        local  Outputs the packet on the ``local port’’ that corresponds
              to  the  network  device  that  has  the same name as the
              bridge, unless the packet was received on the local port.
              OpenFlow  switch implementations are not required to have
              a local port, but Open vSwitch bridges always do.*/
        {
            // NS_LOG_INFO( "OFPP_LOCAL SendToSwitchPort " );
            if (pkt->in_port != outPort)
            {
                // NS_LOG_INFO( "OFPP_LOCAL/NORMAL SendToSwitchPort " );
                // dev->SendToSwitchPort (pkt, outPort, outQueue); //reserved for loopback
                dev->SendToReservedPort(pkt);
            }
            break;
        }
    default: {
        // if (pkt->in_port != outPort)
        //{
        // NS_LOG_INFO( "OFPP_ANY SendToSwitchPort " );
        // std::cout << "\n\nDpActionsOutputPort - DEFAULT " << pkt->ns3_uid
        //           << " (buffer size):" << pkt->buffer->size << std::endl;

        dev->SendToSwitchPort(pkt, outPort, outQueue); // reserved for loopback

        // }
    }
    }
}

void
OFSwitch13Device::MeterCreatedCallback(struct meter_entry* entry)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(entry->dp->id);
    dev->NotifyMeterEntryCreated(entry);
}

void
OFSwitch13Device::MeterDropCallback(struct packet* pkt, struct meter_entry* entry)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    dev->NotifyPacketDroppedByMeter(pkt, entry);
}

void
OFSwitch13Device::TableDropCallback(struct packet* pkt, struct flow_table* table)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    dev->NotifyPacketDroppedByTable(pkt, table);
}

void
OFSwitch13Device::PacketCloneCallback(struct packet* pkt, struct packet* clone)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    dev->NotifyPacketCloned(pkt, clone);
}

void
OFSwitch13Device::PacketDestroyCallback(struct packet* pkt)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    dev->NotifyPacketDestroyed(pkt);
}

void
OFSwitch13Device::BufferSaveCallback(struct packet* pkt, time_t timeout)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    dev->BufferPacketSave(pkt->ns3_uid, timeout);
}

void
OFSwitch13Device::BufferRetrieveCallback(struct packet* pkt)
{
    Ptr<OFSwitch13Device> dev = OFSwitch13Device::GetDevice(pkt->dp->id);
    dev->BufferPacketRetrieve(pkt->ns3_uid);
}

Ptr<OFSwitch13Device>
OFSwitch13Device::GetDevice(uint64_t id)
{
    auto it = OFSwitch13Device::m_globalSwitchMap.find(id);
    if (it != OFSwitch13Device::m_globalSwitchMap.end())
    {
        return it->second;
    }
    NS_ABORT_MSG("Error when retrieving datapath.");
}

/********** Protected methods **********/
void
OFSwitch13Device::DoDispose()
{
    NS_LOG_FUNCTION(this);

    for (auto& port : m_ports)
    {
        port->Dispose();
        port = nullptr;
    }
    m_ports.clear();
    m_bufferPkts.clear();

    for (auto& ctrl : m_controllers)
    {
        free(ctrl->m_remote);
    }
    m_controllers.clear();

    dp_buffers_destroy(m_datapath->buffers);
    pipeline_destroy(m_datapath->pipeline);
    group_table_destroy(m_datapath->groups);
    meter_table_destroy(m_datapath->meters);

    free(m_datapath->mfr_desc);
    free(m_datapath->hw_desc);
    free(m_datapath->sw_desc);
    free(m_datapath->dp_desc);
    free(m_datapath->serial_num);
    free(m_datapath);

    OFSwitch13Device::UnregisterDatapath(m_dpId);

    Object::DoDispose();
}

void
OFSwitch13Device::NotifyConstructionCompleted()
{
    NS_LOG_FUNCTION(this);

    // Create the datapath.
    m_datapath = DatapathNew();

    // Set the attribute values again so it can now update the dapatah structs.
    SetDftFlowTableSize(GetDftFlowTableSize());
    SetGroupTableSize(GetGroupTableSize());
    SetMeterTableSize(GetMeterTableSize());

    // Execute the first datapath timeout.
    DatapathTimeout(m_datapath);
    // set Experiemnter with port info first time
    // GraphPortsUpdate (m_datapath);

    // Chain up.
    Object::NotifyConstructionCompleted();
}

/********** Private methods **********/
struct datapath*
OFSwitch13Device::DatapathNew()
{
    NS_LOG_FUNCTION(this);

    struct datapath* dp = (struct datapath*)xmalloc(sizeof(struct datapath));

    dp->mfr_desc = (char*)xmalloc(DESC_STR_LEN);
    dp->hw_desc = (char*)xmalloc(DESC_STR_LEN);
    dp->sw_desc = (char*)xmalloc(DESC_STR_LEN);
    dp->dp_desc = (char*)xmalloc(DESC_STR_LEN);
    dp->serial_num = (char*)xmalloc(DESC_STR_LEN);
    strncpy(dp->mfr_desc, "The ns-3 team", DESC_STR_LEN);
    strncpy(dp->hw_desc, "N/A", DESC_STR_LEN);
    strncpy(dp->sw_desc, "The ns-3 OFSwitch13 module", DESC_STR_LEN);
    strncpy(dp->dp_desc, "Using BOFUSS (from CPqD)", DESC_STR_LEN);
    strncpy(dp->serial_num, "3.1.0", DESC_STR_LEN);

    dp->id = m_dpId;
    dp->last_timeout = time_now();
    m_lastTimeout = Simulator::Now();
    list_init(&dp->remotes);

    // unused
    dp->generation_id = -1;

    memset(dp->ports, 0x00, sizeof(dp->ports));
    dp->local_port = nullptr;

    // Set the number of flow tables from the PipelineTables attribute.
    dp->pipeline_num_tables = GetNPipelineTables();

    dp->buffers = dp_buffers_create(dp);
    dp->pipeline = pipeline_create(dp);
    dp->groups = group_table_create(dp);
    dp->meters = meter_table_create(dp);

    m_bufferSize = dp_buffers_size(dp->buffers);

    list_init(&dp->port_list);
    dp->ports_num = 0;
    dp->max_queues = PORT_MAX_QUEUES;
    dp->exp = nullptr;

    dp->config.flags = OFPC_FRAG_NORMAL;                  // IP fragments with no special handling
    dp->config.miss_send_len = OFP_DEFAULT_MISS_SEND_LEN; // 128 bytes

    // BOFUSS callbacks
    dp->pkt_clone_cb = &OFSwitch13Device::PacketCloneCallback;
    dp->pkt_destroy_cb = &OFSwitch13Device::PacketDestroyCallback;
    dp->buff_save_cb = &OFSwitch13Device::BufferSaveCallback;
    dp->buff_retrieve_cb = &OFSwitch13Device::BufferRetrieveCallback;
    dp->meter_drop_cb = &OFSwitch13Device::MeterDropCallback;
    dp->miss_drop_cb = &OFSwitch13Device::TableDropCallback;
    dp->meter_created_cb = &OFSwitch13Device::MeterCreatedCallback;

    return dp;
}

void
OFSwitch13Device::SetFlowTableSize(uint8_t tableId, uint32_t value)
{
    NS_LOG_FUNCTION(this << tableId << value);

    NS_ASSERT_MSG(m_datapath, "No datapath defined yet.");
    NS_ABORT_MSG_IF(GetFlowTableEntries(tableId) > value, "Can't reduce table size to this value.");
    m_datapath->pipeline->tables[tableId]->features->max_entries = value;
}

void
OFSwitch13Device::SetDftFlowTableSize(uint32_t value)
{
    NS_LOG_FUNCTION(this << value);

    m_flowTabSize = value;
    if (m_datapath)
    {
        for (size_t i = 0; i < GetNPipelineTables(); i++)
        {
            SetFlowTableSize(i, value);
        }
    }
}

void
OFSwitch13Device::SetGroupTableSize(uint32_t value)
{
    NS_LOG_FUNCTION(this << value);

    m_groupTabSize = value;
    if (m_datapath)
    {
        NS_ABORT_MSG_IF(GetGroupTableEntries() > value, "Can't reduce table size to this value.");
        for (size_t i = 0; i < 4; i++)
        {
            m_datapath->groups->features->max_groups[i] = value;
        }
    }
}

void
OFSwitch13Device::SetMeterTableSize(uint32_t value)
{
    NS_LOG_FUNCTION(this << value);

    m_meterTabSize = value;
    if (m_datapath)
    {
        NS_ABORT_MSG_IF(GetMeterTableEntries() > value, "Can't reduce table size to this value.");
        m_datapath->meters->features->max_meter = value;
    }
}

void
OFSwitch13Device::DatapathTimeout(struct datapath* dp)
{
    meter_table_add_tokens(dp->meters);
    pipeline_timeout(dp->pipeline);

    // Check for chan/s in links (port) status.
    for (const auto& port : m_ports)
    {
        port->PortUpdateState();
    }

    // Update traced values.
    m_groupEntries = GetGroupTableEntries();
    m_meterEntries = GetMeterTableEntries();
    m_sumFlowEntries = GetSumFlowEntries();

    // The pipeline delay is estimated as k * log (n), where 'k' is the
    // m_tcamDelay set to the time for a single TCAM operation, and 'n' is the
    // current number of entries on all flow tables.
    m_pipeDelay = m_sumFlowEntries < 2U
                      ? m_tcamDelay
                      : m_tcamDelay * (int64_t)ceil(log2(static_cast<double>(m_sumFlowEntries)));

    // The CPU load is estimated based on the CPU consumed tokens since last
    // timeout operation.
    m_cpuLoad = DataRate(m_cpuConsumed / m_timeout.GetSeconds());
    m_cpuConsumed = 0;

    // Refill the pipeline bucket with tokens based on elapsed time
    // (bucket capacity is set to the number of tokens for an entire second).
    Time elapTime = Simulator::Now() - m_lastTimeout;
    uint64_t addTokens = m_cpuCapacity.GetBitRate() * elapTime.GetSeconds();
    uint64_t maxTokens = m_cpuCapacity.GetBitRate();
    m_cpuTokens = std::min(m_cpuTokens + addTokens, maxTokens);

    dp->last_timeout = time_now();
    m_lastTimeout = Simulator::Now();
    m_datapathTimeoutTrace(this);
    Simulator::Schedule(m_timeout, &OFSwitch13Device::DatapathTimeout, this, m_datapath);
}

/**
 * @brief Send Experimenter message to the controller every second
 *
 * @param dp
 */
void
OFSwitch13Device::GraphPortsUpdate(struct datapath* dp)
{
    // struct ofl_msg_experimenter msg;
    // msg.header.type = OFPT_EXPERIMENTER;
    // msg.exp_type = 1;
    // msg.experimenter_id = 0; // HELLO LLPD
    // msg.data_length = 0;

    // size_t n_interfaces = 0;
    // //olsr table
    // std::map< Ipv4Address, std::set<Ipv4Address> > table = dev_olsr->GetAuxTable();
    // for (auto it = table.cbegin(); it != table.cend(); ++it) {
    //   msg.data_length +=(1+1+3+4+6 + (*it).second.size()*4 ); // length + port + ipv4 + mac48 +
    //   N_neighbours n_interfaces++;
    // }

    // // NOTA todas as interfaces têm de ser sdn ports
    // assert(n_interfaces <= m_ports.size());
    // if(n_interfaces==0){
    //   NS_LOG_FUNCTION ("Number of interfaces are equal to zero!");
    //   std::cout << "Number of interfaces are equal to zero!" << std::endl;
    //   //return;
    //   msg.data_length = (1+1+3+4+6) * m_ports.size();
    // }

    // msg.data = (uint8_t*) malloc ( msg.data_length);

    // // SDN ports mac and ipv4 address
    // size_t l = 0;
    // //uint8_t i_port = 1;// port no (starts at 1).
    // for (auto i = m_ports.begin(); i != m_ports.end(); ++i){

    //   //from node
    //   Ptr<Node> n = (*i)->GetPortDevice()->GetNode();
    //   Ptr<NetDevice> nd = (*i)->GetPortDevice();

    //   Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
    //   uint32_t interface_index = ipv4->GetInterfaceForDevice(nd);

    //   Ipv4Address ipAddr =  ipv4->GetAddress (interface_index,0).GetLocal ();
    //   //std::cout << ipAddr << " ";
    //   //Address tmp = nd->GetAddress();
    //   //std::cout << tmp.GetLength() << std::endl; // 6 bytes = 48 bits --> mac address
    //   Mac48Address macAddr = Mac48Address::ConvertFrom(nd->GetAddress() );
    //   //std::cout << macAddr << std::endl;

    //   //LENGTH 1byte
    //   msg.data[l] = table[ipAddr].size()*4 + 1+1+3+4+6 ;
    //   // SDN PORT 1 byte
    //   msg.data[l+1] = (*i)->GetPortNo();
    //   // ipv4address 4 bytes
    //   ipAddr.Serialize(msg.data + l+2+3);
    //   //MAC48 6 bytes
    //   macAddr.CopyTo(msg.data + l+ 6+3);
    //   //std::cout << "hellloo----"<<(int)i_port << " " << (int)msg.data[l] << " "  << ipAddr << "
    //   " << macAddr << std::endl;

    //   l = 1+1+3+4+6;

    //   for (auto it2 = table[ipAddr].begin(); it2 != table[ipAddr].end(); it2++)
    //     {
    //         std::cout << (*it2) <<" ";
    //         (*it2).Serialize(msg.data + l);
    //         l+=4; //ipv4 - 4byte
    //     }

    //  //i_port++;
    // }

    // assert(l == msg.data_length);

    // Simulator::Schedule (Simulator::Now () + Seconds(1), &OFSwitch13Device::GraphPortsUpdate,
    //                      this,  dp );

    // dp_send_message (m_datapath, (struct ofl_msg_header *)&msg, 0);
}

void
OFSwitch13Device::SendStatstoCtrl(int length, uint8_t* data, uint32_t exp_type)
{
    struct ofl_msg_experimenter msg;
    msg.header.type = OFPT_EXPERIMENTER;
    msg.exp_type = exp_type;
    msg.experimenter_id = 0; // HELLO LLPD
    msg.data_length = length;
    msg.data = data;
    dp_send_message(m_datapath, (struct ofl_msg_header*)&msg, 0);
    // std::cout << "return from dp_send_message" << res << std::endl;
    for (auto& wf : m_workflowCallback)
    {
        wf(0); // SEND
    }
    // m_workflowCallback(0);
}

std::vector<Ipv4Address>
OFSwitch13Device::GetIpInterfaces()
{
    std::vector<Ipv4Address> tmp;
    for (auto i = m_ports.begin(); i != m_ports.end(); ++i)
    {
        // from node
        Ptr<Node> n = (*i)->GetPortDevice()->GetNode();
        Ptr<NetDevice> nd = (*i)->GetPortDevice();

        Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
        uint32_t interface_index = ipv4->GetInterfaceForDevice(nd);

        Ipv4Address ipAddr = ipv4->GetAddress(interface_index, 0).GetLocal();
        tmp.push_back(ipAddr);
    }

    return tmp;
}

int
OFSwitch13Device::SendPacketInMessage(struct packet* pkt,
                                      uint8_t tableId,
                                      uint8_t reason,
                                      uint16_t maxLength,
                                      uint64_t cookie)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid << tableId << reason);
    std::cout << " -- Packet In " << m_dpId << " " << Simulator::Now().GetMicroSeconds()
              << std::endl;

    // Create the packet_in message.
    struct ofl_msg_packet_in msg;
    msg.header.type = OFPT_PACKET_IN;
    msg.total_len = pkt->buffer->size;
    msg.reason = (enum ofp_packet_in_reason)reason;
    msg.table_id = tableId;
    msg.cookie = cookie;
    msg.data = (uint8_t*)pkt->buffer->data;

    // A maxLength of OFPCML_NO_BUFFER means that the complete packet should be
    // sent, and it should not be buffered. However, in this implementation we
    // always save the packet into buffer to avoid losing ns-3 packet id
    // reference. This is not full compliant with OpenFlow specification, but
    // works very well here.

    dp_buffers_save(pkt->dp->buffers, pkt);
    msg.buffer_id = pkt->buffer_id;

    // std::cout << "SendPacketInMessage (maxLength) " << maxLength
    //           << "(buffer->size): " << pkt->buffer->size << "msg.data: " << pkt->buffer->data
    //           << std::endl;

    msg.data_length = MIN(maxLength, pkt->buffer->size);

    if (!pkt->handle_std->valid)
    {
        packet_handle_std_validate(pkt->handle_std);
    }
    msg.match = (struct ofl_match_header*)&pkt->handle_std->match;
    // if(pkt->handle_std->proto->udp)
    // std::cout << "OFSwitch13Device::SendPacketInMessage "<< msg.match  << " " <<
    // pkt->handle_std->proto->udp->udp_src << std::endl;

    // Increase packet-in counter and send the message.
    m_cPacketIn++;
    return dp_send_message(pkt->dp, (struct ofl_msg_header*)&msg, nullptr);
}

bool
OFSwitch13Device::SendToReservedPort(struct packet* pkt, uint32_t queueNo)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid << pkt->in_port);
    uint32_t inPort = pkt->in_port;
    Ptr<OFSwitch13Port> port = GetSwitchPort(inPort);
    if (!port)
    {
        NS_LOG_ERROR("Can't forward packet to invalid port.");
        return false;
    }

    // When a packet is sent to OpenFlow pipeline, we keep track of its original
    // ns3::Packet using the PipelinePacket structure. When the packet is
    // processed by the pipeline with no internal changes, we forward the
    // original ns3::Packet to the specified output port. When internal changes
    // are necessary, we need to create a new packet with the modified content
    // and copy all packet tags to this new one. This approach is more expensive
    // than the previous one, but is far more simple than identifying which
    // changes were performed in the packet to modify the original ns3::Packet.
    Ptr<Packet> packet;
    if (m_pipePkt.IsValid())
    {
        NS_ASSERT_MSG(m_pipePkt.HasId(pkt->ns3_uid), "Invalid packet ID.");
        if (pkt->changes)
        {
            // The original ns-3 packet was modified by OpenFlow switch.
            // Create a new packet with modified data and copy tags from the
            // original packet.
            NS_LOG_DEBUG("Packet " << pkt->ns3_uid << " modified by switch.");
            // std::cout << "Packet " << pkt->ns3_uid << " modified by switch. NORMAL -->"
            //           << std::endl;
            // packet->Print(std::cout);
            // std::cout << std::endl;

            // TODO: this has a bug, but it should never happen. When going to normal port, set
            // field should not be implemented
            packet = PacketFromBuffer(pkt->buffer);
            OFSwitch13Device::CopyTags(m_pipePkt.GetPacket(), packet);
        }
        else
        {
            // Using the original ns-3 packet.
            // std::cout << "Using the original ns-3 packet." << std::endl;
            packet = m_pipePkt.GetPacket();
            // packet->Print(std::cout);
            // std::cout << "USING original ns3 packet NORMAL " << packet->GetUid()
            //           << " size: " << packet->GetSize() << " | " << pkt->buffer->size <<
            //           std::endl;
        }
    }
    else
    {
        // This is a new packet, probably created by the controller and sent to
        // the switch within an OpenFlow packet-out message.
        NS_ASSERT_MSG(pkt->ns3_uid == 0, "Invalid packet ID.");
        NS_LOG_DEBUG("Creating new ns-3 packet from OpenFlow buffer.");
        // std::cout << "Creating new ns-3 packet from OpenFlow buffer. NORMAL" << std::endl;
        packet = PacketFromBuffer(pkt->buffer);
    }

    // debug print packet
    // std::cout << "\n Packet:" << std::endl;
    // packet->Print(std::cout);
    // std::cout << std::endl;

    // Send the packet to switch port.
    return port->SendNormalAction(packet, queueNo, pkt->tunnel_id);
}

bool
OFSwitch13Device::SendToSwitchPort(struct packet* pkt, uint32_t portNo, uint32_t queueNo)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid << portNo);
    NS_LOG_DEBUG("OFSwitch13Device::SendToSwitchPort");

    Ptr<OFSwitch13Port> port = GetSwitchPort(portNo);
    if (!port)
    {
        NS_LOG_ERROR("Can't forward packet to invalid port.");
        return false;
    }

    // When a packet is sent to OpenFlow pipeline, we keep track of its original
    // ns3::Packet using the PipelinePacket structure. When the packet is
    // processed by the pipeline with no internal changes, we forward the
    // original ns3::Packet to the specified output port. When internal changes
    // are necessary, we need to create a new packet with the modified content
    // and copy all packet tags to this new one. This approach is more expensive
    // than the previous one, but is far more simple than identifying which
    // changes were performed in the packet to modify the original ns3::Packet.
    Ptr<Packet> packet;

    /* -- If CSMA-net-dev or virtual-net-dev --- */
    if (!port->PortDeviceIsWiFi())
    {
        if (m_pipePkt.IsValid())
        {
            NS_ASSERT_MSG(m_pipePkt.HasId(pkt->ns3_uid), "Invalid packet ID.");
            if (pkt->changes)
            {
                // The original ns-3 packet was modified by OpenFlow switch.
                // Create a new packet with modified data and copy tags from the
                // original packet.
                NS_LOG_DEBUG("Packet " << pkt->ns3_uid << " modified by switch.");
                packet = PacketFromBuffer(pkt->buffer);
                OFSwitch13Device::CopyTags(m_pipePkt.GetPacket(), packet);
            }
            else
            {
                // Using the original ns-3 packet.
                packet = m_pipePkt.GetPacket();
            }
        }
        else
        {
            // This is a new packet, probably created by the controller and sent to
            // the switch within an OpenFlow packet-out message.
            NS_ASSERT_MSG(pkt->ns3_uid == 0, "Invalid packet ID.");
            NS_LOG_DEBUG("Creating new ns-3 packet from OpenFlow buffer.");
            packet = PacketFromBuffer(pkt->buffer);
        }
    }
    else
    { /* -- If WIFI-net-dev --- */
        if (m_pipePkt.IsValid())
        {
            NS_ASSERT_MSG(m_pipePkt.HasId(pkt->ns3_uid), "Invalid packet ID.");
            if (pkt->changes)
            {
                // The original ns-3 packet was modified by OpenFlow switch.
                // Create a new packet with modified data and copy tags from the
                // original packet.
                NS_LOG_DEBUG("Packet " << pkt->ns3_uid << " modified by switch.");
                if (pkt->ns3_uid == 1720)
                {
                    // std::cout << "\n\n\t"
                    //           << "Packet " << pkt->ns3_uid << " modified by switch." <<
                    //           std::endl;
                }

                // packet = PacketFromBuffer (pkt->buffer);
                // OFSwitch13Device::CopyTags (m_pipePkt.GetPacket (), packet);
                // this doesn't work in previous versions. Alternative:

                // The byte buffer stores the serialized content of the headers and trailers added
                // to a packet.
                // Ptr<Packet> pkt2 = Create<Packet> ((uint8_t*)pkt->buffer->data,
                // pkt->buffer->size, true); pkt2->Print(std::cout); std::cout << std::endl;

                // std::cout << pkt->buffer->data << std::endl;

                // std::cout << pkt->handle_std->proto->eth->eth_dst << std::endl;
                // example serialize and descerealize
                Ptr<Packet> pkt_org = m_pipePkt.GetPacket();
                // uint8_t *buf = new uint8_t(pkt_org->GetSize ());
                // pkt_org->CopyData(buf , pkt_org->GetSize()); nao vai com os tags --> teria de ser
                // serialize Ptr<Packet> pkt3 = Create<Packet> (buf, pkt_org->GetSize(), true);
                packet = Create<Packet>(*pkt_org, (uint8_t*)pkt->buffer->data, pkt_org->GetSize());

                std::ostringstream stream;
                packet->Print(stream);
                // if (packet->GetSize() > 600)
                // {
                //     std::cout << "\n\n\t"
                //               << "Packet " << packet->GetUid()
                //               << " modified by switch. (pkt-size): " << pkt->buffer->size
                //               << " (packet size):" << packet->GetSize() << " (device): "
                //               << (*m_ports.begin())->GetPortDevice()->GetNode()->GetId()
                //               << std::endl;
                //     packet->Print(std::cout);
                //     std::cout << "--------\n " << std::endl;
                // }

                NS_LOG_DEBUG(stream.str() << "\n");
            }
            else
            {
                // Using the original ns-3 packet.
                packet = m_pipePkt.GetPacket();
                // if (packet->GetSize() > 600)
                // {
                //     std::cout << "USING original ns3 packet " << packet->GetUid()
                //               << " size: " << packet->GetSize() << " | " << pkt->buffer->size
                //               << std::endl;
                //     packet->Print(std::cout);
                //     std::cout << "\n\n\t" << packet->GetUid() << std::endl;
                // }
            }
        }
        else
        {
            // This is a new packet, probably created by the controller and sent to
            // the switch within an OpenFlow packet-out message.
            NS_ASSERT_MSG(pkt->ns3_uid == 0, "Invalid packet ID.");
            NS_LOG_DEBUG("Creating new ns-3 packet from OpenFlow buffer.");

            // ARP packets are the only ones created by the ctrl, and arp request and arp replies
            // have the same size
            // Ptr<Packet> pkt_org = m_pipePkt.GetPacket();
            // packet = Create<Packet>(*pkt_org, (uint8_t*)pkt->buffer->data, pkt_org->GetSize());

            packet = myPacketFromBuffer(pkt->buffer);
            // packet->Print(std::cout);
            EthernetHeader eth_h;
            packet->RemoveHeader(eth_h);
            EthernetTrailer eth_t;
            packet->RemoveTrailer(eth_t);
            // length 14+4 = 18 bytes (header+trailer)
            LlcSnapHeader llc_h;
            packet->RemoveHeader(llc_h);
            // 8
            ArpHeader arp_h;
            packet->RemoveHeader(arp_h);
            // 28
            int offset = 28 + 8 + 18;
            if (pkt->buffer->size - offset == 0 || llc_h.GetType() == 0x806)
            {
                packet = Create<Packet>(0);
            }
            else
            {
                packet = Create<Packet>((uint8_t*)pkt->buffer->data + offset,
                                        pkt->buffer->size - offset);
            }

            packet->AddHeader(arp_h);
            packet->AddHeader(llc_h);
            packet->AddHeader(eth_h);
            packet->AddTrailer(eth_t);

            // TODO ver se o primeiro create for preciso
        }
    }

    // Send the packet to switch port.
    return port->Send(packet, queueNo, pkt->tunnel_id);
}

void
OFSwitch13Device::SendToPipeline(Ptr<Packet> packet,
                                 uint32_t portNo,
                                 uint16_t protocol,
                                 const Address& from,
                                 const Address& to,
                                 uint64_t tunnelId)
{
    NS_LOG_FUNCTION(this << packet << portNo << tunnelId);
    NS_LOG_INFO("SendToPipeline " << packet << " " << portNo << " " << protocol << " " << from
                                  << " " << to);

    NS_ASSERT_MSG(!m_pipePkt.IsValid(), "Another packet in pipeline.");

    // Creating the internal OpenFlow packet structure from ns-3 packet
    // Allocate buffer with some extra space for OpenFlow packet modifications.
    uint32_t headRoom;
    uint32_t bodyRoom;
    struct packet* pkt;
    Ptr<OFSwitch13Port> port_tmp = m_ports.at(portNo - 1);
    // Ptr<NetDevice> netDev_tmp = port_tmp->GetPortDevice();
    // Ptr<WifiNetDevice> wifiDev = netDev_tmp->GetObject<WifiNetDevice> ();
    /*if(port_tmp->PortDeviceIsWiFi() ){ // WIFI



      headRoom = 128 + 2; // nao sei para que é este valor!!!! ERROR
      bodyRoom = packet->GetSize () +  WIFI_HEADER_LEN ; // size of 802.11 header = 24
    //VLAN_ETH_HEADER_LEN;
      // std::cout << VLAN_ETH_HEADER_LEN <<std::endl; 18
      struct ofpbuf *buffer = BufferFromPacket (packet, bodyRoom, headRoom);
      std::cout <<packet->GetSize () <<  " " << buffer->size <<  std::endl;

      struct mac_frame pkt_mac;
      to.CopyTo(pkt_mac.mac_dst);
      from.CopyTo(pkt_mac.mac_src);
      pkt_mac.mac_offset = protocol; // NOTA: size vem no sitio do protocol para wifi-net-devices
      pkt = packet_create (m_datapath, portNo, buffer,
                                          tunnelId, false, &pkt_mac);
      // printf("Packet createD \n");
      // // print the content of my packet on the standard output.));
      // packet->Print (std::cout);

    }
    else{*/ // ETHERNET
    /* in netdev.c Attempts to receive a packet from 'netdev' into 'buffer', which the caller
     * must have initialized with sufficient room for the packet.The space
     * required to receive any packet is ETH_HEADER_LEN bytes, plus VLAN_HEADER_LEN
     * bytes, plus the device's MTU (which may be retrieved via netdev_get_mtu()).
     * (Some devices do not allow for a VLAN header, in which case VLAN_HEADER_LEN
     * need not be included.)*/
    headRoom = 128 + 2;
    bodyRoom = packet->GetSize() + VLAN_ETH_HEADER_LEN;
    struct ofpbuf* buffer = BufferFromPacket(packet, bodyRoom, headRoom);
    pkt = packet_create(m_datapath, portNo, buffer, tunnelId, false);

    //}

    // Save the ns-3 packet into pipeline structure. Note that we are using a
    // private packet uid to avoid conflicts with ns3::Packet uid.
    pkt->ns3_uid = OFSwitch13Device::GetNewPacketId();
    // std::cout << "SendToPipeline " << packet->GetUid() << " -> " << pkt->ns3_uid << std::endl;

    m_pipePkt.SetPacket(pkt->ns3_uid, packet);

    NS_LOG_INFO("BLA1 PAcket UID " << pkt->ns3_uid);

    // Send the packet to pipeline.
    pipeline_process_packet(m_datapath->pipeline, pkt);
}

int
OFSwitch13Device::SendToController(Ptr<Packet> packet, Ptr<RemoteController> remoteCtrl)
{
    if (!remoteCtrl->m_socket)
    {
        NS_LOG_ERROR("No controller connection. Discarding message.");
        return -1;
    }

    // TODO: No support for auxiliary connections.
    return remoteCtrl->m_handler->SendMessage(packet);
}

void
OFSwitch13Device::ReceiveFromController(Ptr<Packet> packet, Address from)
{
    NS_LOG_FUNCTION(this << packet << from);

    struct ofl_msg_header* msg;
    ofl_err error;

    Ptr<RemoteController> remoteCtrl = GetRemoteController(from);
    NS_ASSERT_MSG(remoteCtrl, "Error returning controller for this address.");

    struct sender senderCtrl;
    senderCtrl.remote = remoteCtrl->m_remote;
    senderCtrl.conn_id = 0; // TODO No support for auxiliary connections

    // Get the OpenFlow buffer and unpack the message.
    NS_LOG_LOGIC("ReceiveFrom Controller");
    struct ofpbuf* buffer = BufferFromPacket(packet, packet->GetSize());
    // print buffer
    for (size_t i = 0; i < buffer->size; i++)
    {
        /* code */
        NS_LOG_LOGIC((int)*((uint8_t*)buffer->data + i));
    }
    NS_LOG_LOGIC("\n");
    error = ofl_msg_unpack((uint8_t*)buffer->data,
                           buffer->size,
                           &msg,
                           &senderCtrl.xid,
                           m_datapath->exp);

    // Check for error while unpacking the message.
    if (error)
    {
        // The BOFUSS librady only unpacks messages that has the same
        // OFP_VERSION that is supported by the datapath implementation.
        // However, when an OpenFlow connection is first established, each side
        // of the connection must immediately send an OFPT_HELLO message with
        // the version field set to the highest OpenFlow switch protocol version
        // supported by the sender. Upon receipt of this message, the recipient
        // must calculate the OpenFlow switch protocol version to be used, and
        // the negotiated version must be the smaller of the version number that
        // was sent and the one that was received in the version fields. So, for
        // the OFPT_HELLO message, we will check for advertised version to see
        // if it is higher than ours, in which case we can continue.
        struct ofp_header* header = (struct ofp_header*)buffer->data;
        if (header->type != OFPT_HELLO || header->version <= OFP_VERSION)
        {
            // This is not a hello message or the advertised version is lower
            // than OFP_VERSION. Notify the error and return.
            ReplyWithErrorMessage(error, buffer, &senderCtrl);
            ofpbuf_delete(buffer);
            return;
        }
        else
        {
            // The advertised version is equal or higher than OFP_VERSION. Let's
            // change the message version to OFP_VERSION so the message can be
            // successfully unpacked and we can continue.
            header->version = OFP_VERSION;
            error = ofl_msg_unpack((uint8_t*)buffer->data,
                                   buffer->size,
                                   &msg,
                                   &senderCtrl.xid,
                                   m_datapath->exp);

            // Check for any other error while unpacking the message.
            if (error)
            {
                // Notify the error and return.
                ReplyWithErrorMessage(error, buffer, &senderCtrl);
                ofpbuf_delete(buffer);
                return;
            }
        }
    }

    // Print message content.
    char* msgStr = ofl_msg_to_string(msg, m_datapath->exp);
    Ipv4Address ctrlIp = InetSocketAddress::ConvertFrom(from).GetIpv4();
    // std::cout << "RX from controller " << ctrlIp << ": " << msgStr << std::endl;

    NS_LOG_DEBUG("RX from controller " << ctrlIp << ": " << msgStr);
    free(msgStr);

    // Increase internal counters based on message type.
    switch (msg->type)
    {
    case (OFPT_PACKET_OUT): {
        // std::cout << "\n\t BLAAAAAAAAA----PACKETOUT" << std::endl;
        std::cout << " -- Packet Out " << m_dpId << " " << Simulator::Now().GetMicroSeconds()
                  << std::endl;
        m_cPacketOut++;
        break;
    }
    case (OFPT_FLOW_MOD): {
        m_cFlowMod++;
        std::cout << " -- Packet FLOW MOD " << m_dpId << " " << Simulator::Now().GetMicroSeconds()
                  << std::endl;
        break;
    }
    case (OFPT_METER_MOD): {
        m_cMeterMod++;
        break;
    }
    case (OFPT_GROUP_MOD): {
        m_cGroupMod++;
        break;
    }
    case (OFPT_ECHO_REPLY): {
        // std::cout << "\n\t BLAAAAAAAAA----ECHO" << std::endl;
        //  callback to workflow TODO
        for (auto& wf : m_workflowCallback)
        {
            wf(1); // SEND
        }
        // m_workflowCallback(1);
        break;
    }
    default: {
    }
    }
    // Send the message to handler.
    error = handle_control_msg(m_datapath, msg, &senderCtrl);
    if (error)
    {
        // It is assumed that if a handler returns with error, it did not use
        // any part of the control message, thus it can be freed up. If no error
        // is returned however, the message must be freed inside the handler
        // because the handler might keep parts of the message.
        ofl_msg_free(msg, m_datapath->exp);
        ReplyWithErrorMessage(error, buffer, &senderCtrl);
    }

    // if(msg->type == OFPT_HELLO){
    //   std::cout << "OFPT_HELLO" << std::endl;
    //   int length;
    //   uint8_t   *data;
    //   length = (1+4+4+6) * GetNSwitchPorts() + 1;
    //   data = (uint8_t*) malloc ( length);
    //   data[0] = 100;
    //   // SDN ports mac and ipv4 address
    //   size_t l = 1;
    //   //uint8_t i_port = 1;// port no (starts at 1).
    //   //for (auto i = m_ports.begin(); i != m_ports.end(); ++i){
    //   for (uint32_t i = 1; i <= GetNSwitchPorts(); i++){ // port no (starts at 1).
    //     Ptr<OFSwitch13Port> p = GetSwitchPort(i);
    //     Ptr<Node> n = p->GetPortDevice()->GetNode();
    //     Ptr<NetDevice> nd = p->GetPortDevice();
    //     Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
    //     uint32_t interface_index = ipv4->GetInterfaceForDevice(nd);

    //     Ipv4Address ipAddr =  ipv4->GetAddress (interface_index,0).GetLocal ();
    //     Mac48Address macAddr = Mac48Address::ConvertFrom(nd->GetAddress() );
    //     data[l] = 1+4+4+6 ; //LENGTH 1byte
    //     data[l+1] = p->GetPortNo(); // SDN PORT 4 byte
    //     ipAddr.Serialize(data + l+1+4); // ipv4address 4 bytes
    //     macAddr.CopyTo(data + l+ 1+4+4); //MAC48 6 bytes
    //     l += 1+4+4+6;
    //   }

    //   struct ofl_msg_experimenter msg;
    //   msg.header.type = OFPT_EXPERIMENTER;
    //   msg.exp_type = 1;
    //   msg.experimenter_id = 0; // HELLO LLPD
    //   msg.data_length = length;
    //   msg.data = data;
    //   dp_send_message (m_datapath, (struct ofl_msg_header *)&msg, &senderCtrl);

    // }

    // If we got here, let's free the buffer.
    ofpbuf_delete(buffer);
}

int
OFSwitch13Device::ReplyWithErrorMessage(ofl_err error,
                                        struct ofpbuf* buffer,
                                        struct sender* senderCtrl)
{
    NS_LOG_FUNCTION(this << error);

    struct ofl_msg_error err;
    err.header.type = OFPT_ERROR;
    err.type = (enum ofp_error_type)ofl_error_type(error);
    err.code = ofl_error_code(error);
    err.data_length = buffer->size;
    err.data = (uint8_t*)buffer->data;

    char* msgStr = ofl_msg_to_string((struct ofl_msg_header*)&err, nullptr);
    NS_LOG_ERROR("Error processing OpenFlow message. Reply with " << msgStr);
    free(msgStr);

    return dp_send_message(m_datapath, (struct ofl_msg_header*)&err, senderCtrl);
}

void
OFSwitch13Device::SocketCtrlSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    // std::cout << "------ ENTREI ---------" ;
    NS_LOG_INFO("Controller accepted connection request!");
    Ptr<RemoteController> remoteCtrl = GetRemoteController(socket);
    // remoteCtrl->m_remote = remote_create (m_datapath, 0, 0);
    remoteCtrl->m_remote = remote_create(m_datapath);

    // As we have more than one socket that is used for communication between
    // this OpenFlow switch device and controllers, we need to handle the process
    // of sending/receiving OpenFlow messages to/from sockets in an independent
    // way. So, each socket has its own socket handler to this end.
    remoteCtrl->m_handler = CreateObject<OFSwitch13SocketHandler>(socket);
    remoteCtrl->m_handler->SetReceiveCallback(
        MakeCallback(&OFSwitch13Device::ReceiveFromController, this));

    // Send the OpenFlow Hello message.
    struct ofl_msg_header msg;
    msg.type = OFPT_HELLO;

    struct sender senderCtrl;
    senderCtrl.remote = remoteCtrl->m_remote;
    senderCtrl.conn_id = 0; // TODO No support for auxiliary connections.
    senderCtrl.xid = 0;
    dp_send_message(m_datapath, &msg, &senderCtrl);
}

void
OFSwitch13Device::SocketCtrlFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    NS_LOG_ERROR("Controller did not accepted connection request!");

    // Loop over controllers looking for the one associated to this socket and
    // remove it from the collection.
    for (auto it = m_controllers.begin(); it != m_controllers.end(); it++)
    {
        Ptr<RemoteController> remoteCtrl = *it;
        if (remoteCtrl->m_socket == socket)
        {
            m_controllers.erase(it);
            return;
        }
    }
}

void
OFSwitch13Device::NotifyMeterEntryCreated(struct meter_entry* entry)
{
    NS_LOG_FUNCTION(this << entry->config->meter_id);

    // Update meter entry last_fill field with the time of last datapath timeout,
    // and force a new bucket refill based on this elapsed time.
    for (size_t i = 0; i < entry->config->meter_bands_num; i++)
    {
        entry->stats->band_stats[i]->last_fill =
            static_cast<uint64_t>(m_lastTimeout.GetMilliSeconds());
    }
    refill_bucket(entry);
}

void
OFSwitch13Device::NotifyPacketCloned(struct packet* pkt, struct packet* clone)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid);

    // Assigning a new unique ID for this cloned packet.
    clone->ns3_uid = OFSwitch13Device::GetNewPacketId();
    m_pipePkt.NewCopy(clone->ns3_uid);
}

void
OFSwitch13Device::NotifyPacketDestroyed(struct packet* pkt)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid);

    // This is the packet current under pipeline. Let's delete this copy.
    if (m_pipePkt.IsValid() && m_pipePkt.HasId(pkt->ns3_uid))
    {
        bool valid = m_pipePkt.DelCopy(pkt->ns3_uid);
        if (!valid)
        {
            NS_LOG_DEBUG("Packet " << pkt->ns3_uid << " done at this switch.");
        }
        return;
    }

    // This destroyed packet has no ns-3 ID. This packet was probably created by
    // the OpenFlow controller and sent to the switch within an OpenFlow
    // packet-out message. No action is required here.
    if (pkt->ns3_uid == 0)
    {
        NS_LOG_DEBUG("Deleting lib packet with no corresponding ns-3 packet.");
        return;
    }

    // If we got here, this packet must not be valid on the pipeline structure.
    NS_ASSERT_MSG((m_pipePkt.IsValid() && !m_pipePkt.HasId(pkt->ns3_uid)) || !m_pipePkt.IsValid(),
                  "Packet still valid in pipeline.");

    // This destroyed packet is probably an old packet that was previously saved
    // into buffer and will be deleted now, freeing up space for a new packet at
    // same buffer index (that's how the library handles the buffer). So, we are
    // going to remove this packet from our buffer, if it still exists there.
    BufferPacketDelete(pkt->ns3_uid);
    NS_LOG_DEBUG("Packet " << pkt->ns3_uid << " done at this switch.");
}

void
OFSwitch13Device::NotifyPacketDroppedByMeter(struct packet* pkt, struct meter_entry* entry)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid << entry->stats->meter_id);

    uint32_t meterId = entry->stats->meter_id;
    NS_ASSERT_MSG(m_pipePkt.HasId(pkt->ns3_uid), "Invalid packet ID.");
    NS_LOG_DEBUG("OpenFlow meter id " << meterId << " dropped packet " << pkt->ns3_uid);

    // Increase counter and fire drop trace source.
    m_meterDropTrace(m_pipePkt.GetPacket(), meterId);
}

void
OFSwitch13Device::NotifyPacketDroppedByTable(struct packet* pkt, struct flow_table* table)
{
    NS_LOG_FUNCTION(this << pkt->ns3_uid << table->stats->table_id);

    uint8_t tableId = table->stats->table_id;
    NS_ASSERT_MSG(m_pipePkt.HasId(pkt->ns3_uid), "Invalid packet ID.");
    NS_LOG_DEBUG("OpenFlow table id " << +tableId << " dropped unmatched packet " << pkt->ns3_uid);

    // Fire drop trace source.
    m_tableDropTrace(m_pipePkt.GetPacket(), tableId);
}

void
OFSwitch13Device::BufferPacketSave(uint64_t packetId, time_t timeout)
{
    NS_LOG_FUNCTION(this << packetId);

    NS_ASSERT_MSG(m_pipePkt.HasId(packetId), "Invalid packet ID.");

    // Remove from pipeline and save into buffer.
    std::pair<uint64_t, Ptr<Packet>> entry(packetId, m_pipePkt.GetPacket());
    auto ret = m_bufferPkts.insert(entry);
    if (ret.second == true)
    {
        NS_LOG_DEBUG("Packet " << packetId << " saved into buffer.");

        // time_t now = time(0);
        // printf("\n\n --> buff id: %lu (timeout: %d)->Timestamp: %f / %f\n" , packetId ,
        // (int)timeout,(double)time(NULL), Simulator::Now ().ToDouble (Time::S));

        m_bufferSaveTrace(m_pipePkt.GetPacket());
    }
    else
    {
        NS_LOG_WARN("Packet " << packetId << " already in buffer.");
    }
    m_pipePkt.DelCopy(packetId);
    NS_ASSERT_MSG(!m_pipePkt.IsValid(), "Packet copy still in pipeline.");

    // Scheduling the buffer remove for expired packet. Since packet timeout
    // resolution is expressed in seconds, let's double it to avoid rounding
    // conflicts.
    // Simulator::Schedule (Time::FromInteger (2 * timeout, Time::S),
    //                      &OFSwitch13Device::BufferPacketDelete, this, packetId);
    Simulator::Schedule(Seconds(10), &OFSwitch13Device::BufferPacketDelete, this, packetId);
}

void
OFSwitch13Device::BufferPacketRetrieve(uint64_t packetId)
{
    NS_LOG_FUNCTION(this << packetId);

    NS_ASSERT_MSG(!m_pipePkt.IsValid(), "Another packet in pipeline.");

    // Find packet in buffer.
    auto it = m_bufferPkts.find(packetId);
    NS_ASSERT_MSG(it != m_bufferPkts.end(),
                  "Packet not found in buffer." + std::to_string(packetId));

    // Save packet into pipeline structure.
    m_pipePkt.SetPacket(it->first, it->second);
    m_bufferRetrieveTrace(m_pipePkt.GetPacket());

    // Delete packet from buffer.
    NS_LOG_DEBUG("Packet " << packetId << " removed from buffer.");
    m_bufferPkts.erase(it);
}

void
OFSwitch13Device::BufferPacketDelete(uint64_t packetId)
{
    NS_LOG_FUNCTION(this << packetId);

    // Delete from buffer map.
    auto it = m_bufferPkts.find(packetId);
    if (it != m_bufferPkts.end())
    {
        NS_LOG_DEBUG("Expired packet " << packetId << " deleted from buffer.");
        m_bufferExpireTrace(it->second);
        m_bufferPkts.erase(it);
    }
}

Ptr<OFSwitch13Device::RemoteController>
OFSwitch13Device::GetRemoteController(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    for (const auto& ctrl : m_controllers)
    {
        if (ctrl->m_socket == socket)
        {
            return ctrl;
        }
    }
    NS_ABORT_MSG("Error returning controller for this socket.");
}

Ptr<OFSwitch13Device::RemoteController>
OFSwitch13Device::GetRemoteController(Address address)
{
    NS_LOG_FUNCTION(this << address);

    for (const auto& ctrl : m_controllers)
    {
        if (ctrl->m_address == address)
        {
            return ctrl;
        }
    }
    return nullptr;
}

Ptr<OFSwitch13Device::RemoteController>
OFSwitch13Device::GetRemoteController(struct remote* remote)
{
    NS_LOG_FUNCTION(this << remote);

    for (const auto& ctrl : m_controllers)
    {
        if (ctrl->m_remote == remote)
        {
            return ctrl;
        }
    }
    NS_ABORT_MSG("Error returning controller for this remote pointer.");
}

uint64_t
OFSwitch13Device::GetNewPacketId()
{
    return ++m_globalPktId;
}

bool
OFSwitch13Device::CopyTags(Ptr<const Packet> srcPkt, Ptr<const Packet> dstPkt)
{
    // Copy packet tags.
    PacketTagIterator pktIt = srcPkt->GetPacketTagIterator();
    while (pktIt.HasNext())
    {
        PacketTagIterator::Item item = pktIt.Next();
        Callback<ObjectBase*> constructor = item.GetTypeId().GetConstructor();
        Tag* tag = dynamic_cast<Tag*>(constructor());
        item.GetTag(*tag);
        dstPkt->AddPacketTag(*tag);
        delete tag;
    }

    // Copy byte tags.
    ByteTagIterator bytIt = srcPkt->GetByteTagIterator();
    while (bytIt.HasNext())
    {
        ByteTagIterator::Item item = bytIt.Next();
        Callback<ObjectBase*> constructor = item.GetTypeId().GetConstructor();
        Tag* tag = dynamic_cast<Tag*>(constructor());
        item.GetTag(*tag);
        dstPkt->AddByteTag(*tag);
        delete tag;
    }

    return true;
}

void
OFSwitch13Device::RegisterDatapath(uint64_t id, Ptr<OFSwitch13Device> dev)
{
    std::pair<uint64_t, Ptr<OFSwitch13Device>> entry(id, dev);
    auto ret = OFSwitch13Device::m_globalSwitchMap.insert(entry);
    NS_ABORT_MSG_IF(ret.second == false, "Error when registering datapath.");
}

void
OFSwitch13Device::UnregisterDatapath(uint64_t id)
{
    auto it = OFSwitch13Device::m_globalSwitchMap.find(id);
    if (it != OFSwitch13Device::m_globalSwitchMap.end())
    {
        OFSwitch13Device::m_globalSwitchMap.erase(it);
        return;
    }
    NS_ABORT_MSG("Error when removing datapath.");
}

OFSwitch13Device::RemoteController::RemoteController()
    : m_socket(nullptr),
      m_handler(nullptr),
      m_remote(nullptr)
{
    m_address = Address();
}

OFSwitch13Device::PipelinePacket::PipelinePacket()
    : m_valid(false),
      m_packet(nullptr)
{
}

void
OFSwitch13Device::PipelinePacket::SetPacket(uint64_t id, Ptr<Packet> packet)
{
    NS_ASSERT_MSG(id && packet, "Invalid packet metadata values.");
    m_valid = true;
    m_packet = packet;
    m_ids.emplace_back(id);
}

Ptr<Packet>
OFSwitch13Device::PipelinePacket::GetPacket() const
{
    NS_ASSERT_MSG(IsValid(), "Invalid packet metadata.");
    return m_packet;
}

void
OFSwitch13Device::PipelinePacket::Invalidate()
{
    m_valid = false;
    m_packet = nullptr;
    m_ids.clear();
}

bool
OFSwitch13Device::PipelinePacket::IsValid() const
{
    return m_valid;
}

void
OFSwitch13Device::PipelinePacket::NewCopy(uint64_t id)
{
    NS_ASSERT_MSG(m_valid, "Invalid packet metadata.");
    m_ids.emplace_back(id);
}

bool
OFSwitch13Device::PipelinePacket::DelCopy(uint64_t id)
{
    NS_ASSERT_MSG(m_valid, "Invalid packet metadata.");

    for (auto it = m_ids.begin(); it != m_ids.end(); it++)
    {
        if (*it == id)
        {
            m_ids.erase(it);
            break;
        }
    }
    if (m_ids.size() == 0)
    {
        Invalidate();
    }
    return m_valid;
}

bool
OFSwitch13Device::PipelinePacket::HasId(uint64_t id)
{
    NS_ASSERT_MSG(m_valid, "Invalid packet metadata.");

    for (auto it = m_ids.begin(); it != m_ids.end(); it++)
    {
        if (*it == id)
        {
            return true;
        }
    }
    return false;
}

} // namespace ns3
