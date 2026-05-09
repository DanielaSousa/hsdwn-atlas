#ifndef RYU_CTRL_UTILS_H
#define RYU_CTRL_UTILS_H

#include "ns3/core-module.h"
#include <ns3/mac48-address.h>

#include <Python.h>
#include <string.h>
#include <tuple>

using namespace ns3;

#define NODE_CODE_EXISTS 0
#define NODE_CODE_UPDATE 1

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
std::vector<std::string> Py_get_path(PyObject* pModule,
                                     int src,
                                     int dst,
                                     int service,
                                     int ip_src,
                                     Mac48Address mac_ta);

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
int Py_node(PyObject* pModule,
            int code,
            uint64_t dpId,
            uint32_t port_no,
            std::string ip,
            std::string mac);

/**
 * @brief
 *
 * @param pModule
 * @param dpId
 * @param buf
 * @return int number of changes
 */
std::vector<std::string> Py_parse_experimenter(PyObject* pModule,
                                               uint64_t dpId,
                                               std::string buf,
                                               int period);

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
std::vector<std::string> Py_check_4_loop(PyObject* pModule,
                                         uint64_t dpId,
                                         int src,
                                         int dst,
                                         int service,
                                         Mac48Address mac_transmitter);

int Py_check_same_dev(PyObject* pModule, int idx_a, int idx_b);
void Py_process_hard_timeout(PyObject* pModule, int src, int dst, int service, int ip_src);
void Py_process_idle_timeout(PyObject* pModule, int src, int dst, int service, int ip_src);

void CallPythonWithTime(PyObject* pModule);

std::string bufferToString(uint8_t* buffer, size_t size);

std::vector<std::string> parse_orders(std::string s);
std::tuple<std::string, int> parse_packet_out(std::string s);

#endif