#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include "ns3/core-module.h"
#include <ns3/mac48-address.h>

#include <map>
#include <set>
#include <string.h>
#include <tuple>

struct Hop_t
{
    uint64_t swId;
    Mac48Address nextMac;
    uint32_t portOut;
    Mac48Address prevMac;
};

bool
operator==(Hop_t& lhs, Hop_t& rhs)
{
    if (lhs.swId == rhs.swId)
    {
        if (lhs.nextMac == rhs.nextMac)
        {
            if (lhs.portOut == rhs.portOut)
            {
                if (lhs.prevMac == rhs.prevMac)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

struct FlowKey_t
{
    Ipv4Address src, dst;
    int service;
};

ostream&
operator<<(ostream& os, const FlowKey_t& key)
{
    return os << "Ip: ( " << key.src << "--> " << key.dst << ", Service: " << key.service
              << std::endl;
}

bool
compareKey(Hop_t i1, Hop_t i2)
{
    return (i1.swId < i2.swId);
}

struct RuleInfo_t
{
    Mac48Address nextMac;
    uint32_t portOut;
    int prio;
    std::string actions;
};

struct swRule_t

{
    int group_id = -1;
    std::vector<RuleInfo_t> out_rules; // dstMac, out_port, prioCounter, actions.str()
};

class MultiPath
{
  private:
    /* data */
  public:
    int k = 1;         // default 1 path
    int disjperc = 70; // TODO
    std::string match;

    std::map<uint64_t, swRule_t> hopSw; // each hop is saved : hopSw[sw_id] --> group_id, out_rules

    std::set<Hop_t, decltype(compareKey)> hops; // {edge, edge, edge}

    uint64_t firstDev; // problem!!! when legacy devices do not comply

    MultiPath(/* args */);
    ~MultiPath();

    /**
     * @brief Get the Group Id object given a switch
     *
     * @param dst
     * @param src
     * @param sw
     * @return int
     */
    int getGroupId(uint64_t sw);

    void insertRules();

    void printFlow();

    std::set<Hop_t, decltype(compareKey)> ParseStringPaths(std::string p);

    std::map<uint64_t, std::vector<Hop_t>> pathHops;

    /**
     * @brief Check if package was sent by the path's previous node or not
     *
     * @param sw_id
     * @param prev
     * @return true
     * @return false
     */
    bool CheckComplience(uint64_t sw_id, Mac48Address prev);
};

class Path : public MultiPath
{
}

#endif