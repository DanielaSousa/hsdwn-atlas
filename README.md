# Simulation Framework for Hybrid Software-Defined Wireless Networks and ATLAS

This repository contains the simulation framework developed for the thesis:

> **An adaptive and nomadic multi-technology network for critical scenarios**

The framework is built on top of ns-3 and the ofswitch13 module, extending them with support for **Hybrid Software-Defined Wireless Networks (HSDWN)** and the **ATLAS routing framework**.

It enables the evaluation of spontaneous post-disaster communication networks composed of:

- Hybrid SDN-capable wireless nodes
- Legacy ad hoc nodes
- Multi-technology wireless environments
- SDN/OpenFlow control planes over LTE
- QoS-aware routing and traffic prioritization
- Passive bandwidth estimation
- Dynamic topology monitoring and adaptation

The framework was designed to support dynamic emergency scenarios where conventional infrastructure is unavailable or partially destroyed.

---

## Related Publications

This repository accompanies the research contributions presented in:

- Sousa, D., Sargento, S., Luís, M., ``Hybrid wireless network with sdn and legacy devices in ad-hoc environments,'' in 2022 13th international conference on network of the future (NoF), IEEE. pp. 1–9, 2022.
    
- Sousa, D., Sargento, S., Luís, M., ``A simulation environment for software defined wireless networks with legacy devices,'' in Proceedings of the 18th ACM International Symposium on QoS and Security for Wireless and Mobile Networks, pp. 1–10, 2022.
    
- Sousa, D., Sargento, S., Luís, M., ``On the optimal integration of legacy and SDWN communication nodes in emergency scenarios,'' in Springer Wireless Networks 31, 2275–2292, 2024.
    
- Sousa, D., Sargento, S., Luís, M., ``Passive estimation of available bandwidth in heterogeneous ad hoc networks,'' in 2025 IEEE 26th International Symposium on a World of Wireless, Mobile and Multimedia Networks (WoWMoM), IEEE. pp. 259–265, 2025.
    
- Sousa, D., Sargento, S., Luís, M., ``ATLAS, an Adaptive Multipath Routing for QoS Provisioning in Heterogeneous Software Defined Emergency Wireless Networks'', submitted to Ad Hoc Networks (Elsevier).

  
and the associated thesis:

> **An adaptive and nomadic multi-technology network for critical scenarios**

---
## Environment Overview

The framework implements a three-plane architecture:

| Plane | Description | Language |
|---|---|---|
| Data Plane | Wi-Fi ad hoc network, Monitoring, estimation |c++|
| Control Plane | LTE/OpenFlow communication |c++|
| Management Plane | TDM, QoS, ATLAS |python|

---

## Tutorial — Creating a Simulation Scenario

This section demonstrates the typical workflow used to configure a hybrid SDN emergency scenario.

### 1. Create Hybrid and Legacy Nodes

```cpp
NodeContainer sdnNodes;
NodeContainer legacyNodes;

sdnNodes.Create(10);
legacyNodes.Create(5);
```
Afterwards, create the mobility patterns.

### 2. Configure Control Plane

```cpp
InternetStackHelper controlPlaneInternetStack;
Ptr<HybridSDNHelper> of13Helper = CreateObject<HybridSDNHelper>(controlPlaneInternetStack);

Ptr<OfppPythonController> sdnCtrl = CreateObject<OfppPythonController>(folder);
// creates the LTE layer with EnodeB enb, the sdn controller bridged with RemoteHost, and the instance for the sdn controller
of13Helper->InstallController(RemoteHost, enb, sdnCtrl);

```
There are 2 controller types:
- OfppNormalController : sends all packets to be forwarded by the legacy routing mechanism
- OfppPythonController : next hop is decided by ATLAS

### 3. Install Legacy Routing Protocol

```cpp
OlsrHelper olsr;
InternetStackHelper internet;
// create OLSR protocol stack
Ipv4ListRoutingHelper list;
Ipv4StaticRoutingHelper staticRouting;
list.Add(staticRouting, 0);
list.Add(olsr, 10);
internet.SetRoutingHelper(list);
// install the ad-hoc legacy routing protocol in all nodes
internet.Install(sdnNodes);
internet.Install(legacyNodes);
```

### 4. Configure Data Plane

4.1 Assign which technology is in every node. More than one technology can be added.
For example 2 technologies: ```NodeContainer tech1, tech2;``` add the nodes to these NodeContainers.

```cpp
YansWifiPhyHelper wifiPhy;
std::vector<NetDeviceContainer> switchPorts(opfNodes.GetN());
WirelessConfigs wc;

NetDeviceContainer l1 = wc.configNet_80211ax(NodeContainer(tech1));
NetDeviceContainer l2 = wc.configNet_80211a(NodeContainer(tech2));

AssignDevicesToSwitchPorts(opfNodes, tech1, tech2, l1, l2, switchPorts);

```
4.2 Assign IPV4 for the 2 technologies

4.3 Install the wireless interfaces as switch ports:

```cpp
for (size_t i = 0; i < opfNodes.GetN(); i++)
{
    of13Helper->InstallSwitch(opfNodes.Get(i),
                              switchPorts.at(i));
    // it already installs the monitoring tool
}
```

### 5. Create the openFlow Channels

```cpp
  of13Helper->CreateOpenFlowChannels();
```

### 6. NS-3 setup

 6.1 Add flows.
 6.2 Add FlowMonitors if needed.
 6.3 Define simulation  time.
 6.4 Start the simulation.




