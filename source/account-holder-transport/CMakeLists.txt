
# 
# Copyright 2023 Two Six Technologies
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

################################################################################
# Transport component
################################################################################

setup_component_target(
    TARGET SkyhookTransportAccountHolder
    SOURCES
        ../common/Link.cpp
        ../common/LinkAddress.cpp
        ../common/LinkMap.cpp
        ../common/SkyhookTransport.cpp
        ../common/log.cpp
        LinkAccountHolder.cpp
        LinkAccountHolderSingleReceive.cpp
	SkyhookTransportAccountHolder.cpp
	S3Manager.cpp
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/aws-sdk
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/aws-sdk)
list(APPEND CMAKE_PREFIX_PATH "${AWS_SDK}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "AWS_SDK: ${AWS_SDK}")
set(BUILD_SHARED_LIBS ON CACHE STRING "Link to shared libraries by default.")
find_package(AWSSDK REQUIRED COMPONENTS core s3)

# expect aws-install/lib to be in the directory with the SkyhookTransport.so file
set_target_properties(SkyhookTransportAccountHolder PROPERTIES LINK_FLAGS "-Wl,-rpath,$ORIGIN/aws-sdk/")
target_include_directories(SkyhookTransportAccountHolder SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../build/aws-install/include/)
target_link_libraries(SkyhookTransportAccountHolder ${AWSSDK_LINK_LIBRARIES})

