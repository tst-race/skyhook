
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

#include "Link.h"

#include <ITransportSdk.h>
#include "SkyhookTransport.h"
#include <base64.h>
#include <sstream>

#include <chrono>
#include <nlohmann/json.hpp>

#include "PersistentStorageHelpers.h"
#include "curlwrap.h"
#include "log.h"
#include <openssl/sha.h>
// #include "picosha2.h"

static const size_t ACTION_QUEUE_MAX_CAPACITY = 10;

namespace std {
static std::ostream &operator<<(std::ostream &out, const std::vector<RaceHandle> &handles) {
    return out << nlohmann::json(handles).dump();
}
}  // namespace std

Link::Link(const LinkID &linkId, const LinkAddress &address, const LinkProperties &properties,
           bool isCreator, SkyhookTransport *transport, ITransportSdk *sdk) :
    address(std::move(address)),
    sdk(sdk),
    transport(transport),
    linkId(std::move(linkId)),
    properties(std::move(properties)) {
    logDebug("CREATING LINK");

    // Do this _before_ we potentially flip it for internal use
    this->properties.linkAddress = nlohmann::json(address).dump();
    if (isCreator) {
      // properties.linkAddress is the external address that can be loaded
      // We then set this->address to (partially) flipped values that reflect the reversed nature of being the other side of the link (i.e. fetching what the other side puts and vice versa)
        logDebug("creator, flipping address");
        // Swap post/fetch for the account holder since they do the opposite
        const std::string newFetchBucket(this->address.postBucket);
        const std::string newInitialFetchObjUuid(this->address.initialPostObjUuid);
        this->address.postBucket = this->address.fetchBucket;
        this->address.initialPostObjUuid = this->address.initialFetchObjUuid;
        this->address.fetchBucket = newFetchBucket;
        this->address.initialFetchObjUuid = newInitialFetchObjUuid;
        logDebug("internal address " + nlohmann::json(this->address).dump());
    }
}

Link::~Link() {
    TRACE_METHOD(linkId);
    shutdown();
}

LinkID Link::getId() const {
    return linkId;
}

const LinkProperties &Link::getProperties() const {
    return properties;
}

ComponentStatus Link::enqueueContent(uint64_t actionId, const std::vector<uint8_t> &content) {
    TRACE_METHOD(linkId, actionId);
    {
        std::lock_guard<std::mutex> lock(mutex);
        contentQueue[actionId] = content;
    }
    return COMPONENT_OK;
}

ComponentStatus Link::dequeueContent(uint64_t actionId) {
    TRACE_METHOD(linkId, actionId);
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = contentQueue.find(actionId);
        if (iter != contentQueue.end()) {
            contentQueue.erase(iter);
        }
    }
    return COMPONENT_OK;
}

ComponentStatus Link::fetch(std::vector<RaceHandle> handles) {
    TRACE_METHOD(linkId, handles);

    if (isShutdown) {
        logError(logPrefix + "link has been shutdown: " + linkId);
        return COMPONENT_ERROR;
    }

    std::lock_guard<std::mutex> lock(mutex);

    if (actionQueue.size() >= ACTION_QUEUE_MAX_CAPACITY) {
        logError(logPrefix + "action queue full for link: " + linkId);
        return COMPONENT_ERROR;
    }

    // TODO commented out for testing
    actionQueue.push_back({false, std::move(handles), 0});
    conditionVariable.notify_one();
    return COMPONENT_OK;
}

ComponentStatus Link::post(std::vector<RaceHandle> handles, uint64_t actionId) {
    TRACE_METHOD(linkId, handles, actionId);

    if (isShutdown) {
        logError(logPrefix + "link has been shutdown: " + linkId);
        return COMPONENT_ERROR;
    }

    std::lock_guard<std::mutex> lock(mutex);

    if (actionQueue.size() >= ACTION_QUEUE_MAX_CAPACITY) {
        logError(logPrefix + "action queue full for link: " + linkId);
        return COMPONENT_ERROR;
    }

    if (contentQueue.find(actionId) == contentQueue.end()) {
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return COMPONENT_OK;
    }

    actionQueue.push_back({true, std::move(handles), actionId});
    conditionVariable.notify_one();
    return COMPONENT_OK;
}

