/*
 * Copyright (c) 2005,2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "wifi-net-device.h"

#include "frame-exchange-manager.h"
#include "sta-wifi-mac.h"
#include "wifi-phy.h"

#include "ns3/channel.h"
#include "ns3/eht-configuration.h"
#include "ns3/ethernet-header.h"
#include "ns3/ethernet-trailer.h"
#include "ns3/he-configuration.h"
#include "ns3/ht-configuration.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4.h"
#include "ns3/llc-snap-header.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/object-vector.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/vht-configuration.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("WifiNetDevice");

NS_OBJECT_ENSURE_REGISTERED(WifiNetDevice);

TypeId
WifiNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::WifiNetDevice")
            .SetParent<NetDevice>()
            .AddConstructor<WifiNetDevice>()
            .SetGroupName("Wifi")
            .AddAttribute("Mtu",
                          "The MAC-level Maximum Transmission Unit",
                          UintegerValue(MAX_MSDU_SIZE - LLC_SNAP_HEADER_LENGTH),
                          MakeUintegerAccessor(&WifiNetDevice::SetMtu, &WifiNetDevice::GetMtu),
                          MakeUintegerChecker<uint16_t>(1, MAX_MSDU_SIZE - LLC_SNAP_HEADER_LENGTH))
            .AddAttribute("Channel",
                          "The channel attached to this device",
                          PointerValue(),
                          MakePointerAccessor(&WifiNetDevice::GetChannel),
                          MakePointerChecker<Channel>(),
                          TypeId::DEPRECATED,
                          "class WifiNetDevice; use the Channel "
                          "attribute of WifiPhy")
            .AddAttribute("Phy",
                          "The PHY layer attached to this device.",
                          PointerValue(),
                          MakePointerAccessor((Ptr<WifiPhy>(WifiNetDevice::*)() const) &
                                                  WifiNetDevice::GetPhy,
                                              &WifiNetDevice::SetPhy),
                          MakePointerChecker<WifiPhy>())
            .AddAttribute(
                "Phys",
                "The PHY layers attached to this device (11be multi-link devices only).",
                ObjectVectorValue(),
                MakeObjectVectorAccessor(&WifiNetDevice::GetPhy, &WifiNetDevice::GetNPhys),
                MakeObjectVectorChecker<WifiPhy>())
            .AddAttribute("Mac",
                          "The MAC layer attached to this device.",
                          PointerValue(),
                          MakePointerAccessor(&WifiNetDevice::GetMac, &WifiNetDevice::SetMac),
                          MakePointerChecker<WifiMac>())
            .AddAttribute(
                "RemoteStationManager",
                "The station manager attached to this device.",
                PointerValue(),
                MakePointerAccessor(&WifiNetDevice::SetRemoteStationManager,
                                    (Ptr<WifiRemoteStationManager>(WifiNetDevice::*)() const) &
                                        WifiNetDevice::GetRemoteStationManager),
                MakePointerChecker<WifiRemoteStationManager>())
            .AddAttribute("RemoteStationManagers",
                          "The remote station managers attached to this device (11be multi-link "
                          "devices only).",
                          ObjectVectorValue(),
                          MakeObjectVectorAccessor(&WifiNetDevice::GetRemoteStationManager,
                                                   &WifiNetDevice::GetNRemoteStationManagers),
                          MakeObjectVectorChecker<WifiRemoteStationManager>())
            .AddAttribute("HtConfiguration",
                          "The HtConfiguration object.",
                          PointerValue(),
                          MakePointerAccessor(&WifiNetDevice::GetHtConfiguration),
                          MakePointerChecker<HtConfiguration>())
            .AddAttribute("VhtConfiguration",
                          "The VhtConfiguration object.",
                          PointerValue(),
                          MakePointerAccessor(&WifiNetDevice::GetVhtConfiguration),
                          MakePointerChecker<VhtConfiguration>())
            .AddAttribute("HeConfiguration",
                          "The HeConfiguration object.",
                          PointerValue(),
                          MakePointerAccessor(&WifiNetDevice::GetHeConfiguration),
                          MakePointerChecker<HeConfiguration>())
            .AddAttribute("EhtConfiguration",
                          "The EhtConfiguration object.",
                          PointerValue(),
                          MakePointerAccessor(&WifiNetDevice::GetEhtConfiguration),
                          MakePointerChecker<EhtConfiguration>());
    return tid;
}

WifiNetDevice::WifiNetDevice()
    : m_standard(WIFI_STANDARD_UNSPECIFIED),
      m_configComplete(false)
{
    NS_LOG_FUNCTION_NOARGS();
}

WifiNetDevice::~WifiNetDevice()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
WifiNetDevice::DoDispose()
{
    NS_LOG_FUNCTION_NOARGS();
    m_node = nullptr;
    if (m_mac)
    {
        m_mac->Dispose();
        m_mac = nullptr;
    }
    for (auto& phy : m_phys)
    {
        if (phy)
        {
            phy->Dispose();
            phy = nullptr;
        }
    }
    m_phys.clear();
    for (auto& stationManager : m_stationManagers)
    {
        if (stationManager)
        {
            stationManager->Dispose();
            stationManager = nullptr;
        }
    }
    m_stationManagers.clear();
    if (m_htConfiguration)
    {
        m_htConfiguration->Dispose();
        m_htConfiguration = nullptr;
    }
    if (m_vhtConfiguration)
    {
        m_vhtConfiguration->Dispose();
        m_vhtConfiguration = nullptr;
    }
    if (m_heConfiguration)
    {
        m_heConfiguration->Dispose();
        m_heConfiguration = nullptr;
    }
    if (m_ehtConfiguration)
    {
        m_ehtConfiguration->Dispose();
        m_ehtConfiguration = nullptr;
    }
    NetDevice::DoDispose();
}

void
WifiNetDevice::DoInitialize()
{
    NS_LOG_FUNCTION_NOARGS();

    for (const auto& phy : m_phys)
    {
        if (phy)
        {
            phy->Initialize();
        }
    }
    if (m_mac)
    {
        m_mac->Initialize();
    }
    for (const auto& stationManager : m_stationManagers)
    {
        if (stationManager)
        {
            stationManager->Initialize();
        }
    }
    NetDevice::DoInitialize();
}

void
WifiNetDevice::CompleteConfig()
{
    if (!m_mac || m_phys.empty() || m_stationManagers.empty() || !m_node || m_configComplete)
    {
        return;
    }
    NS_ABORT_IF(m_phys.size() != m_stationManagers.size());
    m_mac->SetWifiPhys(m_phys);
    m_mac->SetWifiRemoteStationManagers(m_stationManagers);
    m_mac->SetForwardUpCallback(MakeCallback(&WifiNetDevice::ForwardUp, this));
    m_mac->SetLinkUpCallback(MakeCallback(&WifiNetDevice::LinkUp, this));
    m_mac->SetLinkDownCallback(MakeCallback(&WifiNetDevice::LinkDown, this));
    for (std::size_t linkId = 0; linkId < m_stationManagers.size(); linkId++)
    {
        m_stationManagers.at(linkId)->SetupPhy(m_phys.at(linkId));
        m_stationManagers.at(linkId)->SetupMac(m_mac);
    }
    m_configComplete = true;
}

void
WifiNetDevice::SetStandard(WifiStandard standard)
{
    NS_ABORT_MSG_IF(m_standard != WIFI_STANDARD_UNSPECIFIED, "Wifi standard already set");
    m_standard = standard;
}

WifiStandard
WifiNetDevice::GetStandard() const
{
    return m_standard;
}

void
WifiNetDevice::SetMac(const Ptr<WifiMac> mac)
{
    m_mac = mac;
    CompleteConfig();
}

void
WifiNetDevice::SetPhy(const Ptr<WifiPhy> phy)
{
    m_phys.clear();
    m_phys.push_back(phy);
    m_linkUp = true;
    CompleteConfig();
}

void
WifiNetDevice::SetPhys(const std::vector<Ptr<WifiPhy>>& phys)
{
    NS_ABORT_MSG_IF(phys.size() > 1 && !m_ehtConfiguration,
                    "Multiple PHYs only allowed for 11be multi-link devices");
    m_phys = phys;
    m_linkUp = true;
    CompleteConfig();
}

void
WifiNetDevice::SetRemoteStationManager(const Ptr<WifiRemoteStationManager> manager)
{
    m_stationManagers.clear();
    m_stationManagers.push_back(manager);
    CompleteConfig();
}

void
WifiNetDevice::SetRemoteStationManagers(const std::vector<Ptr<WifiRemoteStationManager>>& managers)
{
    NS_ABORT_MSG_IF(managers.size() > 1 && !m_ehtConfiguration,
                    "Multiple remote station managers only allowed for 11be multi-link devices");
    m_stationManagers = managers;
    CompleteConfig();
}

Ptr<WifiMac>
WifiNetDevice::GetMac() const
{
    return m_mac;
}

Ptr<WifiPhy>
WifiNetDevice::GetPhy() const
{
    return GetPhy(SINGLE_LINK_OP_ID);
}

Ptr<WifiPhy>
WifiNetDevice::GetPhy(uint8_t i) const
{
    NS_ASSERT(i < GetPhys().size());
    return GetPhys().at(i);
}

const std::vector<Ptr<WifiPhy>>&
WifiNetDevice::GetPhys() const
{
    return m_phys;
}

uint8_t
WifiNetDevice::GetNPhys() const
{
    return GetPhys().size();
}

Ptr<WifiRemoteStationManager>
WifiNetDevice::GetRemoteStationManager() const
{
    return GetRemoteStationManager(0);
}

Ptr<WifiRemoteStationManager>
WifiNetDevice::GetRemoteStationManager(uint8_t linkId) const
{
    NS_ASSERT(linkId < GetRemoteStationManagers().size());
    return GetRemoteStationManagers().at(linkId);
}

const std::vector<Ptr<WifiRemoteStationManager>>&
WifiNetDevice::GetRemoteStationManagers() const
{
    return m_stationManagers;
}

uint8_t
WifiNetDevice::GetNRemoteStationManagers() const
{
    return GetRemoteStationManagers().size();
}

void
WifiNetDevice::SetIfIndex(const uint32_t index)
{
    m_ifIndex = index;
}

uint32_t
WifiNetDevice::GetIfIndex() const
{
    return m_ifIndex;
}

Ptr<Channel>
WifiNetDevice::GetChannel() const
{
    for (uint8_t i = 1; i < GetNPhys(); i++)
    {
        if (GetPhy(i)->GetChannel() != GetPhy(i - 1)->GetChannel())
        {
            NS_ABORT_MSG("Do not call WifiNetDevice::GetChannel() when using multiple channels");
        }
    }

    return m_phys[SINGLE_LINK_OP_ID]->GetChannel();
}

void
WifiNetDevice::SetAddress(Address address)
{
    m_mac->SetAddress(Mac48Address::ConvertFrom(address));
}

Address
WifiNetDevice::GetAddress() const
{
    Ptr<StaWifiMac> staMac;
    std::set<uint8_t> linkIds;

    /**
     * Normally, the MAC address that the network device has to advertise to upper layers is
     * the MLD address, if this device is an MLD, or the unique MAC address, otherwise.
     * Advertising the MAC address returned by WifiMac::GetAddress() is therefore the right
     * thing to do in both cases. However, there is an exception: if this device is a non-AP MLD
     * associated with a single link AP (hence, no ML setup was done), we need to advertise the
     * MAC address of the link used to communicate with the AP. In fact, if we advertised the
     * MLD address, the AP could not forward a frame to us because it would not recognize our
     * MLD address as the MAC address of an associated station.
     */

    // Handle the exception first
    if (m_mac->GetTypeOfStation() == STA &&
        (staMac = StaticCast<StaWifiMac>(m_mac))->IsAssociated() && m_mac->GetNLinks() > 1 &&
        (linkIds = staMac->GetSetupLinkIds()).size() == 1 &&
        !GetRemoteStationManager(*linkIds.begin())
             ->GetMldAddress(m_mac->GetBssid(*linkIds.begin())))
    {
        return m_mac->GetFrameExchangeManager(*linkIds.begin())->GetAddress();
    }

    return m_mac->GetAddress();
}

