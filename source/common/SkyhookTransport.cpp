
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

#include "SkyhookTransport.h"

#include <chrono>
#include <nlohmann/json.hpp>

#include "JsonTypes.h"
#include "Link.h"
#include "LinkAddress.h"
#include "log.h"
#include <iostream>



std::string skyhookRoleToString(SkyhookRole skyhookRole) {
    switch (skyhookRole) {
        case BR_UNDEF:
            return "UNDEF";
        case BR_PUBLIC_USER:
            return "PUBLIC_USER";
        case BR_ACCOUNT_HOLDER:
            return "ACCOUNT_HOLDER";
        default:
            return "UNDEF";
    }
}

SkyhookRole stringToSkyhookRole(const std::string &roleName) {
    if (roleName == "PUBLIC_USER") {
        return BR_PUBLIC_USER;
    }
    if (roleName == "ACCOUNT_HOLDER") {
        return BR_ACCOUNT_HOLDER;
    }
    return BR_UNDEF;
}

namespace std {
static std::ostream &operator<<(std::ostream &out, const std::vector<RaceHandle> &handles) {
    return out << nlohmann::json(handles).dump();
}
}  // namespace std


LinkProperties createDefaultLinkProperties(const ChannelProperties &channelProperties) {
    LinkProperties linkProperties;

    linkProperties.linkType = LT_BIDI;
    linkProperties.transmissionType = channelProperties.transmissionType;
    linkProperties.connectionType = channelProperties.connectionType;
    linkProperties.sendType = channelProperties.sendType;
    linkProperties.reliable = channelProperties.reliable;
    linkProperties.isFlushable = channelProperties.isFlushable;
    linkProperties.duration_s = channelProperties.duration_s;
    linkProperties.period_s = channelProperties.period_s;
    linkProperties.mtu = channelProperties.mtu;

    LinkPropertySet worstLinkPropertySet;
    worstLinkPropertySet.bandwidth_bps = 277200;
    worstLinkPropertySet.latency_ms = 3190;
    worstLinkPropertySet.loss = 0.1;
    linkProperties.worst.send = worstLinkPropertySet;
    linkProperties.worst.receive = worstLinkPropertySet;

    linkProperties.expected = channelProperties.creatorExpected;

    LinkPropertySet bestLinkPropertySet;
    bestLinkPropertySet.bandwidth_bps = 338800;
    bestLinkPropertySet.latency_ms = 2610;
    bestLinkPropertySet.loss = 0.1;
    linkProperties.best.send = bestLinkPropertySet;
    linkProperties.best.receive = bestLinkPropertySet;

    linkProperties.supported_hints = channelProperties.supported_hints;
    linkProperties.channelGid = channelProperties.channelGid;

    return linkProperties;
}

SkyhookTransport::SkyhookTransport(ITransportSdk *sdk, const std::string &roleName) :
    sdk(sdk),
    racePersona(sdk->getActivePersona()),
    channelProperties(sdk->getChannelProperties()),
    defaultLinkProperties(createDefaultLinkProperties(channelProperties)),
    role(stringToSkyhookRole(roleName)),
    ready(false),
    regionReqHandle(sdk->requestPluginUserInput("region", "What AWS region is the S3 bucket located in?", true).handle),
    bucketReqHandle(sdk->requestPluginUserInput("bucket", "What is the name of the S3 bucket?", true).handle),
    seedReqHandle(sdk->requestPluginUserInput("seed", "Enter a random string", true).handle) {}

ComponentStatus SkyhookTransport::onUserInputReceived(RaceHandle handle, bool answered,
                                                                  const std::string &response) {
    TRACE_METHOD(handle, answered, response);

    if (handle == regionReqHandle) {
      regionReqHandle = NULL_RACE_HANDLE;
      if (answered) {
      region = response;
      } else {
        region = "us-east-1";
      }
    }
    if (handle == bucketReqHandle) {
      bucketReqHandle = NULL_RACE_HANDLE;
      if (answered) {
        bucket = response;
      } else {
        bucket = "race-bucket-3";
      }
    }
    if (handle == seedReqHandle) {
      seedReqHandle = NULL_RACE_HANDLE;
      if (answered) {
        seed = response;
      } else {
        seed = "seed";
      }
    }

    // if (!bucket.empty() and !region.empty() and !seed.empty()) 
    if (regionReqHandle == NULL_RACE_HANDLE and
        bucketReqHandle == NULL_RACE_HANDLE and
        seedReqHandle == NULL_RACE_HANDLE) {
        ready = true;
        sdk->updateState(COMPONENT_STATE_STARTED);
    }
    return COMPONENT_OK;
}