void Link::start() {
    TRACE_METHOD(linkId);
    thread = std::thread(&Link::runActionThread, this);
}

void Link::shutdown() {
    TRACE_METHOD(linkId);
    isShutdown = true;
    conditionVariable.notify_one();
    if (thread.joinable()) {
        thread.join();
    }
}

void Link::runActionThread() {
    TRACE_METHOD(linkId);
    logPrefix += linkId + ": ";

    std::string fetchObjUuid = address.initialFetchObjUuid;
    std::string postObjUuid = address.initialPostObjUuid;

    while (not isShutdown) {
        std::unique_lock<std::mutex> lock(mutex);
        conditionVariable.wait(lock, [this] { return isShutdown or not actionQueue.empty(); });

        if (isShutdown) {
            logDebug(logPrefix + "shutting down");
            break;
        }

        auto action = actionQueue.front();
        actionQueue.pop_front();

        if (action.post) {
            postObjUuid = postOnActionThread(postObjUuid, action.handles, action.actionId);
        } else {
            fetchObjUuid = fetchOnActionThread(fetchObjUuid);
        }
    }
}

std::string Link::generateNextObjUuid(const std::string &currentObjUuid) {
    TRACE_METHOD(currentObjUuid);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(currentObjUuid.c_str()), currentObjUuid.size(), hash);
    std::stringstream ss;
    
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++){
      ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>( hash[i] );
    }
    return ss.str();    
}

/**
 * @brief callback function required by libcurl-dev.
 * See documentation in link below:
 * https://curl.haxx.se/libcurl/c/libcurl-tutorial.html
 */
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    (static_cast<std::string *>(userp))->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
}

std::string Link::fetchOnActionThread(const std::string &fetchObjUuid) {
    TRACE_METHOD(linkId, fetchObjUuid);
    logPrefix += linkId + ": ";

    std::string nextFetchObjUuid = fetchObjUuid;
    try {
        std::string url = "https://s3." + address.region + ".amazonaws.com/" + address.fetchBucket + "/" + fetchObjUuid;
            
        CurlWrap curl;
        std::string response;
            
        logInfo(logPrefix + "Fetching from url: " + url);
        curl.setopt(CURLOPT_URL, url.c_str());
        curl.setopt(CURLOPT_WRITEFUNCTION, WriteCallback);
        curl.setopt(CURLOPT_WRITEDATA, &response);
        curl.setopt(CURLOPT_FAILONERROR, 1);
        // Fail to the curl_exception catch on 400+ responses 
        curl.perform();
        logInfo(logPrefix + "response: " + std::to_string(response.size()));
        logInfo(logPrefix + "response: " + std::string(response.begin(), response.end()));
        nextFetchObjUuid = generateNextObjUuid(fetchObjUuid);
        std::vector<uint8_t> data(response.begin(), response.end());
        sdk->onReceive(linkId, {linkId, "*/*", false, {}}, data);
        
    } catch (curl_exception &error) {
        logDebug(logPrefix + "curl exception: " + std::string(error.what()) + " assuming sender hasn't posted yet and will retry later.");
    } catch (nlohmann::json::exception &error) {
        logError(logPrefix + "json exception: " + std::string(error.what()));
    } catch (std::exception &error) {
        logError(logPrefix + "std exception: " + std::string(error.what()));
    }

    return nextFetchObjUuid;
}

