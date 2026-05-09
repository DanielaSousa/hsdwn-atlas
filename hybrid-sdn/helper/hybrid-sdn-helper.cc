/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Daniela Sousa
 */
#ifdef NS3_OFSWITCH13

#include "hybrid-sdn-helper.h"

#include <ns3/hybrid-sdn-controller.h>
#include <ns3/ofswitch13-port.h>
#include <ns3/ofswitch13-stats-calculator.h>

namespace ns3
{
using namespace olsr;

NS_LOG_COMPONENT_DEFINE("HybridSDNHelper");
NS_OBJECT_ENSURE_REGISTERED(HybridSDNHelper);

Ipv4AddressHelper HybridSDNHelper::m_ipv4helper = Ipv4AddressHelper("10.100.0.0", "255.255.255.0");

// class OFSwitch13Controller;
class HybridSDNController;

#define EPISODICAL_EXP_MSG 1
#define PERIODIC_EXP_MSG 0

double
normalization(double input,
              double input_start,
              double input_end,
              double output_start,
              double output_end)
{
    if (input < input_start)
    {
        NS_LOG_WARN("HybridSDNHelper: Input value out of range!" << input << input_start
                                                                 << input_end);
        return input_start;
    }
    if (input > input_end)
    {
        NS_LOG_WARN("HybridSDNHelper: Input value out of range!" << input << input_start
                                                                 << input_end);
        return input_end;
    }

    double slope = 1.0 * (output_end - output_start) / (input_end - input_start);
    return output_start + slope * (input - input_start);
}

/**
 * @brief normalization function
 *
 * @param input value to be normalized between [input_start,input_end ]
 * @param input_start min
 * @param input_end max
 * @return double belongs between [0, 100]
 */
double
norm_v(double input, double input_start, double input_end)
{
    double output_start = 0.0;
    double output_end = 100.0;
    return normalization(input, input_start, input_end, output_start, output_end);
}

HybridSDNHelper::HybridSDNHelper(InternetStackHelper& int_stack)
{
    NS_LOG_FUNCTION(this);
    m_internet = int_stack;
    m_blocked = false;

    // Set OpenFlow device factory TypeId.
    m_devFactory.SetTypeId("ns3::OFSwitch13Device");

    // configure energy source
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
}

HybridSDNHelper::HybridSDNHelper()
    : m_blocked(false)
{
    NS_LOG_FUNCTION(this);

    // Set OpenFlow device factory TypeId.
    m_devFactory.SetTypeId("ns3::OFSwitch13Device");

    // configure energy source
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
}

HybridSDNHelper::~HybridSDNHelper()
{
    NS_LOG_FUNCTION(this);
}

TypeId
HybridSDNHelper::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::HybridSDNHelper")
            .SetParent<Object>()
            .SetGroupName("HybridSDN")
            .AddAttribute("ChannelDataRate",
                          "The data rate to be used for the OpenFlow channel.",
                          DataRateValue(DataRate("10Gb/s")),
                          MakeDataRateAccessor(&HybridSDNHelper::SetChannelDataRate),
                          MakeDataRateChecker())
            .AddAttribute("ChannelType",
                          "The configuration used to create the OpenFlow channel",
                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                          EnumValue(HybridSDNHelper::SINGLECSMA),
                          MakeEnumAccessor(&HybridSDNHelper::SetChannelType),
                          MakeEnumChecker(HybridSDNHelper::SINGLECSMA,
                                          "SingleCsma",
                                          HybridSDNHelper::DEDICATEDCSMA,
                                          "DedicatedCsma",
                                          HybridSDNHelper::DEDICATEDP2P,
                                          "DedicatedP2p",
                                          HybridSDNHelper::SINGLELTE,
                                          "SingleLTE"))
            .AddAttribute("Port",
                          "The port number where controller will be available.",
                          UintegerValue(6653),
                          MakeUintegerAccessor(&HybridSDNHelper::m_controlPort),
                          MakeUintegerChecker<uint16_t>());
    return tid;
}

void
HybridSDNHelper::RemainingEnergy(std::string context, double oldValue, double remainingEnergy)
{
    NS_LOG_LOGIC(Simulator::Now().GetSeconds()
                 << "s Current remaining energy = " << remainingEnergy << "J");
    // std::cout << context << std::endl;
    int nodeId = stoi(context);
    NS_LOG_LOGIC(context << " " << nodeId);
    if (remainingEnergy < 1)
    {
        std::cout << "NodeID " << nodeId << "has no ENERGY LEFT: Node DOWN!" << std::endl;
    }
    energy[nodeId_OPFdev[nodeId]] = remainingEnergy;
}

std::set<Ipv4Address>
HybridSDNHelper::ProcessNeighbours(std::set<Ipv4Address> list, Ipv4Address ip)
{
    Ipv4Mask mask = Ipv4Mask("/24"); // only works for here
    std::set<Ipv4Address> t;
    for (auto x : list)
    {
        if (mask.IsMatch(ip, x))
            t.insert(x);
    }
    return t;
}

