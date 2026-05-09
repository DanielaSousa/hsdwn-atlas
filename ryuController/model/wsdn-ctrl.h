#ifndef WSDN_CONTROLLER_H
#define WSDN_CONTROLLER_H

#include "wsdn-ctrl-abstract.h"

/**
 * @brief A Learning OpenFlow 1.3 controller for wireless networks
 *
 */
class WSDNController : public WSDNCtrlAbstract
{
  public:
    /**
     * Register this type.
     * \return The object TypeId.
     */
    static TypeId GetTypeId();

    /** Destructor implementation */
    void DoDispose() override;

    WSDNController(std::string ryuconfig);
    ~WSDNController();

    pOutRule Handle_Ipv4(uint64_t dpId,
                         uint8_t ip_proto,
                         struct ofl_msg_packet_in* msg,
                         Mac48Address src48,
                         Ipv4Address srcIp,
                         Mac48Address dst48,
                         Ipv4Address dstIp);
    void RecalculatePaths();
    int getServiceType(struct ofl_msg_packet_in* msg);

  private:
    /* data */
    // std::map<uint64_t, std::vector<FlowKey_t>> m_dev_paths;
    std::set<FlowKey_t> m_flowKeys;
};
#endif