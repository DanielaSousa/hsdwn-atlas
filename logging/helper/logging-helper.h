#ifndef LOGGING_HELPER_H_
#define LOGGING_HELPER_H_

#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/logging.h"
#include "ns3/node-container.h"

#include <string>

namespace ns3
{

class LoggingHelper
{
  public:
    LoggingHelper();
    ~LoggingHelper();

    Ptr<Logging> Install(NodeContainer nodes);

    /**
     * @brief It schedules the saveToFile in logging every t seconds.
     *
     * @param t Time interval to schedule the saveToFile in the logging
     * @param fileName the filename to save
     */
    void SaveToFile(Time t,
                    std::string fileName); // it schedules the save to file in the next second

  private:
    /**
     * \brief Enable flow monitoring on a single node
     * \param node A Ptr<Node> to the node on which to enable flow monitoring.
     * \returns a pointer to the Logging object
     */
    Ptr<Logging> Install(Ptr<Node> node);
    ObjectFactory m_monitorFactory; //!< Object factory
    Ptr<Logging> m_monitor;         // logging
};

} // namespace ns3
#endif // LOGGING_HELPER_H_