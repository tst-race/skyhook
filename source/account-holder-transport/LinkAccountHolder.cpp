
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

#include "LinkAccountHolder.h"

#include <ITransportSdk.h>
#include "SkyhookTransportAccountHolder.h"
#include <base64.h>

#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

#include "PersistentStorageHelpers.h"
#include "curlwrap.h"
#include "log.h"

LinkAccountHolder::LinkAccountHolder(const LinkID &linkId_, const LinkAddress &address_, const LinkProperties &properties_,
                                     bool isCreator_, SkyhookTransportAccountHolder *transport_, ITransportSdk *sdk_) :
    Link(linkId_, address_, properties_, isCreator_, transport_, sdk_),
    creator(isCreator_),
    accountHolderTransport(transport_) {

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
    
    accountHolderTransport->s3Manager.addObjPermission("*",
                                                       address.postBucket,
                                                       "private-gettable",
                                                       "s3:GetObject",
                                                       accountHolderTransport->s3Manager.selfPrincipal);
    
    puttableUuids.push_back(this->address.initialFetchObjUuid);
    accountHolderTransport->s3Manager.makeObjPuttable(puttableUuids.back(), address);
    for (int idx = 0; idx < address.openObjects; ++idx) {
      puttableUuids.push_back(generateNextObjUuid(puttableUuids.back()));
      accountHolderTransport->s3Manager.makeObjPuttable(puttableUuids.back(), address);
    }
    logInfo("LinkAccountHolder constructed");
}

LinkAccountHolder::~LinkAccountHolder() {
    TRACE_METHOD(linkId);
    shutdown();
}

std::string LinkAccountHolder::fetchOnActionThread(const std::string &fetchObjUuid) {
    TRACE_METHOD(linkId, fetchObjUuid);
    logPrefix += linkId + ": ";
    
    std::vector<uint8_t> data;
    if (accountHolderTransport->s3Manager.getObject(address.fetchBucket, fetchObjUuid, data)) { 
        // Expand the "buffer" of puttable UUIDs by one, make it puttable
        // Also pop the front of the buffer of UUIDs (which should be the one we just fetched) and make it unputtable
        puttableUuids.push_back(generateNextObjUuid(puttableUuids.back()));
        std::string toBeDropped = puttableUuids.front();
        puttableUuids.pop_front();
        if (toBeDropped != fetchObjUuid) {
          logError(logPrefix + "front of puttableUuids (" + toBeDropped + ") was not the fetched object (" + fetchObjUuid + ") (popped anyway)");
        }
        accountHolderTransport->s3Manager.makeObjUnputtable(toBeDropped, address);
        accountHolderTransport->s3Manager.makeObjPuttable(puttableUuids.back(), address);

        logInfo(logPrefix + "data size: " + std::to_string(data.size()));
        logInfo(logPrefix + "data: " + std::string(data.begin(), data.end()));

       
        sdk->onReceive(linkId, {linkId, "*/*", false, {}}, data);
    }

    return puttableUuids.front();
}

std::string LinkAccountHolder::postOnActionThread(const std::string &postObjUuid, const std::vector<RaceHandle> &handles, uint64_t actionId) {
    TRACE_METHOD(linkId, handles, actionId);
    logPrefix += linkId + ": ";

    std::string nextPostObjUuid = postObjUuid;
    auto iter = contentQueue.find(actionId);
    if (iter == contentQueue.end()) {
        logError(logPrefix +
                 "no enqueued content for given action ID: " + std::to_string(actionId));
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return nextPostObjUuid;
    }

    int tries = 0;
    for (; tries < address.maxTries; ++tries) {
      if (accountHolderTransport->s3Manager.putObject(address.postBucket, postObjUuid, iter->second)) { 
            break;
        }
    }

    if (tries == address.maxTries) {
        logError(logPrefix + "retry limit exceeded: post failed");
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
    } else {
      // Remove GET permission from the oldest fetchable UUID
      // Add GET permission for a newly generated UUID
      if (fetchableUuids.size() >= static_cast<unsigned long>(address.openObjects)) {
        std::string oldUuid = fetchableUuids.front();
        fetchableUuids.pop_front();
        logInfo(logPrefix + "popping old fetchable UUID: " + oldUuid);
        accountHolderTransport->s3Manager.makeObjUngettable(oldUuid, address);
      }

      fetchableUuids.push_back(postObjUuid);
      accountHolderTransport->s3Manager.makeObjGettable(postObjUuid, address);

      nextPostObjUuid = generateNextObjUuid(postObjUuid);
      updatePackageStatus(handles, PACKAGE_SENT);
    }
    return nextPostObjUuid;
}

void LinkAccountHolder::shutdown() {
    TRACE_METHOD(linkId);
    Link::shutdown();
    for (auto &uuid : puttableUuids) {
        accountHolderTransport->s3Manager.makeObjUnputtable(uuid, address);
    }
    std::thread cleanupFetchablesThread([uuidList = this->fetchableUuids,
                                         s3Manager = &this->accountHolderTransport->s3Manager,
                                         address = this->address]() {
      std::this_thread::sleep_for(std::chrono::seconds(SHUTDOWN_DELAY_SECONDS));
      for (auto &uuid : uuidList) {
        s3Manager->makeObjUngettable(uuid, address);
      }
      s3Manager->deleteBucket(address.fetchBucket, address.region);
      if (address.fetchBucket != address.postBucket) {
        s3Manager->deleteBucket(address.postBucket, address.region);
      }
    });
    cleanupFetchablesThread.detach();
}