void
HybridSDNHelper::RoutingTableChanged(std::string context, uint32_t size)
{
    if (Simulator::Now() > lastExperimenter + Seconds(0.01) &&
        Simulator::Now() > Seconds(25)) // 10 miliseconds interval
    {
        NS_LOG_FUNCTION(this);

        // get node
        std::string delimiter = "/";
        size_t pos = context.find(delimiter);
        context.erase(0, pos + delimiter.length() + 9); // remove _/NodeList/
        pos = context.find(delimiter);
        int tmp_node = std::atoi(context.substr(0, pos).c_str());

        // std::cout << "HybridSDNHelper::RoutingTableChanged " << tmp_node << std::endl;

        NS_ASSERT_MSG(nodeId_OPFdev.find(tmp_node) != nodeId_OPFdev.end(),
                      std::to_string(tmp_node) + " Node does not exist!");

        Ptr<OFSwitch13Device> t_dev = nodeId_OPFdev[tmp_node];
        std::map<Ipv4Address, std::set<Ipv4Address>> table;
        /**
         * GetAuxTable brings information that is not on the olsr tables, so going forward,
         * we use GetNeighbours which retrieves information directly from the olsr table
         */
        auto list = rtg[t_dev]->GetNeighbours();
        for (auto i = 1; i <= (int)t_dev->GetNSwitchPorts(); i++) // FOR EACH SW port
        {
            Ptr<OFSwitch13Port> p = t_dev->GetSwitchPort(i); // port numbers start at 1
            Ptr<Node> n = p->GetPortDevice()->GetNode();
            Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
            Ipv4Address t = ipv4->GetAddress(i, 0).GetLocal();
            table.insert({t, ProcessNeighbours(list, t)});
        }
        auto sw_id = nodeId_OPFdev[tmp_node]->GetDpId();
        std::cout << "RoutingTableChanged " << tmp_node << " " << sw_id << " "
                  << Simulator::Now().GetMicroSeconds() << std::endl;

        SendExperimenterMsgFromDev(t_dev, table, EPISODICAL_EXP_MSG);
    }
}

void
HybridSDNHelper::SendExperimenterMsgFromDev(Ptr<OFSwitch13Device> t_dev,
                                            std::map<Ipv4Address, std::set<Ipv4Address>> table,
                                            uint32_t exp_type)
{
    // create experimenter message helper
    ExpMsgData msg = ExpMsgData();

    msg.energy = (int)norm_v(energy[t_dev], 0.0, 100.0); // só garantir que fica entre os 0 e os 100

    for (uint32_t i = 1; i <= t_dev->GetNSwitchPorts(); i++)
    {
        Ptr<OFSwitch13Port> p = t_dev->GetSwitchPort(i);
        // from node
        Ptr<Node> n = p->GetPortDevice()->GetNode();
        Ptr<NetDevice> nd = p->GetPortDevice();

        Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();

        uint32_t interface_index = ipv4->GetInterfaceForDevice(nd);

        Ipv4Address ipAddr = ipv4->GetAddress(interface_index, 0).GetLocal();
        Mac48Address macAddr = Mac48Address::ConvertFrom(nd->GetAddress());

        // Ptr<WifiNetDevice> wifi_nd = DynamicCast<WifiNetDevice>(nd);

        Ptr<WifiPhy> channel =
            DynamicCast<WifiNetDevice>(nd)->GetPhy(); // DynamicCast<WifiPhy>(device->GetPhy());
        // WifiStandard standard = channel->GetStandard();
        uint32_t t_avbw =
            (uint32_t)(m_avbw.at(std::make_pair(n->GetId(), interface_index)).GetAvBw());
        // std::cout << n->GetId() << " " << interface_index << " " << t_avbw << std::endl;

        uint32_t maxC =
            (uint32_t)(m_avbw.at(std::make_pair(n->GetId(), interface_index)).GetMaxC());

        msg.ports.push_back(
            {(uint8_t)channel->GetStandard(), (int)p->GetPortNo(), ipAddr, macAddr, t_avbw, maxC});

        MyFlowStats tmp;
        for (auto it2 = table[ipAddr].begin(); it2 != table[ipAddr].end(); it2++)
        {
            tmp = qm.GetFlowStats(ipAddr, (*it2));
            if (!tmp.valid)
            {
                msg.oneHopNeighs[(int)p->GetPortNo()].push_back({*it2, 0, 0, 0, 0});
            }
            else
            {
                msg.oneHopNeighs[(int)p->GetPortNo()].push_back(
                    {*it2,
                     (uint8_t)norm_v(tmp.u_delivery_ratio, 0.0, 1.0),
                     (int)tmp.avgDelay.GetMicroSeconds(),
                     (uint8_t)norm_v(tmp.avgThroughput, 0.0, 12000000.0),
                     (uint8_t)norm_v(tmp.avgSnr, 0.0, 120.0)});
            }
        }
    }

    // serialize message
    uint8_t* data = nullptr;
    int length = msg.Serialize(data);
    // std::cout << "after serialize ";
    // for (auto i = 0; i < length; i++)
    // {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0')
    //               << (int)static_cast<unsigned char>(data[i]) << ' ';
    // }
    // std::cout << std::endl;
    t_dev->SendStatstoCtrl(length, data, exp_type); // uint8_t*
    NS_LOG_INFO("Message " << msg);
}

void
HybridSDNHelper::SendStatsMsgtoCtrl()
{
    NS_LOG_FUNCTION(this);
    lastExperimenter = Simulator::Now();

    // std::cout << "Experimenter msg " << Simulator::Now().GetSeconds() << " ";

    NS_ABORT_MSG_IF(!m_blocked, "OpenFlow channels not configured yet.");
    // for every device in the network with openflow -> send a message to the connected
    // controller

    for (uint32_t i = 0; i < m_openFlowDevs.GetN(); i++)
    {
        Ptr<OFSwitch13Device> t_dev = m_openFlowDevs.Get(i);
        std::map<Ipv4Address, std::set<Ipv4Address>> table;
        auto list = rtg[t_dev]->GetNeighbours();
        for (auto k = 1; k <= (int)t_dev->GetNSwitchPorts(); k++) // FOR EACH SW port
        {
            Ptr<OFSwitch13Port> p = t_dev->GetSwitchPort(1);
            Ptr<Node> n = p->GetPortDevice()->GetNode();
            Ptr<NetDevice> nd = p->GetPortDevice();
            Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
            Ipv4Address t = ipv4->GetAddress(k, 0).GetLocal();
            table.insert({t, ProcessNeighbours(list, t)});
        }

        SendExperimenterMsgFromDev(t_dev, table, PERIODIC_EXP_MSG);
    }
}

