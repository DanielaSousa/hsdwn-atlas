

set(NS3_OFSWITCH13
    "OFF"
    CACHE INTERNAL
    "ON if OFSwitch13 is found."
)
# Check for BOOST dependency
check_include_files(
  "boost/static_assert.hpp"
  BOOST_STATIC_ASSERT_FOUND
  LANGUAGE
  CXX
)

if(NOT BOOST_STATIC_ASSERT_FOUND)
  message(${HIGHLIGHTED_STATUS} "BOOST static assert was not found.")
  message(${HIGHLIGHTED_STATUS} "Skipping OFSwitch13")
  return()
endif()

# ----- BUG BOFUSS FILES - manually setup ---

# Check for BOFUSS dependency

# List of BOFUSS headers files required by OFSwtich13
set(NS3_OFSWITCH13_BOFUSS_HEADERS
  bofuss/action_set.h
  bofuss/datapath.h
  bofuss/dp_actions.h
  bofuss/dp_buffers.h
  bofuss/dp_control.h
  bofuss/dp_ports.h
  bofuss/dpctl.h
  bofuss/flow_entry.h
  bofuss/flow_table.h
  bofuss/group_entry.h
  bofuss/group_table.h
  bofuss/match_std.h
  bofuss/meter_entry.h
  bofuss/meter_table.h
  bofuss/ofl-actions.h
  bofuss/ofl-err.h
  bofuss/ofl-messages.h
  bofuss/ofl-print.h
  bofuss/ofl-structs.h
  bofuss/ofpbuf.h
  bofuss/openflow.h
  bofuss/oxm-match.h
  bofuss/packet_handle_std.h
  bofuss/packet.h
  bofuss/pipeline.h
  bofuss/timeval.h
  bofuss/vlog.h
)

set(NS3_OFSWITCH13_BOFUSS_PATH
    /home/phd/Documents/PhD/ofsoftswitch13
    CACHE PATH
    "The BOFUSS library path"
)
message(STATUS "Looking for BOFUSS library in ${NS3_OFSWITCH13_BOFUSS_PATH}")
# ${CMAKE_OUTPUT_DIRECTORY}  /home/phd/Documents/PhD/ns-3-dev/build
find_external_library(
  DEPENDENCY_NAME bofuss
  LIBRARY_NAME bofuss
  HEADER_NAMES ${NS3_OFSWITCH13_BOFUSS_HEADERS}
  SEARCH_PATHS ${NS3_OFSWITCH13_BOFUSS_PATH}
)

if(NOT bofuss_FOUND AND NOT ${bofuss_FOUND})
  message(${HIGHLIGHTED_STATUS} "BOFUSS library was not found.")
  message(${HIGHLIGHTED_STATUS} "Skipping Hybrid-SDN")
  return()
endif()

# Enabling OFSwitch13 compilation
set(NS3_OFSWITCH13
    "ON"
    CACHE INTERNAL
    "ON if OFSwitch13 is found."
)

include_directories(${bofuss_INCLUDE_DIRS})
add_definitions(-DNS3_OFSWITCH13)
# -----[END] BUG BOFUSS FILES - manually setup ---

# # Library to link the OFSwitch13 module
# set(libraries_to_link
#   ${libcsma}
#   ${libinternet}
#   ${libapplications}
#   ${libpoint-to-point}
#   ${libvirtual-net-device}
#   ${BOFUSS_LIBRARIES}
# )



build_lib(
    LIBNAME hybrid-sdn
    SOURCE_FILES
        model/quality-monitor.cc
        model/hybrid-sdn-controller.cc
        helper/hybrid-sdn-helper.cc
        model/node_stats.cc
        helper/exp-msg-data-helper.cc
    HEADER_FILES
        model/quality-monitor.h
        model/hybrid-sdn-controller.h
        helper/hybrid-sdn-helper.h
        model/node_stats.h
        helper/exp-msg-data-helper.h
        
    LIBRARIES_TO_LINK   ${bofuss_LIBRARIES}
                        ${libcore}
                        ${libinternet}
                        ${libolsr}
                        ${libofswitch13}
                        ${libwifi}
                        ${liblte}
                        #['core','internet','olsr', 'ofswitch13', 'wifi', 'lte']
    #TEST_SOURCES test/hybrid-sdn-test-suite.cc
    #             test/ipv6-radvd-test.cc
    )
