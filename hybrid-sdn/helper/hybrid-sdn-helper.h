/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef HYBRID_SDN_HELPER_H
#define HYBRID_SDN_HELPER_H

#include "ns3/config-store-module.h"
#include "ns3/config.h" // config::connect
#include "ns3/exp-msg-data-helper.h"
#include "ns3/node_stats.h"
#include "ns3/quality-monitor.h"
#include "ns3/wifi-net-device.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-phy.h"
#include <ns3/application-container.h>
#include <ns3/csma-helper.h>
#include <ns3/hybrid-sdn-controller.h>
#include <ns3/inet-socket-address.h>
#include <ns3/internet-stack-helper.h>
#include <ns3/ipv4-address-helper.h>
#include <ns3/ipv4-interface-container.h>
#include <ns3/ipv4-list-routing-helper.h>
#include <ns3/lte-helper.h>
#include <ns3/lte-module.h>
#include <ns3/names.h>
#include <ns3/node-container.h>
#include <ns3/object-factory.h>
#include <ns3/ofswitch13-controller.h>
#include <ns3/ofswitch13-device-container.h>
#include <ns3/ofswitch13-device.h>
#include <ns3/ofswitch13-interface.h>
#include <ns3/olsr-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/point-to-point-module.h>
#include <ns3/simple-ref-count.h>

#include <map>
#include <string>

#define PERIODIC_STATS_INTERVAL 1

namespace ns3
{

class Node;
class AttributeValue;
class OFSwitch13Controller;
class OFSwitch13LearningController;

// class quality monitor

/**
 * @brief This helper extends the base class and can be instantiated to create and
 * configure an OpenFlow 1.3 network domain composed of one or more OpenFlow +  OLSR
 * devices connected to a single or multiple internal simulated OpenFlow
 * controllers. It brings methods for installing the controller and creating
 * the OpenFlow channels.
 ** This is a base class that must be extended to create and configure an
 * OpenFlow 1.3 network domain composed of one or more OpenFlow switches
 * connected to single or multiple OpenFlow controllers.
 *
 * By default, the connections between switches and controllers are created
 * using a single shared out-of-band CSMA channel, with IP addresses assigned
 * using a /24 network mask. Users can modify this configuration by changing
 * the ChannelType attribute at instantiation time. Dedicated out-of-band
 * connections over CSMA or Point-to-Point channels are also available, using a
 * /30 network mask for IP allocation.
 *
 * Please note that this base helper class was designed to configure a single
 * OpenFlow network domain. All switches will be connected to all controllers
 * on the same domain. If you want to configure separated OpenFlow domains on
 * your network topology (with their individual switches and controllers) so
 * you may need to use a different instance of the derived helper class for
 * each domain. In this case, don't forget to use the SetAddressBase ()
 * method to change the IP network address of the other helper instances, in
 * order to avoid IP conflicts.
 *
 *
 * \attention This helper creates an LTE + CSMA OpenFlow channel  || the default csma from the
 *ofswitch13 module \attention switch ports are wifi or csma (TODO) \attention Only allows for one
 *controller
 *
 */
class HybridSDNHelper : public Object
{
  public:
    /**
     * OpenFlow channel type, used to create the connections.
     * between controllers and switches.
     */
    enum ChannelType
    {
        SINGLECSMA = 0,    //!< Uses a single shared CSMA channel.
        DEDICATEDCSMA = 1, //!< Uses individual CSMA channels.
        DEDICATEDP2P = 2,  //!< Uses individual P2P channels.
        SINGLELTE = 3      //!< Uses a single shared lte channel.
    };

    HybridSDNHelper();
    HybridSDNHelper(InternetStackHelper& int_stack);
    virtual ~HybridSDNHelper();

    /**
     * @brief Populate Arp table on remoteHost of LTE example with the sdnDev(FD) control port
     * (ip 7.0.0.0/...) Adapted from :
     * https://gist.github.com/SzymonSzott/de5c431d687f7b3a0b10743af6ac7ce2
     *
     * @param ctrl RemoteHost node, that have a CSMA interface to communicate to the LTE core;
     * @param pgw_csmaDev NetDevice type CSMA in the PGW node (LTE core);
     * @param opfNodes Nodes that are sdnDev(FD) : (need to talk to the controller).
     */
    static void PopulateARPcacheWithSDNinterfaces(Ptr<Node> ctrl,
                                                  Ptr<NetDevice> pgw_csmaDev,
                                                  NodeContainer& opfNodes);

    /**
     * Register this type.
     * \return The object TypeId.
     */
    static TypeId GetTypeId(void);