bool
WifiNetDevice::SetMtu(const uint16_t mtu)
{
    if (mtu > MAX_MSDU_SIZE - LLC_SNAP_HEADER_LENGTH)
    {
        return false;
    }
    m_mtu = mtu;
    return true;
}

uint16_t
WifiNetDevice::GetMtu() const
{
    return m_mtu;
}

bool
WifiNetDevice::IsLinkUp() const
{
    return !m_phys.empty() && m_linkUp;
}

void
WifiNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    m_linkChanges.ConnectWithoutContext(callback);
}

bool
WifiNetDevice::IsBroadcast() const
{
    return true;
}

Address
WifiNetDevice::GetBroadcast() const
{
    return Mac48Address::GetBroadcast();
}

bool
WifiNetDevice::IsMulticast() const
{
    return true;
}

Address
WifiNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    return Mac48Address::GetMulticast(multicastGroup);
}

Address
WifiNetDevice::GetMulticast(Ipv6Address addr) const
{
    return Mac48Address::GetMulticast(addr);
}

bool
WifiNetDevice::IsPointToPoint() const
{
    return false;
}

bool
WifiNetDevice::IsBridge() const
{
    return false;
}

bool
WifiNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet << dest << protocolNumber);
    NS_LOG_INFO("WifiNetDevice::Send" << this << packet << dest << protocolNumber);
    NS_ASSERT(Mac48Address::IsMatchingType(dest));

    Mac48Address realTo = Mac48Address::ConvertFrom(dest);

    LlcSnapHeader llc;
    llc.SetType(protocolNumber);
    packet->AddHeader(llc);

    m_mac->NotifyTx(packet);

    if (realTo.IsBroadcast())
    {
        m_mac->NotifyBroadcastTx(packet);
    }

    m_mac->Enqueue(packet, realTo);
    return true;
}

