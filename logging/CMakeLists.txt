build_lib(
  LIBNAME logging
  SOURCE_FILES
    helper/logging-helper.cc
    model/logging.cc
    model/node-probe.cc
  HEADER_FILES
    helper/logging-helper.h
    model/logging.h
    model/node-probe.h
    
  LIBRARIES_TO_LINK ${libinternet}
                    ${libstats}
                    ${libconfig-store}
  #TEST_SOURCES test/dhcp-test.cc
  #             test/ipv6-radvd-test.cc
)
