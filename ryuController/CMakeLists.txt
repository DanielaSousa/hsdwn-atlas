# def build(bld):
#    obj = bld.create_ns3_program --> migrates to build_lib_example


# ----- BUG BOFUSS FILES - manually setup ---
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



# ----- python library -------
# move ryuController folder to build/bindings/python, which allows us to add only
# PYTHONPATH=ns-3-dev/build/bindings/python
file(
  GLOB_RECURSE
  ryuController_files
  ${CMAKE_CURRENT_SOURCE_DIR}/ryuController/*
)
foreach(
  file
  ${ryuController_files}
)
  string(
    REPLACE "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_OUTPUT_DIRECTORY}/bindings/python"
            destination
            ${file}
  )
  configure_file(
    ${file}
    ${destination}
    COPYONLY
  )
endforeach()

if(NOT
   NS3_BINDINGS_INSTALL_DIR
)
  message(
    ${HIGHLIGHTED_STATUS}
    "NS3_BINDINGS_INSTALL_DIR was not set. The ryuController python module won't be installed with ./ns3 install."
  )
else()
  install(
    DIRECTORY ./ryuController
    DESTINATION ${NS3_BINDINGS_INSTALL_DIR}/
  )

  add_custom_target(
    uninstall_ryuController
    COMMAND rm -R ${NS3_BINDINGS_INSTALL_DIR}/ryuController
  )
  add_dependencies(
    uninstall
    uninstall_ryuController
  )
endif()
# ---- [END] ------


# build_lib(
#   LIBNAME ryuController
#   SOURCE_FILES #model/sdn-controller.cc
#               model/ryuController.cc
#               #model/ofpp_normal-controller.cc
#               #model/workshopctrl.cc
#               model/utils.cc
#               #model/path.cc
#               #model/multiPath-controller.cc
#   HEADER_FILES #model/sdn-controller.h
#               model/ryuController.h
#               #model/ofpp_normal-controller.h
#               #model/workshopctrl.h
#               model/utils.h
#               #model/path.h
#               #model/multiPath-controller.h
              
#   LIBRARIES_TO_LINK ${Python3_LIBRARIES}
#                     ${libcsma}
#                     ${libinternet}
#                     ${libapplications}
#                     ${libpoint-to-point}
#                     ${libvirtual-net-device}
#                     ${libofswitch13} # refresh_if_ticket error. Assertion `inited' failed
#                     ${bofuss_LIBRARIES} # it needs this
# )

# # build_lib(
# #   LIBNAME ryuController
# #   SOURCE_FILES model/learning-controller.cc
# #   HEADER_FILES model/learning-controller.h
# #   LIBRARIES_TO_LINK #${Python3_LIBRARIES}
# #                     ${libcsma}
# #                     ${libinternet}
# #                     ${libapplications}
# #                     ${libpoint-to-point}
# #                     ${libvirtual-net-device}
# #                     ${libofswitch13} # refresh_if_ticket error. Assertion `inited' failed
# #                     ${bofuss_LIBRARIES} # it needs this
# # )

build_lib(
  LIBNAME ryuController
  SOURCE_FILES model/ofpp-simple-ctrl.cc
              model/ofpp-python-ctrl.cc
              model/ofpp_normal-controller.cc
              model/utils.cc
  HEADER_FILES model/ofpp-simple-ctrl.h
            model/ofpp-python-ctrl.h
            model/ofpp_normal-controller.h
            model/utils.h
  LIBRARIES_TO_LINK ${Python3_LIBRARIES}
                    ${libcsma}
                    ${libinternet}
                    ${libapplications}
                    ${libpoint-to-point}
                    ${libvirtual-net-device}
                    ${libofswitch13} # refresh_if_ticket error. Assertion `inited' failed
                    ${bofuss_LIBRARIES} # it needs this
)