Ptr<Node>
WifiNetDevice::GetNode() const
{
    return m_node;
}

void
WifiNetDevice::SetNode(const Ptr<Node> node)
{
    m_node = node;
    CompleteConfig();
}

bool
WifiNetDevice::NeedsArp() const
{
    return true;
}

// function from callbac
void
WifiNetDevice::WorkflowMonitor(int signal)
{
    NS_LOG_FUNCTION(this << signal);
    // TODO function
    if (signal == 0)
    {
        // TODO count++
        // after 10 exp messages sent, and none received controller is no longer in reach
        ; // std::cout << "WORKFLOW MESSAGE SENT" << std::endl;
    }
    else if (signal == 1)
    {
        // TODO reset counter
        ; // std::cout << "WORKFLOW MESSAGE RECEVEID" << std::endl;
    }
    else
    {
        ; // std::cout << "WORKFLOW MESSAGE ! " << std::endl;
    }
}

void
WifiNetDevice::SetOpenFlowReceiveCallback(NetDevice::PromiscReceiveCallback cb)
{
    NS_LOG_FUNCTION(&cb);
    m_openFlowRxCallback = cb;
    openflow_enabled = true;
}

void
WifiNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
{
    m_forwardUp = cb;
}

void
WifiNetDevice::ForwardUp(Ptr<const Packet> packet, Mac48Address from, Mac48Address to)
{
    NS_LOG_FUNCTION(this << packet << from << to);
    LlcSnapHeader llc;
    packet->PeekHeader(llc);
    NetDevice::PacketType type;
    if (to.IsBroadcast())
    {
        type = NetDevice::PACKET_BROADCAST;
    }
    else if (to.IsGroup())
    {
        type = NetDevice::PACKET_MULTICAST;
    }
    else if (to == GetAddress())
    {
        type = NetDevice::PACKET_HOST;
    }
    else
    {
        type = NetDevice::PACKET_OTHERHOST;
    }

    Ptr<Packet> copy = packet->Copy();
    Ptr<Packet> pkt_eth = packet->Copy();

    // ---- NOTIFY RX ---
    if (type != NetDevice::PACKET_OTHERHOST)
    {
        m_mac->NotifyRx(packet);
    }
    copy->RemoveHeader(llc);
    if (!m_promiscRx.IsNull())
    {
        m_mac->NotifyPromiscRx(copy);
    }

    // ---- FORWARD ---
    // ----- (1) PIPELINE ---
    // Check if this device is configure as an OpenFlow switch port.
    // nas interfaces wifi, se nao tiver o meu mac, ignoro
    if (type != NetDevice::PACKET_OTHERHOST && !m_openFlowRxCallback.IsNull() &&
        Simulator::Now() >= Seconds(14.5))
    {
        // We should!! forward the original packet (which includes the EthernetHeader) to
        // the OpenFlow receive callback for all kinds of packetType we receive
        // (broadcast, multicast, host or other host).
        // However, the WifiMacHeader is removed in the Received function on file adHocWifiMac.cc
        EthernetHeader eth_header(false);
        eth_header.SetLengthType(packet->GetSize());
        eth_header.SetSource(from);
        eth_header.SetDestination(to);

        EthernetTrailer trailer;
        // All Ethernet frames must carry a minimum payload of 46 bytes.  The
        // LLC SNAP header counts as part of this payload.  We need to padd out
        // if we don't have enough bytes.  These must be real bytes since they
        // will be written to pcap files and compared in regression trace files.
        //
        if (pkt_eth->GetSize() < 46)
        {
            // uint8_t buffer[46];
            // memset (buffer, 0, 46);
            // Ptr<Packet> padd = Create<Packet> (buffer, 46 - pkt_eth->GetSize ());
            // pkt_eth->AddAtEnd (padd);
            NS_LOG_FUNCTION(this << "Add Padding to end of packet!");
        }

        pkt_eth->AddHeader(eth_header);
        if (Node::ChecksumEnabled())
        {
            trailer.EnableFcs(true);
        }
        trailer.CalcFcs(packet); // original
        pkt_eth->AddTrailer(trailer);

        // pkt_eth->Print(std::cout);
        // std::cout << std::endl;
        // packet->Print(std::cout);
        // std::cout << "og: " << packet->GetSize() << " | eth_p: " << pkt_eth->GetSize() <<
        // std::endl;

        m_mac->NotifyOpenFlowRx(packet);
        m_openFlowRxCallback(this, pkt_eth, llc.GetType(), from, to, type);
        return;
    }

    // ----- (2) NORMAL ---
    if (type != NetDevice::PACKET_OTHERHOST)
    {
        m_forwardUp(this, copy, llc.GetType(), from);
    }

    if (!m_promiscRx.IsNull())
    {
        m_mac->NotifyPromiscRx(copy);
        m_promiscRx(this, copy, llc.GetType(), from, to, type);
    }
}

