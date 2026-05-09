#include "path.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PathlUtils");

MultiPath::MultiPath(/* args */)
{
}

MultiPath::~MultiPath()
{
}

int
MultiPath::getGroupId(uint64_t sw)
{
    return this->hopSw[sw].group_id;
}

void
MultiPath::insertRules()
{
}

std::set<Hop_t, decltype(compareKey)>
MultiPath::ParseStringPaths(std::string ss)
{
    std::set<Hop_t, decltype(compareKey)> edges;
    std::cout << "string -> " << ss << std::endl;

    // TODO parse serialized path
    if (ss == "")
    {
        return edges;
    }
    // string delimeter
    size_t pos = 0;
    std::string delimiter = "|";
    std::string token;

    std::string delimiter2 = " ";
    size_t pos2 = 0;

    std::string delimiter3 = ",";
    size_t pos3 = 0;
    size_t val = 0;

    // example: "1 00:00:00:00:00:04 1,2 00:00:00:00:00:06 1,4 00:00:00:00:00:02 1|1
    // 00:00:00:00:00:05 1,3 00:00:00:00:00:06 1,4 00:00:00:00:00:02 1|"
    while ((pos = ss.find(delimiter)) != std::string::npos)
    {
        token = ss.substr(0, pos);
        std::cout << "TOKEN: " << token << std::endl;
        int sw_id = -1;
        int port = -1;
        Mac48Address next_sw;

        std::vector<Hop_t> p;
        while ((pos3 = token.find(delimiter3)) != std::string::npos)
        {
            std::string s = token.substr(0, pos3);
            std::cout << "EDGE: " << s << std::endl;
            if ((pos2 = s.find(delimiter2)) != std::string::npos)
            {
                std::string sw = s.substr(val, pos2 - val);
                if (sw == "None")
                {
                    token.erase(0, pos3 + delimiter3.length());
                    continue; // not a switch , so no OpenLfow mod can be sent to this device
                }
                std::cout << "Sw " << sw << std::endl;
                sw_id = std::stoi(sw);
                val = pos2 + delimiter2.length();
                // cout << "NEW val: " << val << endl;
                if ((pos2 = s.find(delimiter2, val)) != std::string::npos)
                {
                    next_sw = Mac48Address(s.substr(val, pos2).c_str());
                    std::cout << "MAC: " << next_sw << std::endl;
                    val = pos2 + delimiter2.length();
                    // cout << "NEW val: " << val << endl;
                    if (val < s.length())
                    {
                        std::cout << "PORT " << s.substr(val, pos2 - val) << std::endl;
                        port = stoi(s.substr(val, pos2 - val));

                        if (sw_id != -1 && !next_sw.IsNull() && port != -1)
                        {
                            // cout << sw_id << " " << next_sw << " " << port <<endl;
                            Hop_t tmp_edge = {sw_id, next_sw, port};
                            p.push_back(tmp_edge);
                        }
                        else
                        {
                            p.clear(); // delete all edges for this path, is not a valid path
                            break;
                            NS_LOG_ERROR(
                                "something went wrong when calling function get_multi_path");
                        }
                    }
                }
            }
            token.erase(0, pos3 + delimiter3.length());
        }
        for (Hop_t x : p)
        {
            edges.insert(x); // add path to edges to process afterwards
        }

        ss.erase(0, pos + delimiter.length());
    }

    return edges;
}

bool
MultiPath::CheckComplience(uint64_t sw_id, Mac48Address prev)
{
    // 1. find sw_id in paths
    if (pathHops.find(sw_id) == pathHops.end())
        return false;
    // return if prev is equal do saved previous mac address
    for (auto it = pathHops[sw_id].begin(); it != pathHops[sw_id].end(); it++)
    {
        if (it->prevMac == prev)
            return true;
    }
    return false;
}