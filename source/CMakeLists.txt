
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
# Standard library dependencies (common to all components)
################################################################################

if(ANDROID)
    find_library(LIB_CURL curl)
    find_package(Boost COMPONENTS filesystem system REQUIRED) 
    find_library(LIB_CPPREST cpprest)
    find_library(LIB_LOG log)

    if(TARGET raceSdkCommon)
      set(RACE_JAVA_SHIMS RaceJavaShims)
    else()
      find_library(RACE_JAVA_SHIMS RaceJavaShims)
    endif()

    if(TARGET raceSdkCommon)
        # If this is a global build, use the targets directly so that cmake can do its dependency magic
        set(LIB_SDK_COMMON raceSdkCommon)
    else()
        # Otherwise, just treat them as any other library
        find_library(LIB_SDK_COMMON raceSdkCommon)
    endif()

    set(LIB_DEPS
        ${LIB_SDK_COMMON}
        ${LIB_CURL}
        Boost::system
        Boost::filesystem
        ${LIB_CPPREST}
        ${LIB_LOG}
        ${RACE_JAVA_SHIMS}
    )
else()
    ## Default to Linux
    find_package(CURL)
    find_library(LIB_CPPREST cpprest)
    find_library(LIB_CRYPTO crypto)
    find_library(LIB_SSL ssl)
    find_library(RACE_SDK_COMMON raceSdkCommon HINTS ENV LD_LIBRARY_PATH)

    include_directories(${CURL_INCLUDE_DIR})

    set(LIB_DEPS
        ${RACE_SDK_COMMON}
        ${CURL_LIBRARIES}
        ${LIB_CPPREST}
        ${LIB_CRYPTO}
        ${LIB_SSL}
    )

endif()

################################################################################
# Function to define the component CMake target
################################################################################

set(PLUGIN_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kit)
set(COMMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
file(GLOB_RECURSE COMMON_HEADERS CONFIGURE_DEPENDS ${COMMON_SRC_DIR}/*.h)

function(setup_component_target)
    set(OPTIONS "")
    set(ONE_VALUE_ARGS TARGET)
    set(MULTI_VALUE_ARGS SOURCES)
    cmake_parse_arguments(COMPONENT "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    add_library(${COMPONENT_TARGET} SHARED ${COMPONENT_SOURCES})
    target_include_directories(${COMPONENT_TARGET} PRIVATE ${COMMON_SRC_DIR})
    target_link_libraries(${COMPONENT_TARGET} ${LIB_DEPS})
    target_compile_definitions(${COMPONENT_TARGET} PUBLIC
        BUILD_VERSION="${BUILD_VERSION}"
    )
    set_target_properties(${COMPONENT_TARGET} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        NO_SONAME ON
    )

    file(GLOB_RECURSE COMP_HEADERS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    )
    list(APPEND COMP_HEADERS "${COMMON_HEADERS}")
    set_target_properties(${COMPONENT_TARGET} PROPERTIES
        PRIVATE_HEADER "${COMP_HEADERS}"
    )

    # TODO enable performance-* and readability-* checks (needs SDK fixes)
    setup_clang_tidy_for_target(TARGET ${COMPONENT_TARGET})
    setup_cppcheck_for_target(
        TARGET ${COMPONENT_TARGET}
        CHECKS all
        SUPPRESS unmatchedSuppression unusedFunction missingIncludeSystem
    )
    setup_clang_format_for_target(${COMPONENT_TARGET} PARENT skyhook)

    install(
        TARGETS ${COMPONENT_TARGET}
        LIBRARY DESTINATION lib/race/comms/PluginSkyhook
    )

    if(ANDROID)
        if ("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_arm64-v8a")
            set(NODE_TYPE android-arm64-v8a-client)
        else()
            set(NODE_TYPE android-x86_64-client)
        endif()

        add_custom_command(TARGET ${COMPONENT_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${COMPONENT_TARGET}> ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook/
            BYPRODUCTS ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook/lib${COMPONENT_TARGET}.so
        )
    else()
        if ("${TARGET_ARCHITECTURE}" STREQUAL "LINUX_arm64-v8a")
            set(NODE_TYPE linux-arm64-v8a)
        else()
            set(NODE_TYPE linux-x86_64)
        endif()

        message(STATUS "COMPONENT TARGETS: ${COMPONENT_TARGET}")
        add_custom_command(TARGET ${COMPONENT_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${COMPONENT_TARGET}> ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${COMPONENT_TARGET}> ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/
            BYPRODUCTS
                ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook/lib${COMPONENT_TARGET}.so
                ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/lib${COMPONENT_TARGET}.so
        )
    endif()

endfunction()

################################################################################
# Plugin manifest
################################################################################

if(ANDROID)
    if ("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_arm64-v8a")
        set(NODE_TYPE android-arm64-v8a-client)
    else()
        set(NODE_TYPE android-x86_64-client)
    endif()

    add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook/
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        OUTPUT ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook/manifest.json
    )
    add_custom_target(PluginSkyhookManifest ALL
        DEPENDS ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}/PluginSkyhook/manifest.json
    )
else()
    if ("${TARGET_ARCHITECTURE}" STREQUAL "LINUX_arm64-v8a")
        set(NODE_TYPE linux-arm64-v8a)
    else()
        set(NODE_TYPE linux-x86_64)
    endif()

    add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/client-manifest.json ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook/manifest.json
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/server-manifest.json ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/manifest.json
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/client-manifest.json
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/server-manifest.json
        OUTPUT
            ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook/manifest.json
            ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/manifest.json
    )
    add_custom_target(PluginSkyhookManifest ALL DEPENDS
        ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginSkyhook/manifest.json
        ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/manifest.json
    )


  set(AWS_SDK ${CMAKE_CURRENT_SOURCE_DIR}/../build/aws-install)
  set(AWS_SDK_FILES
    libaws-cpp-sdk-core.so
    libaws-cpp-sdk-s3.so
    )
  FOREACH(AWS_SDK_FILE ${AWS_SDK_FILES})
    add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/aws-sdk
        COMMAND ${CMAKE_COMMAND} -E copy ${AWS_SDK}/lib/${AWS_SDK_FILE} ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/aws-sdk/
        DEPENDS ${AWS_SDK}/lib/${AWS_SDK_FILE}
        OUTPUT
            ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/aws-sdk/${AWS_SDK_FILE}
    )
    add_custom_target(PluginSkyhook${AWS_SDK_FILE} ALL DEPENDS
        ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginSkyhook/aws-sdk/${AWS_SDK_FILE}
    )
  ENDFOREACH()


endif()

################################################################################
# Components
################################################################################

add_subdirectory(account-holder-transport)
add_subdirectory(public-user-transport)
add_subdirectory(user-model)