void
HybridSDNHelper::SetDeviceAttribute(std::string n1, const AttributeValue& v1)
{
    NS_LOG_FUNCTION(this);

    m_devFactory.Set(n1, v1);
}

void
HybridSDNHelper::SetChannelType(ChannelType type)
{
    NS_LOG_FUNCTION(this << type);

    // Set the channel type and address, which will select proper netowrk mask.
    m_channelType = type;
}

void
HybridSDNHelper::SetChannelDataRate(DataRate rate)
{
    NS_LOG_FUNCTION(this << rate);

    m_channelDataRate = rate;
}

void
HybridSDNHelper::EnableOpenFlowPcap(std::string prefix, bool promiscuous)
{
    NS_LOG_FUNCTION(this << prefix);

    NS_ABORT_MSG_IF(!m_blocked, "OpenFlow channels not configured yet.");
    switch (m_channelType)
    {
    case HybridSDNHelper::SINGLECSMA:
    case HybridSDNHelper::DEDICATEDCSMA: {
        m_csmaHelper.EnablePcap(prefix, m_controlDevs, promiscuous);
        break;
    }
    case HybridSDNHelper::DEDICATEDP2P: {
        m_p2pHelper.EnablePcap(prefix, m_controlDevs, promiscuous);
        break;
    }
    case HybridSDNHelper::SINGLELTE: {
        // m_p2pHelper.EnablePcap (prefix, m_controlDevs, promiscuous);
        // m_p2pHelper.EnablePcapAll(prefix);
        // m_csmaHelper.EnablePcap(prefix, m_controlDevs, promiscuous);
        // m_csmaHelper.EnablePcapAll(prefix);
        m_csmaHelper.EnablePcap(prefix + "_pgw", m_pgw_csma, promiscuous);
        m_csmaHelper.EnablePcap(prefix + "_remote", m_controlDevs, promiscuous);
        break;
    }
    default: {
        NS_ABORT_MSG("Invalid OpenflowChannelType.");
    }
    }
}

void
HybridSDNHelper::EnableOpenFlowAscii(std::string prefix)
{
    NS_LOG_FUNCTION(this << prefix);

    NS_ABORT_MSG_IF(!m_blocked, "OpenFlow channels not configured yet.");
    AsciiTraceHelper ascii;
    switch (m_channelType)
    {
    case HybridSDNHelper::SINGLECSMA:
    case HybridSDNHelper::DEDICATEDCSMA: {
        m_csmaHelper.EnableAsciiAll(ascii.CreateFileStream(prefix + ".txt"));
        break;
    }
    case HybridSDNHelper::DEDICATEDP2P:
    case HybridSDNHelper::SINGLELTE: {
        m_p2pHelper.EnableAsciiAll(ascii.CreateFileStream(prefix + ".txt"));
        break;
    }
    default: {
        NS_ABORT_MSG("Invalid OpenflowChannelType.");
    }
    }
}

void
HybridSDNHelper::EnableDatapathStats(std::string prefix, bool useNodeNames)
{
    NS_LOG_FUNCTION(this << prefix);

    NS_ABORT_MSG_IF(!m_blocked, "OpenFlow channels not configured yet.");
    NS_ASSERT_MSG(prefix.size(), "Empty prefix string.");
    if (prefix.back() != '-')
    {
        prefix += "-";
    }

    ObjectFactory statsFactory("ns3::OFSwitch13StatsCalculator");
    Ptr<OFSwitch13StatsCalculator> statsCalculator;
    const std::string extension = ".log";

    // Iterate over the container and for each OpenFlow devices create a stats
    // calculator to monitor datapath statistcs.
    OFSwitch13DeviceContainer::Iterator it;
    for (it = m_openFlowDevs.Begin(); it != m_openFlowDevs.End(); it++)
    {
        Ptr<OFSwitch13Device> dev = *it;
        std::string filename = prefix;
        std::string nodename;

        if (useNodeNames)
        {
            Ptr<Node> node = dev->GetObject<Node>();
            nodename = Names::FindName(node);
        }
        if (nodename.size())
        {
            filename += nodename;
        }
        else
        {
            filename += std::to_string(dev->GetDatapathId());
        }
        filename += extension;

        statsFactory.Set("OutputFilename", StringValue(filename));
        statsCalculator = statsFactory.Create<OFSwitch13StatsCalculator>();
        statsCalculator->AggregateObject(dev);
        statsCalculator->HookSinks(dev);
    }
}

