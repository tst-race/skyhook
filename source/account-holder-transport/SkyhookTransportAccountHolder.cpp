
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

#include "SkyhookTransportAccountHolder.h"

#include <chrono>
#include <nlohmann/json.hpp>

#include "JsonTypes.h"
#include "LinkAccountHolder.h"
#include "LinkAddress.h"
#include "log.h"
#include <iostream>
#include <aws/core/VersionConfig.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/AccessControlPolicy.h>
#include <aws/s3/model/GetBucketAclRequest.h>
#include <aws/s3/model/GetObjectAclRequest.h>
#include <aws/s3/model/PutObjectAclRequest.h>
#include <aws/s3/model/Grant.h>
#include <aws/s3/model/Permission.h>

#include <openssl/sha.h>


SkyhookTransportAccountHolder::SkyhookTransportAccountHolder(ITransportSdk *sdk, const std::string &roleName) :
    SkyhookTransport(sdk, roleName) {
}

std::shared_ptr<Link> SkyhookTransportAccountHolder::createLinkInstance(
  const LinkID &linkId, const LinkAddress &address, const LinkProperties &properties, bool isCreator) {
    std::shared_ptr<Link> link;
    logInfo("createLinkInstance: isCreator: " + std::to_string(isCreator));
    link = std::make_shared<LinkAccountHolder>(linkId, address, properties, isCreator, this, sdk);
    logInfo("starting link");
    link->start();
    logInfo("link started");
    return link;
}


#ifndef TESTBUILD
ITransportComponent *createTransport(const std::string &transport, ITransportSdk *sdk,
                                     const std::string &roleName,
                                     const PluginConfig & /* pluginConfig */) {
    TRACE_FUNCTION(transport, roleName);
    
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    return new SkyhookTransportAccountHolder(sdk, roleName);
}
void destroyTransport(ITransportComponent *component) {
    TRACE_FUNCTION();
    Aws::SDKOptions options;
    Aws::ShutdownAPI(options);
    delete component;
}

#endif