bool
WifiNetDevice::Forward_OFPP_NORMAL(Ptr<const Packet> packet, Mac48Address from, Mac48Address to)
{
    NS_LOG_FUNCTION(this << packet << from << to << packet->GetSize());
    // packet->Print(std::cout);
    NetDevice::PacketType type;
    if (to.IsBroadcast())
    {
        type = NetDevice::PACKET_BROADCAST;
    }
    else if (to.IsGroup())
    {
        type = NetDevice::PACKET_MULTICAST;
    }
    else if (to == m_mac->GetAddress())
    {
        type = NetDevice::PACKET_HOST;
    }
    else
    {
        type = NetDevice::PACKET_OTHERHOST;
    }
    m_mac->NotifyOpenFlowNormalAction(packet);

    // If this packet is not destined for some other host, it must be for us
    // as either a broadcast, multicast or unicast.  We need to hit the mac
    // packet received trace hook and forward the packet up the stack.
    Ptr<Packet> copy = packet->Copy();
    LlcSnapHeader llc;
    copy->RemoveHeader(llc);
    // std::cout << copy->GetSize() << std::endl;

    if (type != NetDevice::PACKET_OTHERHOST)
    {
        // m_mac->NotifyRx (packet);
        m_forwardUp(this, copy, llc.GetType(), from);
    }
    //
    // For all kinds of packetType we receive, we hit the promiscuous sniffer
    // hook and pass a copy up to the promiscuous callback.  Pass a copy to
    // make sure that nobody messes with our packet.
    //
    if (!m_promiscRx.IsNull())
    {
        // m_mac->NotifyPromiscRx (copy);
        m_promiscRx(this, copy, llc.GetType(), from, to, type);
    }

    return true;
}