void
HybridSDNHelper::Create_external_OpenFlowChannels(void)
{
    // Create and start the connections between switches and controllers.
    switch (m_channelType)
    {
    case HybridSDNHelper::SINGLECSMA: {
        NS_LOG_INFO("Attach all switches and controllers to the same "
                    "CSMA network.");

        // Connecting all switches to the common channel.
        NetDeviceContainer switchDevices;
        switchDevices = m_csmaHelper.Install(m_switchNodes, m_csmaChannel);
        m_ipv4helper.Assign(switchDevices);
        InetSocketAddress addr(m_controlAddr, m_controlPort);

        // Start the connections between controller and switches.
        OFSwitch13DeviceContainer::Iterator ofDev;
        for (ofDev = m_openFlowDevs.Begin(); ofDev != m_openFlowDevs.End(); ofDev++)
        {
            NS_LOG_INFO("Connect switch " << (*ofDev)->GetDatapathId() << " to controller "
                                          << addr.GetIpv4() << " port " << addr.GetPort());
            Simulator::ScheduleNow(&OFSwitch13Device::StartControllerConnection, *ofDev, addr);
        }
        m_ipv4helper.NewNetwork();
        break;
    }
    case HybridSDNHelper::SINGLELTE: {
        NS_LOG_INFO("Attach all switches and controllers to the same "
                    "eNodeB (LTE) network.");

        // Connecting all switches to the common channel.
        // NetDeviceContainer ueLteDevs;
        ueLteDevs = m_lteHelper->InstallUeDevice(m_switchNodes); // ueLteDevs
        m_num_lte_iface = 2;
        Ipv4InterfaceContainer ueIpIface;
        ueIpIface = m_epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
        // m_ipv4helper.Assign (ueLteDevs);
        InetSocketAddress addr(m_controlAddr, m_controlPort);
        Ptr<NetDevice> pgw_csmaDev = GetPgwDev();
        // populates the arp table so the StartControllerConnection  can work
        PopulateARPcacheWithSDNinterfaces(m_controlNode,
                                          pgw_csmaDev,
                                          m_switchNodes); // tem de ser antes de criar os channels

        // Assign IP address to UEs, and install applications
        int u = 0;
        OFSwitch13DeviceContainer::Iterator ofDev;
        for (ofDev = m_openFlowDevs.Begin(); ofDev != m_openFlowDevs.End(); ofDev++)
        {
            NS_LOG_INFO("Connect switch " << (*ofDev)->GetDatapathId() << " to controller "
                                          << addr.GetIpv4() << " port " << addr.GetPort() << "addr "
                                          << addr);

            Ptr<Node> ueNode = m_switchNodes.Get(u);
            // Set the default gateway for the UE
            Ipv4StaticRoutingHelper ipv4RoutingHelper; // nao sei se deveria criar novo
            Ptr<Ipv4StaticRouting> ueStaticRouting =
                ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
            ueStaticRouting->SetDefaultRoute(m_epcHelper->GetUeDefaultGatewayAddress(), 2);

            // Attach a UE to a eNB
            m_lteHelper->Attach(ueLteDevs.Get(u), m_enbLteDevs.Get(0));

            // Start the tcp socket between controller and switches.
            Simulator::ScheduleNow(&OFSwitch13Device::StartControllerConnection, *ofDev, addr);

            u++;
        }

        //
        // OFSwitch13DeviceContainer::Iterator ofDev;
        // for (ofDev = m_openFlowDevs.Begin ();
        //     ofDev != m_openFlowDevs.End (); ofDev++)
        //   {

        //   }

        m_lteHelper->EnableTraces();

        // m_ipv4helper.NewNetwork ();
        break;
    }
    case HybridSDNHelper::DEDICATEDCSMA:
    case HybridSDNHelper::DEDICATEDP2P:
    default: {
        NS_ABORT_MSG("Invalid OpenflowChannelType.");
    }
    }
}

// Trace function for CSMA transmissions
void
CsmaTxTrace(std::string context, Ptr<const Packet> packet)
{
    std::cout << "CSMA TX Trace: " << context << " Packet UID: " << packet->GetUid()
              << " Size: " << packet->GetSize() << " bytes" << std::endl;
}

void
CsmaRxTrace(std::string context, Ptr<const Packet> packet)
{
    std::cout << "CSMA RX Trace: " << context << " Packet UID: " << packet->GetUid()
              << " Size: " << packet->GetSize() << " bytes" << std::endl;
}

void
HybridSDNHelper::Create_internal_OpenFlowChannels(void)
{
    // Create and start the connections between switches and controllers.
    switch (m_channelType)
    {
    case HybridSDNHelper::SINGLECSMA: {
        NS_LOG_INFO("Attach all switches and controllers to the same "
                    "CSMA network.");

        // Create the common channel for all switches and controllers.
        // Ptr<CsmaChannel> csmaChannel =
        //     CreateObjectWithAttributes<CsmaChannel>("DataRate",
        //     DataRateValue(m_channelDataRate));

        // Connecting all switches and controllers to the common channel.
        NetDeviceContainer switchDevices;
        // Ipv4InterfaceContainer controllerAddrs;
        // m_controlDevs = m_csmaHelper.Install(m_controlNode, m_csmaChannel);
        switchDevices = m_csmaHelper.Install(m_switchNodes, m_csmaChannel);

        // // Connect to CSMA Tx and Rx trace sources
        // for (uint32_t i = 0; i < switchDevices.GetN(); ++i)
        // {
        //     Ptr<CsmaNetDevice> dev = DynamicCast<CsmaNetDevice>(switchDevices.Get(i));
        //     if (dev)
        //     {
        //         dev->TraceConnect("MacTx", "node" + std::to_string(i),
        //         MakeCallback(&CsmaTxTrace)); dev->TraceConnect("MacRx", "node" +
        //         std::to_string(i), MakeCallback(&CsmaRxTrace));
        //     }
        // }

        // controllerAddrs = m_ipv4helper.Assign(m_controlDevs);
        m_ipv4helper.Assign(switchDevices);

        // Start the connections between controllers and switches.
        UintegerValue portValue;
        // for (uint32_t ctIdx = 0; ctIdx < controllerAddrs.GetN(); ctIdx++)
        // {
        uint32_t ctIdx = 0;
        m_controlApps.Get(ctIdx)->GetAttribute("Port", portValue);
        // InetSocketAddress addr(controllerAddrs.GetAddress(ctIdx), portValue.Get());
        InetSocketAddress addr(m_controlAddr, m_controlPort);

        OFSwitch13DeviceContainer::Iterator ofDev;
        for (ofDev = m_openFlowDevs.Begin(); ofDev != m_openFlowDevs.End(); ofDev++)
        {
            NS_LOG_INFO("Connect switch " << (*ofDev)->GetDatapathId() << " to controller "
                                          << addr.GetIpv4() << " port " << addr.GetPort());
            Simulator::ScheduleNow(&OFSwitch13Device::StartControllerConnection, *ofDev, addr);
        }
        // }
        m_ipv4helper.NewNetwork();
        break;
    }
    case HybridSDNHelper::SINGLELTE: {
        /* ---- connection PGW - Controller (CSMA) ---- */
        // Allready done in Install Controller
        /* ---- connect lte core with switches ----- */
        NS_LOG_INFO("Attach all switches and controllers to the same "
                    "eNodeB (LTE) network.");

        // Connecting all switches to the common channel.
        // NetDeviceContainer ueLteDevs;
        ueLteDevs = m_lteHelper->InstallUeDevice(m_switchNodes); // ueLteDevs
        m_num_lte_iface = 2;
        Ipv4InterfaceContainer ueIpIface;
        ueIpIface = m_epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
        // m_ipv4helper.Assign (ueLteDevs);
        InetSocketAddress addr(m_controlAddr, m_controlPort);
        Ptr<NetDevice> pgw_csmaDev = GetPgwDev();
        // populates the arp table so the StartControllerConnection  can work
        PopulateARPcacheWithSDNinterfaces(m_controlNode,
                                          pgw_csmaDev,
                                          m_switchNodes); // tem de ser antes de criar os channels

        // Assign IP address to UEs, and install applications
        int u = 0;
        OFSwitch13DeviceContainer::Iterator ofDev;
        for (ofDev = m_openFlowDevs.Begin(); ofDev != m_openFlowDevs.End(); ofDev++)
        {
            NS_LOG_INFO("Connect switch " << (*ofDev)->GetDatapathId() << " to controller "
                                          << addr.GetIpv4() << " port " << addr.GetPort() << "addr "
                                          << addr);

            Ptr<Node> ueNode = m_switchNodes.Get(u);
            // Set the default gateway for the UE
            Ipv4StaticRoutingHelper ipv4RoutingHelper; // nao sei se deveria criar novo
            Ptr<Ipv4StaticRouting> ueStaticRouting =
                ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
            ueStaticRouting->SetDefaultRoute(m_epcHelper->GetUeDefaultGatewayAddress(), 2);

            // Attach a UE to a eNB
            m_lteHelper->Attach(ueLteDevs.Get(u), m_enbLteDevs.Get(0));

            // Start the tcp socket between controller and switches.
            Simulator::ScheduleNow(&OFSwitch13Device::StartControllerConnection, *ofDev, addr);

            u++;
        }

        //
        // OFSwitch13DeviceContainer::Iterator ofDev;
        // for (ofDev = m_openFlowDevs.Begin ();
        //     ofDev != m_openFlowDevs.End (); ofDev++)
        //   {

        //   }

        m_lteHelper->EnableTraces();

        // m_ipv4helper.NewNetwork ();
        break;
    }

    case HybridSDNHelper::DEDICATEDCSMA:
    case HybridSDNHelper::DEDICATEDP2P:
    default: {
        NS_ABORT_MSG("Invalid OpenflowChannelType.");
    }
    }
}

