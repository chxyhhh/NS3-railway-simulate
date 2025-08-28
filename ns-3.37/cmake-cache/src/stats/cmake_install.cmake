# Install script for directory: /Users/nnn/ns3/ns-3.37/src/stats

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "default")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/nnn/ns3/ns-3.37/build/lib/libns3.37-stats-default.dylib")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-stats-default.dylib" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-stats-default.dylib")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/nnn/ns3/ns-3.37/build/lib"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-stats-default.dylib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" -x "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-stats-default.dylib")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/Users/nnn/ns3/ns-3.37/src/stats/model/sqlite-data-output.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/sqlite-output.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/helper/file-helper.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/helper/gnuplot-helper.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/average.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/basic-data-calculators.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/boolean-probe.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/data-calculator.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/data-collection-object.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/data-collector.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/data-output-interface.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/double-probe.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/file-aggregator.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/get-wildcard-matches.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/gnuplot-aggregator.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/gnuplot.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/histogram.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/omnet-data-output.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/probe.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/stats.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/time-data-calculators.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/time-probe.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/time-series-adaptor.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/uinteger-16-probe.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/uinteger-32-probe.h"
    "/Users/nnn/ns3/ns-3.37/src/stats/model/uinteger-8-probe.h"
    "/Users/nnn/ns3/ns-3.37/build/include/ns3/stats-module.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/nnn/ns3/ns-3.37/cmake-cache/src/stats/examples/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/nnn/ns3/ns-3.37/cmake-cache/src/stats/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
