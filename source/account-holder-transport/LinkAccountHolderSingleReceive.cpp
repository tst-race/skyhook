
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

#include "LinkAccountHolderSingleReceive.h"

#include <ITransportSdk.h>
#include "SkyhookTransportAccountHolder.h"
#include <base64.h>

#include <chrono>
#include <nlohmann/json.hpp>

#include "PersistentStorageHelpers.h"
#include "curlwrap.h"
#include "log.h"

LinkAccountHolderSingleReceive::LinkAccountHolderSingleReceive(const LinkID &linkId_, const LinkAddress &address_, const LinkProperties &properties_,
                                     bool isCreator_, SkyhookTransportAccountHolder *transport_, ITransportSdk *sdk_) :
    LinkAccountHolder(linkId_, address_, properties_, isCreator_, transport_, sdk_) {

    // Initialize the policy for the initial Uuids and _n_ forward
    accountHolderTransport->s3Manager.createBucket(address.fetchBucket, address.region);
    if (address.fetchBucket != address.postBucket) {
      accountHolderTransport->s3Manager.createBucket(address.postBucket, address.region);
    }
    accountHolderTransport->s3Manager.addObjPermission("*",
                                                       address.fetchBucket,
                                                       "private-puttable",
                                                       "s3:PutObject",
                                                       accountHolderTransport->s3Manager.selfPrincipal);
    
    puttableUuids.push_back(this->address.initialFetchObjUuid);
    accountHolderTransport->s3Manager.makeObjPuttable(puttableUuids.back(), address);
    logInfo("LinkAccountHolderSingleReceive constructed");
}

LinkAccountHolderSingleReceive::~LinkAccountHolderSingleReceive() {
    TRACE_METHOD(linkId);
    shutdown();
}

std::string LinkAccountHolderSingleReceive::fetchOnActionThread(const std::string &fetchObjUuid) {
    TRACE_METHOD(linkId, fetchObjUuid);
    logPrefix += linkId + ": ";
    
    std::vector<uint8_t> data;
    if (accountHolderTransport->s3Manager.getObject(address.fetchBucket, fetchObjUuid, data)) {
        logInfo(logPrefix + "data size: " + std::to_string(data.size()));
        logInfo(logPrefix + "data: " + std::string(data.begin(), data.end()));
        sdk->onReceive(linkId, {linkId, "*/*", false, {}}, data);
        accountHolderTransport->s3Manager.deleteObject(address.fetchBucket, fetchObjUuid);
    }

    return puttableUuids.front();
}

std::string LinkAccountHolderSingleReceive::postOnActionThread(const std::string &postObjUuid, const std::vector<RaceHandle> &handles, uint64_t actionId) {
    TRACE_METHOD(linkId, handles, actionId);
    logPrefix += linkId + ": ";
    logError(logPrefix + "No sending allowed on a SingleReceive link");
    updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
    return postObjUuid;
}