void
WifiNetDevice::LinkUp()
{
    m_linkUp = true;
    m_linkChanges();
}

void
WifiNetDevice::LinkDown()
{
    m_linkUp = false;
    m_linkChanges();
}

bool
WifiNetDevice::SendFrom(Ptr<Packet> packet,
                        const Address& source,
                        const Address& dest,
                        uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet << source << dest << protocolNumber);
    NS_ASSERT(Mac48Address::IsMatchingType(dest));
    NS_ASSERT(Mac48Address::IsMatchingType(source));

    Mac48Address realTo = Mac48Address::ConvertFrom(dest);
    Mac48Address realFrom = Mac48Address::ConvertFrom(source);

    LlcSnapHeader llc;
    llc.SetType(protocolNumber);
    // packet->AddHeader (llc); TODO por alguma razao com isto sem ser comentado nunca ha arp reply

    m_mac->NotifyTx(packet);
    // TODO
    // if(realTo.IsBroadcast()){
    //   m_mac->NotifyBroadcastTx(packet);
    // }
    if (packet->GetUid() == 742988)
    {
        std::cout << m_mac->GetAddress() << ": " << realTo << "\n\n-- send from " << realFrom
                  << "protocol " << protocolNumber << std::endl;
    }
    // std::cout << realTo << "\n\n-- send from " << realFrom << "protocol " << protocolNumber <<
    // std::endl; packet->Print (std::cout);
    m_mac->Enqueue(packet, realTo, realFrom);

    return true;
}