    /**
     * Set an attribute on each OpenFlow device created by this helper.
     *
     * \param n1 the name of the attribute to set.
     * \param v1 the value of the attribute to set.
     */
    void SetDeviceAttribute(std::string n1, const AttributeValue& v1);

    /**
     * Set the OpenFlow channel type used to create the connections between
     * switches and controllers.
     *
     * \param type The ChannelType to use.
     */
    virtual void SetChannelType(ChannelType type);

    /**
     * Set the OpenFlow channel data rate used to create the connections between
     * switches and controllers.
     *
     * \param rate The channel data rate to use.
     */
    virtual void SetChannelDataRate(DataRate rate);

    /**
     * Enable pacp traces at OpenFlow channel between controller and switches.
     *
     * \attention Call this method only after configuring the OpenFlow channels.
     *
     * \param prefix Filename prefix to use for pcap files.
     * \param promiscuous If true, enable promisc trace.
     */
    void EnableOpenFlowPcap(std::string prefix = "ofchannel", bool promiscuous = true);

    /**
     * Enable ASCII traces at OpenFlow channel between controller and switches.
     *
     * \attention Call this method only after configuring the OpenFlow channels.
     *
     * \param prefix Filename prefix to use for ascii files.
     */
    void EnableOpenFlowAscii(std::string prefix = "ofchannel");

    /**
     * Enable OpenFlow datapath statistics at OpenFlow switch devices configured
     * by this helper. This method will create an OFSwitch13StatsCalculator for
     * each switch device, dumping statistcs to output files.
     *
     * \attention Call this method only after configuring the OpenFlow channels.
     *
     * \param prefix Filename prefix to use for stats files.
     * \param useNodeNames Use node names instead of datapath id.
     */
    void EnableDatapathStats(std::string prefix = "datapath", bool useNodeNames = false);

    /**
     * @brief This method creates an OpenFlow device and aggregates it to the switch
     * node. It also attaches the given devices as physical ports on the switch.
     * If no devices are given, the switch will be configured without ports. In
     * this case, don't forget to add ports to it later, or it will do nothing.
     *
     * @param swNode The switch node where to install the OpenFlow device.
     * @param swPorts  Container of devices to be added as physical switch ports.
     * @return Ptr<OFSwitch13Device> The OpenFlow device created.
     */
    Ptr<OFSwitch13Device> InstallSwitch(Ptr<Node> swNode, NetDeviceContainer& swPorts);

    /**
     * @brief This method creates an OpenFlow device and aggregates it to the switch
     * node. The switch configured by this method will have no switch ports.
     * Don't forget to add ports do it later, or it will do nothing.
     *
     * @param swNode The switch node where to install the OpenFlow device.
     * @return Ptr<OFSwitch13Device> The OpenFlow device created.
     */
    Ptr<OFSwitch13Device> InstallSwitch(Ptr<Node> swNode);

    /**
     * This virtual method must interconnect all switches to all controllers
     * installed by this helper and starts the individual OpenFlow channel
     * connections.
     * \attention After calling this method, it will not be allowed to install
     *            more switches or controller using this helper.
     * \attention This function call other private ones that actually creates the openflow channels
     */
    void CreateOpenFlowChannels(void);

    /**
     * This method prepares the controller node so it can be used to connect
     * internal simulated switches to an external OpenFlow controller running on
     * the local machine over a TapBridge device. It installs the TCP/IP stack
     * into controller node, attach it to the common CSMA channel and configure
     * IP address for it.
     *
     * \param cNode The node to configure as the controller.
     * \return The network device to bind to the TapBridge.
     */
    // Ptr<NetDevice> InstallExternalController(Ptr<Node> cNode, Ptr<Node> enb = NULL);

    /**
     * Set the IP network base address, used to assign IP addresses to switches
     * and controllers during the CreateOpenFlowChannels () procedure.
     *
     * \param network The Ipv4Address containing the initial network number to
     *        use during allocation.
     * \param mask The Ipv4Mask containing one bits in each bit position of the
              network number.
     * \param base An optional Ipv4Address containing the initial address used
     *        for IP address allocation.
     */
    static void SetAddressBase(Ipv4Address network, Ipv4Mask mask, Ipv4Address base = "0.0.0.1");

    /**
     * Enable OpenFlow datapath logs at all OpenFlow switch devices on the
     * simulation. This method will enable vlog system at debug level on the
     * ofsoftswitch13 library, dumping messages to output file.
     *
     * \param prefix Filename prefix to use for log file.
     * \param explicitFilename Treat the prefix as an explicit filename if true.
     */
    static void EnableDatapathLogs(std::string prefix = "", bool explicitFilename = false);

