
#include "exp-msg-data-helper.h"
using namespace ns3;

ExpMsgData::ExpMsgData(/* args */)
{
}

ExpMsgData::~ExpMsgData()
{
}

std::ostream&
operator<<(std::ostream& os, const ExpMsgData& dt)
{
    os << "Experimeter message Data:\n"
       << "Energy:" << (int)dt.energy << "\n";
    for (ExpMsgData::port i : dt.ports)
    {
        os << "Port " << i.port << " (" << (int)i.type << ") : (" << i.mac << ") - " << i.ipv4
           << " : " << (int)i.avbw << " bps \n";
        for (ExpMsgData::neig k : dt.oneHopNeighs.at(i.port))
        {
            os << "\tNeig: " << k.ipv4 << ": snr " << (int)k.avgSnr << " , delivery "
               << (int)k.u_delivery_ratio << " , delay " << (int)k.avgDelay_us << " , throughput "
               << (int)k.avgThroughput << "\n";
            // TODO information
        }
    }
    return os;
}

// link information
uint8_t u_delivery_ratio;
int avgDelay_us;
uint8_t avgThroughput;
uint8_t avgSnr; // [0,100]%

int
ExpMsgData::Serialize(uint8_t*& data)
{
    // energy + ()*ports.size + ()*neighs.size
    int sum = 0;
    for (const auto& [key, val] : oneHopNeighs)
        sum += val.size();
    int length = 1 + port_size * this->ports.size() +
                 neig_size * sum; // energy + interface record + neighbour

    data = (uint8_t*)malloc(length);

    data[0] = energy;

    // SDN ports mac and ipv4 address
    size_t l = 1;
    // uint8_t i_port = 1;// port no (starts at 1).
    // for (auto i = m_ports.begin(); i != m_ports.end(); ++i){
    for (auto& element : ports)
    {
        // std::cout << " PORT " << (port_size + oneHopNeighs[element.port].size() * neig_size)
        //           << std::endl;
        //  LENGTH 1byte
        //  data[l] = (uint8_t)(port_size + oneHopNeighs[element.port].size() * neig_size);

        int port_length = port_size + oneHopNeighs[element.port].size() * neig_size;
        memcpy(&data[l], &port_length, sizeof(port_length));

        data[l + 4] = element.type;
        memcpy(&data[l + 5], &element.port, sizeof(element.port));
        element.ipv4.Serialize(data + l + 5 + 4);
        element.mac.CopyTo(data + l + 5 + 4 + 4);
        l += 4 + 1 + 4 + 4 + 6;                                // length + type + port + ipv4 + mac
        memcpy(&data[l], &element.avbw, sizeof(element.avbw)); // avbw
        // Print copied bytes in hex format
        /*std::cout << "Copied bytes: " << element.avbw << " --> ";
        for (int i = 0; i < 4; i++)
        {
            printf("%02X ", data[l + i]);
        }
        std::cout << std::endl;*/

        l += 4;
        // maxC (4B)
        memcpy(&data[l], &element.maxC, sizeof(element.maxC));
        /*std::cout << "Copied maxC: " << element.maxC << " --> ";
        for (int i = 0; i < 4; i++)
            printf("%02X ", data[l + i]);
        std::cout << std::endl;*/
        l += 4;

        /*printf(" EXPERIMENTER node avbw %d, maxC %d, type %d \n",
               element.avbw,
               element.maxC,
               element.type);
*/
        for (auto it2 = oneHopNeighs[element.port].begin(); it2 != oneHopNeighs[element.port].end();
             it2++)
        {
            (*it2).ipv4.Serialize(data + l);
            l += 4; // ipv4 - 4byte

            int delivery = static_cast<int>((*it2).u_delivery_ratio);
            int throughput = static_cast<int>((*it2).avgThroughput);
            memcpy(&data[l], &delivery, sizeof(int)); // Delivery (4 bytes)
            memcpy(&data[l + 4], &(*it2).avgDelay_us, sizeof((*it2).avgDelay_us)); // Delay
            memcpy(&data[l + 8], &throughput, sizeof(int)); // Throughput (4 bytes)
            data[l + 12] = (*it2).avgSnr;                   // snr

            l += 13;
        }
    }
    NS_ASSERT_MSG((int)l == length, "size not the same" << l << " != " << length);

    // for (auto i = 0; i < length; i++)
    // {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0')
    //               << (int)static_cast<unsigned char>(data[i]) << ' ';
    // }
    // std::cout << std::endl;
    return length;
}