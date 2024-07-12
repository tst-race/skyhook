
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
#include "LinkAccountHolderSingleReceive.h"
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
  SkyhookTransport(sdk, roleName),
  canonicalIdReqHandle(sdk->requestPluginUserInput("canonicalId", "What is the Canonical ID for your AWS S3 account? (https://docs.aws.amazon.com/accounts/latest/reference/manage-acct-identifiers.html#FindingCanonicalId)", true).handle) {
}

ComponentStatus SkyhookTransportAccountHolder::onUserInputReceived(RaceHandle handle, bool answered,
                                                                   const std::string &response) {
    TRACE_METHOD(handle, answered, response);
    if (handle == canonicalIdReqHandle) {
      canonicalIdReqHandle = NULL_RACE_HANDLE;
      if (answered) {
        // canonicalId = response;
        s3Manager.selfPrincipal = response;
      } else {
        logError(logPrefix + "AWS Canonical ID for S3 account is required, provide --param skyhookBasicComposition.canonicalId=< canonical ID > to resolve (change composition name as appropriate) (https://docs.aws.amazon.com/accounts/latest/reference/manage-acct-identifiers.html#FindingCanonicalId)");
        return COMPONENT_ERROR;
      }
    } else {
      SkyhookTransport::handleUserInputResponse(handle, answered, response);
    }

    if (canonicalIdReqHandle == NULL_RACE_HANDLE and
        regionReqHandle == NULL_RACE_HANDLE and
        bucketReqHandle == NULL_RACE_HANDLE and
        seedReqHandle == NULL_RACE_HANDLE and
        singleReceiveReqHandle == NULL_RACE_HANDLE) {
        ready = true;
        sdk->updateState(COMPONENT_STATE_STARTED);
    }
    return COMPONENT_OK;
}

std::shared_ptr<Link> SkyhookTransportAccountHolder::createLinkInstance(
  const LinkID &linkId, const LinkAddress &address, const LinkProperties &properties, bool isCreator) {
    std::shared_ptr<Link> link;
    logInfo("createLinkInstance: isCreator: " + std::to_string(isCreator));
    logInfo("createLinkInstance: singleReceive: " + std::to_string(address.singleReceive));
    if (isCreator and address.singleReceive) {
      link = std::make_shared<LinkAccountHolderSingleReceive>(linkId, address, properties, isCreator, this, sdk);
    } else {
      link = std::make_shared<LinkAccountHolder>(linkId, address, properties, isCreator, this, sdk);
    }
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