void
WifiNetDevice::SetPromiscReceiveCallback(PromiscReceiveCallback cb)
{
    m_promiscRx = cb;
    m_mac->SetPromisc();
}

bool
WifiNetDevice::SupportsSendFrom() const
{
    return m_mac->SupportsSendFrom();
}

void
WifiNetDevice::SetHtConfiguration(Ptr<HtConfiguration> htConfiguration)
{
    m_htConfiguration = htConfiguration;
}

Ptr<HtConfiguration>
WifiNetDevice::GetHtConfiguration() const
{
    return (m_standard >= WIFI_STANDARD_80211n ? m_htConfiguration : nullptr);
}

void
WifiNetDevice::SetVhtConfiguration(Ptr<VhtConfiguration> vhtConfiguration)
{
    m_vhtConfiguration = vhtConfiguration;
}

Ptr<VhtConfiguration>
WifiNetDevice::GetVhtConfiguration() const
{
    return (m_standard >= WIFI_STANDARD_80211ac ? m_vhtConfiguration : nullptr);
}

void
WifiNetDevice::SetHeConfiguration(Ptr<HeConfiguration> heConfiguration)
{
    m_heConfiguration = heConfiguration;
}

Ptr<HeConfiguration>
WifiNetDevice::GetHeConfiguration() const
{
    return (m_standard >= WIFI_STANDARD_80211ax ? m_heConfiguration : nullptr);
}

void
WifiNetDevice::SetEhtConfiguration(Ptr<EhtConfiguration> ehtConfiguration)
{
    m_ehtConfiguration = ehtConfiguration;
}

Ptr<EhtConfiguration>
WifiNetDevice::GetEhtConfiguration() const
{
    return (m_standard >= WIFI_STANDARD_80211be ? m_ehtConfiguration : nullptr);
}

} // namespace ns3