Ptr<OFSwitch13Device>
HybridSDNHelper::InstallSwitch(Ptr<Node> swNode, NetDeviceContainer& swPorts)
{
    NS_LOG_FUNCTION(this << swNode);

    // Install the OpenFlow device into switch node.
    Ptr<OFSwitch13Device> openFlowDev = InstallSwitch(swNode);

    // ----------- Legacy Routing -------
    Ptr<Ipv4> ipv4 = swNode->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol();
    NS_ASSERT_MSG(ipv4rp, "No routing protocol associated with Ipv4");
    Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting>(ipv4rp);
    NS_ASSERT_MSG(lrp, "No Ipv4ListRouting");
    int16_t priority = 10;
    Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol(0, priority);
    NS_ASSERT_MSG(temp, "No Ipv4RoutingProtocol");
    rtg[openFlowDev] = DynamicCast<olsr::RoutingProtocol>(temp);
    NS_ASSERT_MSG(rtg[openFlowDev], "No olsr::RoutingProtocol");

    //------- Energy -------------
    // install source
    EnergySourceContainer sources = basicSourceHelper.Install(swNode);
    /* device energy model */
    WifiRadioEnergyModelHelper radioEnergyHelper;
    // configure radio energy model
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.00174));
    // install device model
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(swPorts.Get(0), sources);
    Ptr<BasicEnergySource> basicSourcePtr =
        DynamicCast<BasicEnergySource>(sources.Get(0)); // nao faço a menor
    // basicSourcePtr->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback
    // (&RemainingEnergy));
    //  /NodeList/*/$ns3::BasicEnergySource
    //  std::ostringstream trace2;
    //  trace2 << "/NodeList/" << swNode->GetId () <<
    //  "/$ns3::BasicEnergySource/RemainingEnergy";
    basicSourcePtr->TraceConnect("RemainingEnergy",
                                 std::to_string(swNode->GetId()),
                                 MakeCallback(&HybridSDNHelper::RemainingEnergy, this));

    std::ostringstream trace;
    trace << "/NodeList/" << swNode->GetId() << "/$ns3::olsr::RoutingProtocol/RoutingTableChanged";
    Config::Connect(trace.str(), MakeCallback(&ns3::HybridSDNHelper::RoutingTableChanged, this));

    nodeId_OPFdev[swNode->GetId()] = openFlowDev;
    // ----------openflow -----------
    // Add switch ports.
    NetDeviceContainer::Iterator it;
    for (it = swPorts.Begin(); it != swPorts.End(); it++)
    {
        NS_LOG_INFO(" Adding switch port " << *it);

        openFlowDev->AddSwitchPort(*it);
    }
    // helper index of device to device
    std::map<Ptr<NetDevice>, int> m_devices;
    for (auto i = 0; i < (int)swNode->GetNDevices(); i++)
    {
        m_devices[swNode->GetDevice(i)] = i;
    }

    for (auto i = 0; i < (int)swPorts.GetN(); i++)
    {
        NS_LOG_INFO(" Adding quality monitor to interface " << swPorts.Get(i)->GetTypeId());
        int dev_index = m_devices[swPorts.Get(i)];
        qm.Install(swNode, dev_index);
    }

    /* ----- install: devices avbw stats ------ */
    for (it = swPorts.Begin(); it != swPorts.End(); it++)
    {
        auto intKey = std::make_pair(swNode->GetId(), m_devices[(*it)]);
        // m_avbw.insert(std::make_pair(intKey, NodeStatistics(swNode, (*it))));
        m_avbw.emplace(std::piecewise_construct,
                       std::make_tuple(intKey),
                       std::make_tuple(swNode, (*it), intKey.second));
    }

    return openFlowDev;
}