std::string Link::postOnActionThread(const std::string &postObjUuid, const std::vector<RaceHandle> &handles, uint64_t actionId) {
    TRACE_METHOD(linkId, handles, actionId);
    logPrefix += linkId + ": ";

    std::string nextPostObjUuid = postObjUuid;
    auto iter = contentQueue.find(actionId);
    if (iter == contentQueue.end()) {
        // TODO why was this empty post necessary?
        // postToBucket(std::vector<uint8_t>(), postObjUuid);
        // We really shouldn't get here, since we already check for this before queueing the action,
        // but just in case...
        logError(logPrefix +
                 "no enqueued content for given action ID: " + std::to_string(actionId));
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return nextPostObjUuid;
    }

    int tries = 0;
    for (; tries < address.maxTries; ++tries) {
        if (postToBucket(iter->second, postObjUuid)) {
            break;
        }
    }

    if (tries == address.maxTries) {
        logError(logPrefix + "retry limit exceeded: post failed");
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
    } else {
        nextPostObjUuid = generateNextObjUuid(postObjUuid);
        updatePackageStatus(handles, PACKAGE_SENT);
    }
    return nextPostObjUuid;
}

void Link::updatePackageStatus(const std::vector<RaceHandle> &handles, PackageStatus status) {
    for (auto &handle : handles) {
        sdk->onPackageStatusChanged(handle, status);
    }
}


struct inc_copy_vec {
    size_t current_offset;
    const std::vector<uint8_t> *dataVec;
};
  
size_t read_callback(char *ptr, size_t size, size_t nmemb, inc_copy_vec *userdata)
{
  logDebug("read_callback called");
  size_t ncopied = 0;
  size_t remaining = userdata->dataVec->size() - userdata->current_offset;
  logDebug("remaining: " + std::to_string(remaining));
  if (remaining == 0) {
      logDebug("ncopied: " + std::to_string(ncopied));
      return ncopied;
  }

  ncopied = size * nmemb;
  if (ncopied > remaining) {
      ncopied = remaining;
  }
  memcpy(ptr, userdata->dataVec->data() + userdata->current_offset, ncopied);
  userdata->current_offset += ncopied;
 
  logDebug("src: " + std::string(userdata->dataVec->data(), userdata->dataVec->data() + ncopied));
  logDebug("dst: " + std::string(ptr, ptr+ncopied));
  logDebug("ncopied: " + std::to_string(ncopied));
  return size*nmemb;
}

bool Link::postToBucket(const std::vector<uint8_t> &message, const std::string &postObjUuid) {
    TRACE_METHOD(linkId);
    logPrefix += linkId + ": ";
    bool success = false;

    std::string url = "https://s3." + address.region + ".amazonaws.com/" + address.postBucket + "/" + postObjUuid;

    // TODO: RAII this thing
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: curl/7.86.0");

    try {
        CurlWrap curl;
        std::string response;

        logInfo(logPrefix + "Attempting to post to: " + url);
        curl.setopt(CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl.setopt(CURLOPT_UPLOAD, 1L);

        // connecton timeout. override the default and set to 10 seconds.
        curl.setopt(CURLOPT_CONNECTTIMEOUT, 10L);

        curl.setopt(CURLOPT_WRITEFUNCTION, WriteCallback);
        curl.setopt(CURLOPT_WRITEDATA, &response);
        curl.setopt(CURLOPT_HTTPHEADER, headers);
        curl.setopt(CURLOPT_INFILESIZE, message.size());
        
        struct inc_copy_vec curl_msg = {0, &message};
        curl_easy_setopt(curl, CURLOPT_READDATA, &curl_msg);
        curl.perform();
        // TODO: add XML parsing to check for application-level errors like:
        //            <?xml version="1.0" encoding="UTF-8"?>
        // <Error><Code>AccessDenied</Code><Message>Access Denied</Message><RequestId>FC1STS0GMRKHPCY8</RequestId><HostId>+hdFUV92l9lcRBvKIpXeuawSa3xJVKYT7Q3KfUFl/g41QNcsQTL0HES0Rk5yELLD/oUPQtkWtKM=</HostId></Error>
        logDebug(logPrefix + " post-response: " + response);
        // }
        success = true;
    } catch (curl_exception &error) {
        logWarning(logPrefix + "curl exception: " + std::string(error.what()));
    }

    curl_slist_free_all(headers);

    return success;
}


