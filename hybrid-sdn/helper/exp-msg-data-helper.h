#ifndef EXP_MSG_HELPER_H
#define EXP_MSG_HELPER_H

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/pointer.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include <ns3/wifi-net-device.h>

#include <cmath>
#include <iomanip> // std::setbase
#include <iostream>
#include <map>
using namespace ns3;

class ExpMsgData
{
  private:
    /* data */
  public:
    ExpMsgData(/* args */);
    ~ExpMsgData();
    /**
     * @brief serilize to uint8_t array
     *
     * @param out_data
     * @return int length of the malloc on out_data
     */
    int Serialize(uint8_t*& out_data);

    friend std::ostream& operator<<(std::ostream& os, const ExpMsgData& dt);

    struct port
    {
        uint8_t type;      // 1
        int port;          // 4
        Ipv4Address ipv4;  // 4
        Mac48Address mac;  // 6
        uint32_t avbw = 0; // bps //4
        uint32_t maxC = 0; // 4
    };

    struct neig
    {
        Ipv4Address ipv4; // 4B
        // link information
        uint8_t u_delivery_ratio;
        int avgDelay_us;
        uint8_t avgThroughput;
        uint8_t avgSnr; // [0,100]%
    };

    int neig_size = 17;
    int port_size = 27; // 23 +  length (4B)

    uint8_t energy;
    std::vector<struct port> ports;
    std::map<int, std::vector<struct neig>> oneHopNeighs;
};

#endif