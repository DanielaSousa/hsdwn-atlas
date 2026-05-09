
#include "utils.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("RyuCtrlUtils");

/*
https://docs.python.org/3/extending/extending.html
*/

void
CallPythonWithTime(PyObject* pModule)
{
    // Get the current NS-3 simulation time in seconds
    double now = Simulator::Now().GetSeconds();

    if (pModule != nullptr)
    {
        PyObject* pFunc = PyObject_GetAttrString(pModule, "receive_clock");
        if (pFunc)
        {
            // Build Python args: pass the current simulation time
            PyObject* pArgs = PyTuple_Pack(1, PyFloat_FromDouble(now));
            PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
            Py_DECREF(pArgs);
            Py_DECREF(pValue);
        }
        else
        {
            std::cerr << "Failed to call Python function" << std::endl;
        }
        Py_DECREF(pFunc);
    }
    else
    {
        std::cerr << "Failed to load Python module" << std::endl;
    }

    // Reschedule this function to run again in 1 second
    Simulator::Schedule(Seconds(1.0), &CallPythonWithTime, pModule);
}

/**
 * @brief Get single Best Path
 *
 * @param pModule
 * @param src who is requesting this flow
 * @param dst destination IP
 * @param service service code
 * @param ip_src source IP
 * @param mac_ta mac from transmisser
 * @return std::vector<std::string> returns OPF orders to be executed (first string is for packet
 * out, the rest are flow mods)
 */
std::vector<std::string>
Py_get_path(PyObject* pModule, int src, int dst, int service, int ip_src, Mac48Address mac_ta)
{
    NS_LOG_FUNCTION(ip_src << "(" << src << ")" << dst << " - " << mac_ta);
    std::stringstream str1;
    str1 << mac_ta;
    std::cout << " -------- Python " << std::endl;
    PyObject* pFunc;
    pFunc = PyObject_GetAttrString(pModule, "process_packet_in");
    if (!pFunc)
    {
        NS_LOG_ERROR("something went wrong when getting function get_path");
    }
    PyObject* args = PyTuple_Pack(5,
                                  Py_BuildValue("i", src),
                                  Py_BuildValue("i", dst),
                                  Py_BuildValue("i", service),
                                  Py_BuildValue("i", ip_src),
                                  PyUnicode_FromString(str1.str().c_str()));
    if (!args)
    {
        NS_LOG_ERROR("something went wrong when packing arguments");
    }

    PyObject* myResult = PyObject_CallObject(pFunc, args);
    if (!myResult)
    {
        NS_LOG_ERROR("something went wrong when calling function get_path");
    }
    std::cout << " -------- END Python " << std::endl;
    Py_DECREF(args);
    PyUnicode_AsEncodedString(myResult, "utf-8", "strict");
    std::cout << "CHECKED4" << std::endl;
    char* str_p = PyBytes_AsString(myResult);
    std::cout << "length " << strlen(str_p) << std::endl;

    std::string ss(str_p, strlen(str_p));

    NS_LOG_INFO("result getPath " << ss.size() << " " << ss);
    Py_DECREF(myResult);
    return parse_orders(ss);
}

/**
 * @brief
 *
 * @param pModule
 * @param dst destination IP
 * @param service service code
 * @param ip_src source IP
 */
void
Py_process_hard_timeout(PyObject* pModule, int src, int dst, int service, int ip_src)
{
    NS_LOG_FUNCTION(ip_src << "(" << src << ")" << dst);
    PyObject* pFunc;
    pFunc = PyObject_GetAttrString(pModule, "process_hard_timeout");
    if (!pFunc)
    {
        NS_LOG_ERROR("something went wrong when getting function process_hard_timeout");
    }
    PyObject* args = PyTuple_Pack(4,
                                  Py_BuildValue("i", src),
                                  Py_BuildValue("i", dst),
                                  Py_BuildValue("i", service),
                                  Py_BuildValue("i", ip_src));
    if (!args)
    {
        NS_LOG_ERROR("something went wrong when packing arguments");
    }

    PyObject* myResult = PyObject_CallObject(pFunc, args);
    if (!myResult)
    {
        NS_LOG_ERROR("something went wrong when calling function process_hard_timeout");
    }
}

void
Py_process_idle_timeout(PyObject* pModule, int src, int dst, int service, int ip_src)
{
    // do i need to reclalculate???
    NS_LOG_FUNCTION(ip_src << "(" << src << ")" << dst);
    PyObject* pFunc;
    pFunc = PyObject_GetAttrString(pModule, "process_idle_timeout");
    if (!pFunc)
    {
        NS_LOG_ERROR("something went wrong when getting function process_idle_timeout");
    }
    PyObject* args = PyTuple_Pack(4,
                                  Py_BuildValue("i", src),
                                  Py_BuildValue("i", dst),
                                  Py_BuildValue("i", service),
                                  Py_BuildValue("i", ip_src));
    if (!args)
    {
        NS_LOG_ERROR("something went wrong when packing arguments");
    }

    PyObject* myResult = PyObject_CallObject(pFunc, args);
    if (!myResult)
    {
        NS_LOG_ERROR("something went wrong when calling function process_idle_timeout");
    }
}

/**
 * @brief Check for loops, and solves them
 *
 * @param pModule
 * @param dpId receiver's openflow ID
 * @param src flowKey
 * @param dst flowKey
 * @param service flowKey
 * @param mac_transmitter the transmisster of the packet
 * @return std::vector<std::string>
 */