    /**
     * This method installs the given controller application into the given
     * controller node. If no application is given, a new (default) learning
     * controller application is created and installed into controller node.
     *
     * \param cNode The node to configure as controller.
     * \param controller The controller application to install into cNode
     * \return The installed controller application.
     */
    Ptr<OFSwitch13Controller> InstallController(
        Ptr<Node> cNode,
        Ptr<Node> enb,
        Ptr<OFSwitch13Controller> controller = CreateObject<HybridSDNController>());

    /**
     * @brief sending experimenter message to pipeline
     *
     */
    void SendStatsMsgtoCtrl();

    /**
     * @brief Remainging energy in device
     *
     * @param oldValue
     * @param remainingEnergy
     */
    void RemainingEnergy(std::string context, double oldValue, double remainingEnergy);

    void RoutingTableChanged(std::string context, uint32_t size);
    // int GetDataLength(Ptr<OFSwitch13Device> t_dev,
    //                   std::map<Ipv4Address, std::set<Ipv4Address>> table);
    void SendExperimenterMsgFromDev(Ptr<OFSwitch13Device> t_dev,
                                    std::map<Ipv4Address, std::set<Ipv4Address>> table,
                                    uint32_t exp_type);

    /**
     * @brief Get the Pgw Dev object in case LTE is on
     *
     * @return Ptr<NetDevice> | null
     */
    Ptr<NetDevice> GetPgwDev();

    // quality monitor
    QualityMonitor qm; // Quality monitor for the simulation

    // node state - avbw TODO
    std::map<std::pair<int, int>, NodeStatistics> m_avbw; // (nodeId, devId)--> NodeStatistics

  protected:
    /** Destructor implementation. */
    virtual void DoDispose();
    void PeriodicCheckStats();

    ChannelType m_channelType;  //!< OF channel type.
    DataRate m_channelDataRate; //!< OF channel data rate.
    ObjectFactory m_devFactory; //!< OF device factory.
    bool m_blocked;             //!< Block this helper.

    NetDeviceContainer m_controlDevs;         //!< OF channel ctrl devices.
    OFSwitch13DeviceContainer m_openFlowDevs; //!< OF switch devices.
    NodeContainer m_switchNodes;              //!< OF switch nodes.

    InternetStackHelper m_internet; //!< Helper for TCP/IP stack.

    CsmaHelper m_csmaHelper;        //!< Helper for CSMA links.
    PointToPointHelper m_p2pHelper; //!< Helper for P2P links.
    Ptr<LteHelper> m_lteHelper;     //!< Helper for LTE links.

    static Ipv4AddressHelper m_ipv4helper; //!< Helper for IP address.

    NetDeviceContainer m_enbLteDevs; //!< OF channel enb devices.
    Ptr<PointToPointEpcHelper> m_epcHelper;
    NetDeviceContainer ueLteDevs;
    uint32_t m_num_lte_iface;
    Ptr<NetDevice> m_pgw_csma;

    // pointer to ns3 node agregated to this device class
    std::map<Ptr<OFSwitch13Device>, Ptr<olsr::RoutingProtocol>> rtg; // legacy routing

    std::map<int, Ptr<OFSwitch13Device>> nodeId_OPFdev;
    std::map<Ptr<OFSwitch13Device>, double> energy;

  private:
    /**
     * Create an individual connection between the switch and the controller
     * node, using the already configured channel type.
     *
     * \param ctrl The controller node.
     * \param swtch The switch node.
     * \return The devices created on both nodes.
     */
    NetDeviceContainer Connect(Ptr<Node> ctrl, Ptr<Node> swtch);

    /**
     * @brief returns the set of neighbours for a specific ip
     *
     * @param list
     * @param ip
     * @return std::set<Ipv4Address>
     */
    std::set<Ipv4Address> ProcessNeighbours(std::set<Ipv4Address> list, Ipv4Address ip);

    /**
     * These methods connect all switches to the controller.
     */
    void Create_external_OpenFlowChannels(void);
    void Create_internal_OpenFlowChannels(void);

    ApplicationContainer m_controlApps; //!< OF controller apps.
    // NodeContainer             m_controlNodes;     //!< OF controller nodes.

    BasicEnergySourceHelper basicSourceHelper;

    // from external helper
    Ptr<CsmaChannel> m_csmaChannel; //!< Common CSMA channel.
    Ptr<Node> m_controlNode;        //!< OF controller node.
    uint16_t m_controlPort;         //!< OF controller TCP port.
    Ipv4Address m_controlAddr;      //!< OF IP controller addr.

    bool isExternal = false;
    Time lastExperimenter = Simulator::Now();
};

} // namespace ns3

#endif /* HYBRID_SDN_HELPER_H */