TransportProperties SkyhookTransport::getTransportProperties() {
    TRACE_METHOD();
    return {
        // supportedActions
        {
            {"post", {"*/*"}},
            {"fetch", {}},
        },
    };
}

LinkProperties SkyhookTransport::getLinkProperties(const LinkID &linkId) {
    TRACE_METHOD(linkId);
    return links.get(linkId)->getProperties();
}

bool SkyhookTransport::preLinkCreate(const std::string &logPrefix, RaceHandle handle,
                                                 const LinkID &linkId,
                                                 LinkSide invalideRoleLinkSide) {
    int numLinks = links.size();
    if (numLinks >= channelProperties.maxLinks) {
        logError(logPrefix + "preLinkCreate: Too many links. links: " + std::to_string(numLinks) +
                 ", maxLinks: " + std::to_string(channelProperties.maxLinks));
        sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return false;
    }

    if (channelProperties.currentRole.linkSide == LS_UNDEF ||
        channelProperties.currentRole.linkSide == invalideRoleLinkSide) {
        logError(logPrefix + "preLinkCreate: Invalid role for this call. currentRole: '" +
                 channelProperties.currentRole.roleName +
                 "' linkSide: " + linkSideToString(channelProperties.currentRole.linkSide));
        sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return false;
    }

    return true;
}

ComponentStatus SkyhookTransport::postLinkCreate(const std::string &logPrefix,
                                                             RaceHandle handle,
                                                             const LinkID &linkId,
                                                             const std::shared_ptr<Link> &link,
                                                             LinkStatus linkStatus) {
  logInfo(logPrefix + "called");
    if (link == nullptr) {
        logError(logPrefix + "postLinkCreate: link was null");
        sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return COMPONENT_ERROR;
    }

    logInfo(logPrefix + "checking bucket");
    if (bucket.empty()) {
      if (not link->address.fetchBucket.empty()) {
        bucket = link->address.fetchBucket;
      }
      else if (not link->address.postBucket.empty()) {
        bucket = link->address.postBucket;
      }
    }

    logInfo(logPrefix + "adding link");
    links.add(link);
    logInfo(logPrefix + "updating status " + std::to_string(linkStatus));
    sdk->onLinkStatusChanged(handle, linkId, linkStatus, {});

    return COMPONENT_OK;
}

std::shared_ptr<Link> SkyhookTransport::createLinkInstance(
  const LinkID &linkId, const LinkAddress &address, const LinkProperties &properties, bool isCreator) {
    auto link = std::make_shared<Link>(linkId, address, properties, isCreator, this, sdk);
    link->start();
    return link;
}

ComponentStatus SkyhookTransport::createLink(RaceHandle handle, const LinkID &linkId) {
    TRACE_METHOD(handle, linkId);
    if (not preLinkCreate(logPrefix, handle, linkId, LS_LOADER)) {
        return COMPONENT_ERROR;
    }

    LinkAddress address;
    address.region = region; // TODO
    address.fetchBucket = bucket; // TODO
    address.initialFetchObjUuid = Link::generateNextObjUuid("fetch" + seed);
    address.postBucket = bucket; // TODO
    address.initialPostObjUuid = Link::generateNextObjUuid("post" + seed);
    logInfo("Created new Link, address:" + nlohmann::json(address).dump());

    LinkProperties properties;

    bool isCreator = true;
    auto link = createLinkInstance(linkId, address, properties, isCreator);

    return postLinkCreate(logPrefix, handle, linkId, link, LINK_CREATED);
}

ComponentStatus SkyhookTransport::loadLinkAddress(RaceHandle handle,
                                                              const LinkID &linkId,
                                                              const std::string &linkAddress) {
    TRACE_METHOD(handle, linkId, linkAddress);
    if (not preLinkCreate(logPrefix, handle, linkId, LS_CREATOR)) {
        return COMPONENT_OK;
    }
    logInfo(logPrefix + "preLinkCreate finished");

    LinkAddress address = nlohmann::json::parse(linkAddress);
    LinkProperties properties = defaultLinkProperties;
    bool isCreator = false;
    auto link = createLinkInstance(linkId, address, properties, isCreator);
    logInfo(logPrefix + "createLinkInstance finished");

    return postLinkCreate(logPrefix, handle, linkId, link, LINK_LOADED);
}

ComponentStatus SkyhookTransport::loadLinkAddresses(
    RaceHandle handle, const LinkID &linkId, const std::vector<std::string> & /* linkAddresses */) {
    TRACE_METHOD(handle, linkId);

    // We do not support multi-address loading
    sdk->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
    return COMPONENT_ERROR;
}