Ptr<OFSwitch13Device>
HybridSDNHelper::InstallSwitch(Ptr<Node> swNode)
{
    NS_LOG_FUNCTION(this << swNode);
    NS_LOG_INFO("Installing OpenFlow device on node " << swNode->GetId());
    NS_ASSERT_MSG(!m_blocked, "OpenFlow channels already configured.");

    // ueLteDevs = m_lteHelper->InstallUeDevice (m_switchNodes); // ueLteDevs

    // // Install the TCP/IP stack into switch node.
    // m_internet.SetRoutingHelper(list);

    // // std::cout << "bla 2.1" << s  << std::endl;
    // m_internet.Install(swNode);
    // std::cout << "bla 2.2" << std::endl;

    // Create and aggregate the OpenFlow device to the switch node.
    Ptr<OFSwitch13Device> openFlowDev = m_devFactory.Create<OFSwitch13Device>();
    swNode->AggregateObject(openFlowDev);
    m_openFlowDevs.Add(openFlowDev);
    m_switchNodes.Add(swNode);

    return openFlowDev;
}

void
HybridSDNHelper::SetAddressBase(Ipv4Address network, Ipv4Mask mask, Ipv4Address base)
{
    NS_LOG_FUNCTION_NOARGS();

    m_ipv4helper.SetBase(network, mask, base);
}

void
HybridSDNHelper::EnableDatapathLogs(std::string prefix, bool explicitFilename)
{
    NS_LOG_FUNCTION_NOARGS();

    // Saving library logs into output file.
    EnableBofussLog(true, prefix, explicitFilename);
}

void
HybridSDNHelper::CreateOpenFlowChannels(void)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Creating OpenFlow channels.");
    NS_ABORT_MSG_IF(m_blocked, "OpenFlow channels already configured.");

    // Block this helper to avoid further calls to install methods.
    m_blocked = true;
    // std::cout << "is External "<<  isExternal ;
    if (isExternal)
    {
        Create_external_OpenFlowChannels();
    }
    else
    {
        Create_internal_OpenFlowChannels();
    }

    // Simulator::ScheduleNow ( &HybridSDNHelper::PeriodicCheckStats, this);
    Simulator::Schedule(Seconds(14), &HybridSDNHelper::PeriodicCheckStats, this);
}

void
HybridSDNHelper::PeriodicCheckStats()
{
    qm.CheckForOldPackets(Time("10s"));
    SendStatsMsgtoCtrl();
    Simulator::Schedule(Seconds(PERIODIC_STATS_INTERVAL),
                        &HybridSDNHelper::PeriodicCheckStats,
                        this);
}

Ptr<OFSwitch13Controller>
HybridSDNHelper::InstallController(Ptr<Node> cNode,
                                   Ptr<Node> enb,
                                   Ptr<OFSwitch13Controller> controller)
{
    NS_LOG_FUNCTION(this << cNode << controller);

    NS_LOG_INFO("Installing OpenFlow controller on node " << cNode->GetId());
    NS_ABORT_MSG_IF(m_blocked, "OpenFlow channels already configured.");

    // Install the TCP/IP stack into controller node.
    if (!cNode->GetObject<Ipv4>())
    {
        m_internet.Install(cNode);
    }
    controller->SetStartTime(Seconds(0));
    cNode->AddApplication(controller);
    m_controlApps.Add(controller);
    m_controlNode = cNode;

    switch (m_channelType)
    {
    case HybridSDNHelper::SINGLECSMA: {
        // Create the common channel for all switches and controllers.
        m_csmaChannel =
            CreateObjectWithAttributes<CsmaChannel>("DataRate", DataRateValue(m_channelDataRate));
        // Connect the controller node to the common channel and configure IP addrs.
        m_controlDevs = m_csmaHelper.Install(cNode, m_csmaChannel);
        Ipv4InterfaceContainer ctrlIface = m_ipv4helper.Assign(m_controlDevs);
        m_controlAddr = ctrlIface.GetAddress(0);
        break;
    }
    case HybridSDNHelper::SINGLELTE: {
        // Create the lte channel for all switches and controllers.
        m_lteHelper = CreateObject<LteHelper>(); // TODO aqui nao pode ser o data rate
        // internet.Install (remoteHostContainer); already done above

        // create LTE insfrastructure
        m_epcHelper = CreateObject<PointToPointEpcHelper>();
        m_lteHelper->SetEpcHelper(m_epcHelper);

        Ptr<Node> pgw = m_epcHelper->GetPgwNode();
        NS_ABORT_MSG_IF(!pgw, "pgw is empty!");

        // Create the Internet
        // CsmaHelper csmaHelper;
        m_csmaHelper.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
        m_csmaHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
        m_csmaHelper.SetDeviceAttribute("Mtu", UintegerValue(1500));
        NetDeviceContainer internetDevices =
            m_csmaHelper.Install(NodeContainer(pgw, m_controlNode));
        m_controlDevs = NetDeviceContainer(internetDevices.Get(1));
        m_pgw_csma = internetDevices.Get(0);

        NS_LOG_INFO("[ctrl ] Control Dev " << internetDevices.Get(1)->GetAddress());

        // assign ipv4 to remote controller
        Ipv4AddressHelper ipv4h;
        ipv4h.SetBase("1.0.0.0", "255.0.0.0");
        Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
        // interface 0 is localhost, 1 is the p2p device
        m_controlAddr = internetIpIfaces.GetAddress(1); // m_controlNode
        NS_LOG_INFO("[ctrl ] Control ipv4 Addr " << internetIpIfaces.GetAddress(1));

        // install eNodeB and Connect the enb node to the lte channel and configure IP addrs.
        m_enbLteDevs = m_lteHelper->InstallEnbDevice(enb);
        // Ipv4InterfaceContainer enbIface = m_ipv4helper.Assign (m_enbLteDevs);
        // Ipv4Address m_enbAddr = enbIface.GetAddress (0);

        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(m_controlNode->GetObject<Ipv4>());
        remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                                   Ipv4Mask("255.0.0.0"),
                                                   1); // cannot change network UE 7.0.0.0

        break;
    }
    case HybridSDNHelper::DEDICATEDCSMA:
    case HybridSDNHelper::DEDICATEDP2P:
    default: {
        NS_ABORT_MSG("Invalid OpenflowChannelType.");
    }
    }

    return controller;
}

