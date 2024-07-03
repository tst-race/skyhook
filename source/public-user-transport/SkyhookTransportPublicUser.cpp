
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

#include "SkyhookTransportPublicUser.h"

#include <chrono>
#include <nlohmann/json.hpp>

#include "JsonTypes.h"
#include "Link.h"
#include "LinkAddress.h"
#include "log.h"
#include <iostream>
#include <openssl/sha.h>

SkyhookTransportPublicUser::SkyhookTransportPublicUser(ITransportSdk *sdk, const std::string &roleName) :
  SkyhookTransport(sdk, roleName) {}

#ifndef TESTBUILD
ITransportComponent *createTransport(const std::string &transport, ITransportSdk *sdk,
                                     const std::string &roleName,
                                     const PluginConfig & /* pluginConfig */) {
    TRACE_FUNCTION(transport, roleName);
    return new SkyhookTransportPublicUser(sdk, roleName);
}
void destroyTransport(ITransportComponent *component) {
    TRACE_FUNCTION();
    delete component;
}

#endif