// std::vector<std::string>
// Py_check_4_loop(PyObject* pModule,
//                 uint64_t dpId,
//                 int src,
//                 int dst,
//                 int service,
//                 Mac48Address mac_transmitter)
// {
//     PyObject* pFunc;
//     pFunc = PyObject_GetAttrString(pModule, ???);
//     PyObject* args = PyTuple_Pack(5,
//                                   Py_BuildValue("i", (int)dpId),
//                                   Py_BuildValue("i", src),
//                                   Py_BuildValue("i", dst),
//                                   Py_BuildValue("i", service),
//                                   PyUnicode_FromString(mac_transmitter.c_str()));
//     PyObject* myResult = PyObject_CallObject(pFunc, args);
//     if (!myResult)
//     {
//         NS_LOG_ERROR("something went wrong when calling function get_path");
//     }
//     Py_DECREF(args);
//     // TODO
//     return NULL;
// }

/**
 * @brief
 *
 * @param pModule
 * @param code net.EXISTS | net.UPDATE
 * @param dpid
 * @param port_no
 * @param ip
 * @param mac
 * @return int
 */
int
Py_node(PyObject* pModule,
        int code,
        uint64_t dpId,
        uint32_t port_no,
        std::string ip,
        std::string mac)
{
    NS_LOG_FUNCTION(code << dpId << port_no << ip << mac);
    // net view code
    // EXISTS = 0
    // UPDATE = 1

    PyObject* pFunc;
    pFunc = PyObject_GetAttrString(pModule, "node");
    PyObject* args = PyTuple_Pack(5,
                                  Py_BuildValue("i", code),
                                  Py_BuildValue("i", dpId),
                                  Py_BuildValue("i", port_no),
                                  PyUnicode_FromString(ip.c_str()),
                                  PyUnicode_FromString(mac.c_str()));
    PyObject* myResult = PyObject_CallObject(pFunc, args);
    int result = (int)PyLong_AsLong(myResult);

    if (!myResult)
    {
        NS_LOG_ERROR("something went wrong when calling function node");
    }
    if (code == NODE_CODE_UPDATE)
    {
        NS_ASSERT_MSG(result >= 0, "Node does NOT exist!");
    }
    Py_DECREF(myResult);

    return result;
}

int
Py_check_same_dev(PyObject* pModule, int idx_a, int idx_b)
{
    NS_LOG_FUNCTION(idx_a << idx_b);
    NS_ASSERT_MSG(idx_a >= 0, "idx_a is out ot bounds!");
    NS_ASSERT_MSG(idx_b >= 0, "idx_b is out ot bounds!");
    PyObject* pFunc;
    pFunc = PyObject_GetAttrString(pModule, "check_same_dev");
    PyObject* args = PyTuple_Pack(2, Py_BuildValue("i", idx_a), Py_BuildValue("i", idx_b));
    PyObject* myResult = PyObject_CallObject(pFunc, args);
    if (!myResult)
    {
        NS_LOG_ERROR("something went wrong when calling function check_same_dev");
    }
    int result = (int)PyLong_AsLong(myResult);
    Py_DECREF(myResult);
    return result;
}

/**
 * @brief
 *
 * @param pModule
 * @param dpId
 * @param buf
 * @return int number of changes
 */
std::vector<std::string>
Py_parse_experimenter(PyObject* pModule, uint64_t dpId, std::string buf, int period)
{
    NS_LOG_FUNCTION(buf.size());

    PyObject* pFunc;
    pFunc = PyObject_GetAttrString(pModule, "parse_experimenter_message");
    // PyObject* args = PyTuple_Pack(2, PyBytes_FromString(buf.c_str()), Py_BuildValue("i", dpId));
    PyObject* args = PyTuple_Pack(3,
                                  PyBytes_FromStringAndSize(buf.c_str(), buf.size()),
                                  Py_BuildValue("i", dpId),
                                  Py_BuildValue("i", period));

    if (!args)
    {
        NS_LOG_ERROR("something went wrong when packing arguments");
    }

    PyObject* myResult = PyObject_CallObject(pFunc, args);
    if (!myResult)
    {
        NS_LOG_ERROR("something went wrong when calling function parse_experimenter_message");
    }
    Py_DECREF(args);

    std::vector<std::string> bla;
    PyUnicode_AsEncodedString(myResult, "utf-8", "strict");
    char* str_p = PyBytes_AsString(myResult);

    std::string ss(str_p);
    Py_DECREF(myResult);
    if (ss.size() > 0)
    {
        NS_LOG_INFO("network graph edges changes:" << dpId << " " << ss);
        bla = parse_orders(ss);
    }

    return bla;
}

// TODO change this

//     std::string
//     bufferToString(uint8_t* buffer, size_t size)
// {
//     std::stringstream ss;
//     for (size_t i = 0; i < size; i++)
//     {
//         ss << (int)buffer[i];
//     }
//     return ss.str();
// }

std::vector<std::string>
parse_orders(std::string s)
{
    std::stringstream test(s);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(test, segment, '|'))
    {
        seglist.push_back(segment);
    }
    return seglist;
}

std::tuple<std::string, int>
parse_packet_out(std::string s)
{
    // port, mac
    std::stringstream test(s);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(test, segment, '&'))
    {
        seglist.push_back(segment);
    }

    if (seglist[1] == "None")
    {
        return std::make_tuple("", -1);
    }

    return std::make_tuple(seglist[1], std::stoi(seglist[0]));
}