ComponentStatus SkyhookTransport::createLinkFromAddress(
    RaceHandle handle, const LinkID &linkId, const std::string &linkAddress) {
    TRACE_METHOD(handle, linkId, linkAddress);
    if (not preLinkCreate(logPrefix, handle, linkId, LS_LOADER)) {
        return COMPONENT_OK;
    }

    LinkAddress address = nlohmann::json::parse(linkAddress);
    LinkProperties properties = defaultLinkProperties;
    bool isCreator = true;
    auto link = createLinkInstance(linkId, address, properties, isCreator);

    return postLinkCreate(logPrefix, handle, linkId, link, LINK_CREATED);
}

ComponentStatus SkyhookTransport::destroyLink(RaceHandle handle, const LinkID &linkId) {
    TRACE_METHOD(handle, linkId);

    auto link = links.remove(linkId);
    if (not link) {
        logError(logPrefix + "link with ID '" + linkId + "' does not exist");
        return COMPONENT_ERROR;
    }

    link->shutdown();

    return COMPONENT_OK;
}

std::vector<EncodingParameters> SkyhookTransport::getActionParams(
    const Action &action) {
    TRACE_METHOD(action.actionId, action.json);

    try {
        ActionJson actionParams = nlohmann::json::parse(action.json);
        switch (actionParams.type) {
            case ACTION_FETCH:
                return {};
            case ACTION_POST:
                return {{actionParams.linkId, "*/*", true, {}}};
            default:
                logError(logPrefix +
                         "Unrecognized action type: " + nlohmann::json(actionParams.type).dump());
                break;
        }
    } catch (nlohmann::json::exception &err) {
        logError(logPrefix + "Error in action JSON: " + err.what());
    }

    sdk->updateState(COMPONENT_STATE_FAILED);
    return {};
}

ComponentStatus SkyhookTransport::enqueueContent(
    const EncodingParameters & /* params */, const Action &action,
    const std::vector<uint8_t> &content) {
    TRACE_METHOD(action.actionId, action.json, content.size());

    if (content.empty()) {
        logDebug(logPrefix + "Skipping enqueue content. Content size is 0.");
        return COMPONENT_OK;
    }

    try {
        ActionJson actionParams = nlohmann::json::parse(action.json);
        switch (actionParams.type) {
            case ACTION_FETCH:
                // Nothing to be queued
                return COMPONENT_OK;

            case ACTION_POST:
                return links.get(actionParams.linkId)->enqueueContent(action.actionId, content);

            default:
                logError(logPrefix +
                         "Unrecognized action type: " + nlohmann::json(actionParams.type).dump());
                break;
        }
    } catch (nlohmann::json::exception &err) {
        logError(logPrefix + "Error in action JSON: " + err.what());
    }

    return COMPONENT_ERROR;
}

ComponentStatus SkyhookTransport::dequeueContent(const Action &action) {
    TRACE_METHOD(action.actionId);

    try {
        ActionJson actionParams = nlohmann::json::parse(action.json);
        switch (actionParams.type) {
            case ACTION_POST:
                return links.get(actionParams.linkId)->dequeueContent(action.actionId);

            default:
                // No content associated with any other action types
                return COMPONENT_OK;
        }
    } catch (nlohmann::json::exception &err) {
        logError(logPrefix + "Error in action JSON: " + err.what());
    }

    return COMPONENT_ERROR;
}

ComponentStatus SkyhookTransport::doAction(const std::vector<RaceHandle> &handles,
                                                       const Action &action) {
    TRACE_METHOD(handles, action.actionId);

    try {
        ActionJson actionParams = nlohmann::json::parse(action.json);
        switch (actionParams.type) {
            case ACTION_FETCH:
                return links.get(actionParams.linkId)->fetch(std::move(handles));

            case ACTION_POST:
                return links.get(actionParams.linkId)->post(std::move(handles), action.actionId);

            default:
                logError(logPrefix +
                         "Unrecognized action type: " + nlohmann::json(actionParams.type).dump());
                break;
        }
    } catch (nlohmann::json::exception &err) {
        logError(logPrefix + "Error in action JSON: " + err.what());
    } catch (std::out_of_range &err) {
        logDebug(logPrefix + "Link for action is gone, likely shutting down " + err.what());
        return COMPONENT_OK;
    }


    return COMPONENT_ERROR;
}

const RaceVersionInfo raceVersion = RACE_VERSION;
