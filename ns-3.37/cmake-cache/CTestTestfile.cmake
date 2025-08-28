# CMake generated Testfile for 
# Source directory: /Users/nnn/ns3/ns-3.37
# Build directory: /Users/nnn/ns3/ns-3.37/cmake-cache
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ctest-stdlib_pch_exec "ns3.37-stdlib_pch_exec-default")
set_tests_properties(ctest-stdlib_pch_exec PROPERTIES  WORKING_DIRECTORY "/Users/nnn/ns3/ns-3.37/cmake-cache/" _BACKTRACE_TRIPLES "/Users/nnn/ns3/ns-3.37/build-support/macros-and-definitions.cmake;1411;add_test;/Users/nnn/ns3/ns-3.37/build-support/macros-and-definitions.cmake;1334;set_runtime_outputdirectory;/Users/nnn/ns3/ns-3.37/CMakeLists.txt;133;process_options;/Users/nnn/ns3/ns-3.37/CMakeLists.txt;0;")
subdirs("src")
subdirs("examples")
subdirs("scratch")
subdirs("utils")
