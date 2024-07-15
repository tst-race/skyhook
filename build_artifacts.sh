#!/usr/bin/env bash

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

# -----------------------------------------------------------------------------
# Script to build artifacts for the plugin in all possible environments: 
# android client, linux client, and linux server. Once built, move the artifacts
# to the plugin/artifacts dir for publishing to Jfrog Artifactory
# -----------------------------------------------------------------------------


set -e
CALL_NAME="$0"


###
# Helper functions
###


# Load Helper Functions
BASE_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) >/dev/null 2>&1 && pwd)
. ${BASE_DIR}/helper_functions.sh


###
# Arguments
###

# Version values
RACE_VERSION="2.6.0"
PLUGIN_REVISION="r1"

# Build Arguments
CMAKE_ARGS="-j"
VERBOSE=""

HELP=\
"Script to build artifacts for the plugin for all possible environments: 
android client, linux client, and linux server. Once built, move the artifacts
to the plugin/artifacts dir for publishing to Jfrog Artifactory
NOTE: run from race-sdk container 

Build Arguments:
    -c [value], --cmake_args [value], --cmake_args=[value]
        Additional arguments to pass to cmake.
    --race-version [value], --race-version=[value]
        Specify the RACE version. Defaults to '${RACE_VERSION}'.
    --plugin-revision [value], --plugin-revision=[value]
        Specify the Plugin Revision Number. Defaults to '${PLUGIN_REVISION}'.
    --verbose
        Make everything very verbose.

Help Arguments:
    -h, --help
        Print this message

Examples:
    ./build_artifacts.sh --race-version=1.4.0
"

while [ $# -gt 0 ]
do
    key="$1"

    case $key in
        --race-version)
        if [ $# -lt 2 ]; then
            formatlog "ERROR" "missing RACE version number" >&2
            exit 1
        fi
        RACE_VERSION="$2"
        shift
        shift
        ;;
        --race-version=*)
        RACE_VERSION="${1#*=}"
        shift
        ;;
        
        --plugin-revision)
        if [ $# -lt 2 ]; then
            formatlog "ERROR" "missing revision number" >&2
            exit 1
        fi
        PLUGIN_REVISION="$2"
        shift
        shift
        ;;
        --plugin-revision=*)
        PLUGIN_REVISION="${1#*=}"
        shift
        ;;

        --cmake-args)
        if [ $# -lt 2 ]; then
            formatlog "ERROR" "missing revision number" >&2
            exit 1
        fi
        CMAKE_ARGS="$2"
        shift
        shift
        ;;
       --cmake-args=*)
        CMAKE_ARGS="${1#*=}"
        shift
        ;;

        --verbose)
        VERBOSE="-DCMAKE_VERBOSE_MAKEFILE=ON"
        shift
        ;;

        --skip-aws-sdk)
        SKIP_AWS_SDK=1
        shift
        ;;

        -h|--help)
        printf "%s" "${HELP}"
        shift
        exit 1;
        ;;
        *)
        formatlog "ERROR" "${CALL_NAME} unknown argument \"$1\""
        exit 1
        ;;
    esac
done

if [ ! -z "${VERBOSE}" ] ; then
    set -x
fi

###
# Main Execution
###

formatlog "INFO" "Cleaning plugin/artifacts Before Building Artifacts"
SKIP_AWS_SDK=$SKIP_AWS_SDK bash ${BASE_DIR}/clean_artifacts.sh

if [ -z "${SKIP_AWS_SDK}" ]
then
    formatlog "INFO" "Building AWS CPP SDK"
    mkdir -p build/aws-install 
    apt-get update && apt-get install zlib1g-dev -y
    pushd aws-sdk-cpp
    cmake ../aws-sdk-cpp/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../build/aws-install
    make
    make install
    popd
else
    formatlog "INFO" "Skipping building AWS CPP SDK"
fi

if [ "$(uname -m)" == "x86_64" ]
then
    formatlog "INFO" "Building x86_64 Linux Client/Server"
    cmake --preset=LINUX_x86_64 -Wno-dev \
        -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
    # This will copy the output to plugin/artifacts/linux-x86_64-[client|server]
    cmake --build --preset=LINUX_x86_64 $CMAKE_ARGS

    # formatlog "INFO" "Building Android x86_64 Client"
    # cmake --preset=ANDROID_x86_64 -Wno-dev \
    #     -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
    # # This will copy the output to plugin/artifacts/android-x86_64-client
    # cmake --build --preset=ANDROID_x86_64 $CMAKE_ARGS

    # formatlog "INFO" "Building Android arm64-v8a Client"
    # cmake --preset=ANDROID_arm64-v8a -Wno-dev \
    #     -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
    # # This will copy the output to plugin/artifacts/android-arm64-v8a-client
    # cmake --build --preset=ANDROID_arm64-v8a $CMAKE_ARGS
else 
    formatlog "INFO" "Building Linux aarch64 Client/Server"
    # export CMAKE_TEMPLATE_DIR="/opt/race/"
    cmake --preset=LINUX_arm64-v8a -Wno-dev \
        -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
    # This will copy the output to plugin/artifacts/linux-arm64-v8a-[client|server]
    cmake --build --preset=LINUX_arm64-v8a $CMAKE_ARGS

    formatlog "android build not supported on aarch64 arch"
    # formatlog "INFO" "Building Android arm64-v8a_M1 Client"
    # cmake --preset=ANDROID_arm64-v8a_M1 -Wno-dev \
    #     -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
    # # This will copy the output to plugin/artifacts/android-arm64-v8a_M1-client
    # cmake --build --preset=ANDROID_arm64-v8a_M1
fi

# # comment arm or x86 linux builds as needed for host architecture
# formatlog "INFO" "Building x86_64 Linux Client/Server"
# cmake --preset=LINUX_x86_64 -Wno-dev \
#     -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
# # This will copy the output to plugin/artifacts/linux-x86_64-[client|server]
# cmake --build --preset=LINUX_x86_64

# formatlog "INFO" "Building Linux Client/Server"
# cmake --preset=LINUX_arm64-v8a -Wno-dev \
#     -DBUILD_VERSION="${RACE_VERSION}-${PLUGIN_REVISION}"
# # This will copy the output to plugin/artifacts/linux-arm64-v8a-[client|server]
# cmake --build --preset=LINUX_arm64-v8a