Ptr<NetDevice>
HybridSDNHelper::GetPgwDev()
{
    NS_ABORT_MSG_IF((m_channelType != HybridSDNHelper::SINGLELTE),
                    "Not valid Channel Type! no LTE core found!");
    return m_pgw_csma;
}

// Ptr<NetDevice>
// HybridSDNHelper::InstallExternalController(Ptr<Node> cNode, Ptr<Node> enb)
// {
//     NS_LOG_FUNCTION(this << cNode);

//     // Ptr<Node>                 m_controlNode;      //!< OF controller node.
//     // uint16_t                  m_controlPort;      //!< OF controller TCP port.
//     // Ipv4Address               m_controlAddr;      //!< OF IP controller addr.

//     NS_LOG_INFO("Installing OpenFlow controller on node " << cNode->GetId());
//     NS_ABORT_MSG_IF(m_blocked || m_controlDevs.GetN() != 0,
//                     "OpenFlow controller/channels already configured.");
//     // local stuff
//     isExternal = true;
//     // Check for valid channel type for this helper.
//     NS_ABORT_MSG_IF((m_channelType != HybridSDNHelper::SINGLECSMA &&
//                      m_channelType != HybridSDNHelper::SINGLELTE),
//                     "Invalid channel "
//                     "type for OFSwitch13ExternalHelper (use SingleCsma or SingleLTE).");

//     NS_ABORT_MSG_IF((m_channelType == HybridSDNHelper::SINGLELTE && !enb),
//                     "Invalid enb node "
//                     " give valid enb node.");

//     // install controller
//     // Install the TCP/IP stack and the controller application into node.
//     m_internet.Install(cNode);
//     m_controlNode = cNode;
//     switch (m_channelType)
//     {
//     case HybridSDNHelper::SINGLECSMA: {
//         // Create the common channel for all switches and controllers.
//         m_csmaChannel =
//             CreateObjectWithAttributes<CsmaChannel>("DataRate",
//             DataRateValue(m_channelDataRate));
//         // Connect the controller node to the common channel and configure IP addrs.
//         m_controlDevs = m_csmaHelper.Install(cNode, m_csmaChannel);
//         Ipv4InterfaceContainer ctrlIface = m_ipv4helper.Assign(m_controlDevs);
//         m_controlAddr = ctrlIface.GetAddress(0);
//         break;
//     }
//     case HybridSDNHelper::SINGLELTE: {
//         // Create the lte channel for all switches and controllers.
//         m_lteHelper = CreateObject<LteHelper>(); // TODO aqui nao pode ser o data rate
//         // internet.Install (remoteHostContainer); already done above

//         // create LTE insfrastructure
//         m_epcHelper = CreateObject<PointToPointEpcHelper>();
//         m_lteHelper->SetEpcHelper(m_epcHelper);

//         Ptr<Node> pgw = m_epcHelper->GetPgwNode();

//         // Create the Internet
//         // CsmaHelper csmaHelper;
//         m_csmaHelper.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
//         m_csmaHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
//         m_csmaHelper.SetDeviceAttribute("Mtu", UintegerValue(1500));
//         NetDeviceContainer internetDevices =
//             m_csmaHelper.Install(NodeContainer(pgw, m_controlNode));
//         m_controlDevs = NetDeviceContainer(internetDevices.Get(1));
//         m_pgw_csma = internetDevices.Get(0);

//         NS_LOG_INFO("[ctrl ] Control Dev " << internetDevices.Get(1)->GetAddress());

//         // assign ipv4 to remote controller
//         Ipv4AddressHelper ipv4h;
//         ipv4h.SetBase("1.0.0.0", "255.0.0.0");
//         Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
//         // interface 0 is localhost, 1 is the p2p device
//         m_controlAddr = internetIpIfaces.GetAddress(1); // m_controlNode
//         NS_LOG_INFO("[ctrl ] Control ipv4 Addr " << internetIpIfaces.GetAddress(1));

//         // install eNodeB and Connect the enb node to the lte channel and configure IP addrs.
//         m_enbLteDevs = m_lteHelper->InstallEnbDevice(enb);
//         // Ipv4InterfaceContainer enbIface = m_ipv4helper.Assign (m_enbLteDevs);
//         // Ipv4Address m_enbAddr = enbIface.GetAddress (0);

//         Ipv4StaticRoutingHelper ipv4RoutingHelper;
//         Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
//             ipv4RoutingHelper.GetStaticRouting(m_controlNode->GetObject<Ipv4>());
//         remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
//                                                    Ipv4Mask("255.0.0.0"),
//                                                    1); // cannot change network UE 7.0.0.0

//         break;
//     }
//     case HybridSDNHelper::DEDICATEDCSMA:
//     case HybridSDNHelper::DEDICATEDP2P:
//     default: {
//         NS_ABORT_MSG("Invalid OpenflowChannelType.");
//     }
//     }

//     return m_controlDevs.Get(0);
// }

