#ifndef LOGGING_H_
#define LOGGING_H_

#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/node-probe.h"
#include "ns3/olsr-module.h"
#include "ns3/simulator.h"
#include <ns3/core-module.h>

#include <iostream>
#include <mutex>
#include <string>

#define PERIODIC_CHECK_INTERVAL (Seconds(1))

namespace ns3
{
using namespace olsr;

// class Node;

class Logging : public Object
{
  public:
    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId() const;
    Logging();
    // virtual ~Logging();

    mutable std::mutex monitor_mutex;

    //     // --- methods to be used by the FlowMonitorNodeProbe's only ---
    //   /// Register a new FlowNodeProbe that will begin monitoring and report
    //   /// events to this monitor.  This method is normally only used by
    //   /// FlowNodeProbe implementations.
    //   /// \param probe the probe to add
    //   void AddNodeProbe (Ptr<NodeProbe> probe);

    //   /// FlowNodeProbe implementations are supposed to call this method to
    //   /// report that a new packet was transmitted (but keep in mind the
    //   /// distinction between a new packet entering the system and a
    //   /// packet that is already known and is only being forwarded).
    //   /// \param probe the reporting probe
    //   /// \param flowId flow identification
    //   /// \param packetId Packet ID
    //   /// \param packetSize packet size
    //   void ReportFirstTx (Ptr<NodeProbe> probe, FlowId flowId, FlowPacketId packetId, uint32_t
    //   packetSize);
    //   /// FlowNodeProbe implementations are supposed to call this method to
    //   /// report that a known packet is being forwarded.
    //   /// \param probe the reporting probe
    //   /// \param flowId flow identification
    //   /// \param packetId Packet ID
    //   /// \param packetSize packet size
    //   void ReportForwarding (Ptr<NodeProbe> probe, FlowId flowId, FlowPacketId packetId, uint32_t
    //   packetSize);
    //   /// FlowNodeProbe implementations are supposed to call this method to
    //   /// report that a known packet is being received.
    //   /// \param probe the reporting probe
    //   /// \param flowId flow identification
    //   /// \param packetId Packet ID
    //   /// \param packetSize packet size
    //   void ReportLastRx (Ptr<NodeProbe> probe, FlowId flowId, FlowPacketId packetId, uint32_t
    //   packetSize);
    //   /// FlowNodeProbe implementations are supposed to call this method to
    //   /// report that a known packet is being dropped due to some reason.
    //   /// \param probe the reporting probe
    //   /// \param flowId flow identification
    //   /// \param packetId Packet ID
    //   /// \param packetSize packet size
    //   /// \param reasonCode drop reason code
    //   void ReportDrop (Ptr<NodeProbe> probe, FlowId flowId, FlowPacketId packetId,
    //                    uint32_t packetSize, uint32_t reasonCode);

    // ---- methods for results
    std::string log_path = "log.csv";
    // functions

    void SaveToFile();
    void PeriodicSave();
    void AddProbe(Ptr<NodeProbe> p);
    void saveNodeStats(Ptr<NodeProbe> p, int seconds);

    /// Structure to represent a single tracked packet data
    struct TrackedPacket
    {
        Time firstSeenTime;      // pktTimeSend
        Time lastSeenTime;       // pktTimeReceive
        uint32_t timesForwarded; // numHops
    };

    typedef std::map<uint64_t, TrackedPacket> TrackedPacketMap;

    TrackedPacketMap m_trackedPackets;
    std::map<uint64_t, Ptr<olsr::RoutingProtocol>> rtg; // legacy routing

  protected:
    virtual void NotifyConstructionCompleted();
    virtual void DoDispose(void);

  private:
    // variables
    uint32_t log_id = 0;

    // (packet uid) --> TrackedPacket

    std::vector<Ptr<NodeProbe>> m_probes; // NodeProbeContainer
};

}; // namespace ns3
#endif /* LOGGING_H_ */