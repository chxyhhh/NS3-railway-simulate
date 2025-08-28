# Install script for directory: /Users/nnn/ns3/ns-3.37/src/wave

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/nnn/ns3/ns-3.37/build/lib/libns3.37-wave-default.dylib")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-wave-default.dylib" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-wave-default.dylib")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/nnn/ns3/ns-3.37/build/lib"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-wave-default.dylib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" -x "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.37-wave-default.dylib")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/Users/nnn/ns3/ns-3.37/src/wave/helper/wave-bsm-helper.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/helper/wave-bsm-stats.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/helper/wave-helper.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/helper/wave-mac-helper.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/helper/wifi-80211p-helper.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/bsm-application.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/channel-coordinator.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/channel-manager.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/channel-scheduler.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/default-channel-scheduler.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/higher-tx-tag.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/ocb-wifi-mac.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/vendor-specific-action.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/vsa-manager.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/wave-frame-exchange-manager.h"
    "/Users/nnn/ns3/ns-3.37/src/wave/model/wave-net-device.h"
    "/Users/nnn/ns3/ns-3.37/build/include/ns3/wave-module.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/nnn/ns3/ns-3.37/cmake-cache/src/wave/examples/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/nnn/ns3/ns-3.37/cmake-cache/src/wave/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
