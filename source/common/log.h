
//
// Copyright 2023 Two Six Technologies
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef __COMMS_TWOSIX_COMMON_LOG_H__
#define __COMMS_TWOSIX_COMMON_LOG_H__

#include <RaceLog.h>

#include <string>

void logDebug(const std::string &message);
void logInfo(const std::string &message);
void logWarning(const std::string &message);
void logError(const std::string &message);

// logDebug messages don't make it to stdout
#ifdef TRACE_METHOD_BASE
#undef TRACE_METHOD_BASE
#endif

#ifdef TRACE_FUNCTION_BASE
#undef TRACE_FUNCTION_BASE
#endif

#define TRACE_METHOD_BASE(pluginName, ...)                                                         \
    std::string logPrefix = RaceLog::cppDemangle(typeid(*this).name()) + "::" + __func__ + ": ";   \
    RaceLog::logInfo(                                                                             \
        #pluginName, logPrefix + "called" + RaceLog::stringifyValues(#__VA_ARGS__, ##__VA_ARGS__), \
        "");                                                                                       \
    Defer defer([logPrefix] { RaceLog::logInfo(#pluginName, logPrefix + "returned", ""); })

#define TRACE_FUNCTION_BASE(pluginName, ...)                                                       \
    std::string logPrefix = std::string(__func__) + ": ";                                          \
    RaceLog::logInfo(#pluginName,                                                                  \
                     logPrefix + "called" + RaceLog::stringifyValues(#__VA_ARGS__, ##__VA_ARGS__), \
                     "");                                                                          \
    Defer defer([logPrefix] { RaceLog::logInfo(#pluginName, logPrefix + "returned", ""); })

#define TRACE_METHOD(...) TRACE_METHOD_BASE(PluginSkyhook, ##__VA_ARGS__)
#define TRACE_FUNCTION(...) TRACE_FUNCTION_BASE(PluginSkyhook, ##__VA_ARGS__)

#endif  // __COMMS_TWOSIX_COMMON_LOG_H__