void
HybridSDNHelper::DoDispose()
{
    m_csmaChannel = 0;
    NS_LOG_FUNCTION(this);
}

NetDeviceContainer
HybridSDNHelper::Connect(Ptr<Node> ctrl, Ptr<Node> swtch)
{
    NS_LOG_FUNCTION(this << ctrl << swtch);

    NodeContainer pairNodes(ctrl, swtch);
    switch (m_channelType)
    {
    case HybridSDNHelper::DEDICATEDCSMA: {
        return m_csmaHelper.Install(pairNodes);
    }
    case HybridSDNHelper::DEDICATEDP2P: {
        return m_p2pHelper.Install(pairNodes);
    }
    case HybridSDNHelper::SINGLECSMA:
    default: {
        NS_ABORT_MSG("Invalid OpenflowChannelType.");
    }
    }
}

void
HybridSDNHelper::PopulateARPcacheWithSDNinterfaces(Ptr<Node> ctrl,
                                                   Ptr<NetDevice> pgw_csmaDev,
                                                   NodeContainer& opfNodes)
{
    Ptr<ArpCache> arp = CreateObject<ArpCache>();
    arp->SetAliveTimeout(Seconds(3600 * 24 * 365)); // 1 ano

    int n_dev_lte = 2;

    /*Ptr<Ipv4L3Protocol> ip = pgw->GetObject<Ipv4L3Protocol> ();
    ObjectVectorValue interfaces;
      ip->GetAttribute ("InterfaceList", interfaces);
    Ptr<Ipv4Interface> ipIface = interfaces.Get(2)->GetObject<Ipv4Interface> ();
    Ptr<NetDevice> device = ipIface->GetDevice ();

    Mac48Address lte_endPoint = Mac48Address::ConvertFrom (device->GetAddress() );
    */
    Mac48Address lte_endPoint = Mac48Address::ConvertFrom(pgw_csmaDev->GetAddress());
    std::cout << "main " << lte_endPoint << std::endl;

    for (NodeContainer::Iterator i = opfNodes.Begin(); i != opfNodes.End(); ++i)
    {
        std::cout << "Getting mac from node " << (*i)->GetId() << std::endl;

        Ptr<Ipv4> ipv4 = (*i)->GetObject<Ipv4>();
        Ipv4Address ipAddr = ipv4->GetAddress(n_dev_lte, 0).GetLocal(); // LTE is device 2
        std::cout << "Entry " << ipAddr << " - " << lte_endPoint << std::endl;
        ArpCache::Entry* entry = arp->Add(ipAddr);
        Ipv4Header ipv4Hdr;
        ipv4Hdr.SetDestination(ipAddr);
        Ptr<Packet> p = Create<Packet>(100);
        entry->MarkWaitReply(ArpCache::Ipv4PayloadHeaderPair(p, ipv4Hdr));
        entry->MarkAlive(lte_endPoint);

        /*Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
        NS_ASSERT (ip !=0);
        ObjectVectorValue interfaces;
        ip->GetAttribute ("InterfaceList", interfaces);
    std::cout << interfaces.GetN() << std::endl;

        for (ObjectVectorValue::Iterator j = interfaces.Begin (); j != interfaces.End (); j++)
        {
            Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface> ();
            NS_ASSERT (ipIface != 0);
            Ptr<NetDevice> device = ipIface->GetDevice ();
            NS_ASSERT (device != 0);
      std::cout << "\n "<< unsigned(device->GetAddress ().GetLength()) << std::endl ;
      if (device->GetAddress ().GetLength() != 6){ // mac 48==6 bytes, LTE is 64 bits== 8bytes
        continue;
      }
      //else continue to gather mac entries
            Mac48Address addr = Mac48Address::ConvertFrom (device->GetAddress () );
      std::cout << " -- mac " << addr ;

            for (uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
            {
                Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();
                if (ipAddr == Ipv4Address::GetLoopback ())
                    continue;
        std::cout << " "  << ipAddr <<std::endl;
                ArpCache::Entry *entry = arp->Add (ipAddr);
                Ipv4Header ipv4Hdr;
                ipv4Hdr.SetDestination (ipAddr);
                Ptr<Packet> p = Create<Packet> (100);
                entry->MarkWaitReply (ArpCache::Ipv4PayloadHeaderPair (p, ipv4Hdr));
                entry->MarkAlive (addr);
            }
        }
    */
    }

    // Ipv4Address ipAddr("7.0.0.2");
    // Mac48Address addr("00:00:00:00:00:06"); // od a interface seguinte pois é o endpoint para
    // o csma ArpCache::Entry *entry = arp->Add (ipAddr); Ipv4Header ipv4Hdr;
    // ipv4Hdr.SetDestination (ipAddr); Ptr<Packet> p = Create<Packet> (100);
    // entry->MarkWaitReply (ArpCache::Ipv4PayloadHeaderPair (p, ipv4Hdr)); entry->MarkAlive
    // (addr);

    // Ipv4Address ipAddr2("7.0.0.3");
    // Mac48Address addr2("00:00:00:00:00:06"); // od a interface seguinte pois é o endpoint
    // para o csma ArpCache::Entry *entry2 = arp->Add (ipAddr2); Ipv4Header ipv4Hdr2;
    // ipv4Hdr.SetDestination (ipAddr2);
    // Ptr<Packet> p2 = Create<Packet> (100);
    // entry2->MarkWaitReply (ArpCache::Ipv4PayloadHeaderPair (p2, ipv4Hdr2));
    // entry2->MarkAlive (addr2);

    NodeContainer c = NodeContainer(ctrl);
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol>();
        NS_ASSERT(ip != nullptr);
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);

        for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++)
        {
            Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface>();
            ipIface->SetAttribute("ArpCache", PointerValue(arp));
        }
    }

    std::cout << "END populating " << std::endl;
}

} // namespace ns3
#endif // NS3_OFSWITCH13