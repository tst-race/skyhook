
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

#ifndef __SKYHOOK_TRANSPORT_H__
#define __SKYHOOK_TRANSPORT_H__

#include <ChannelProperties.h>
#include <ITransportComponent.h>
#include <LinkProperties.h>

#include <atomic>

#include "LinkMap.h"

enum SkyhookRole {
    BR_UNDEF = 0,
    BR_PUBLIC_USER = 1,
    BR_ACCOUNT_HOLDER = 2,
};
std::string skyhookRoleToString(SkyhookRole skyhookRole);
SkyhookRole stringToSkyhookRole(const std::string &roleName);



class SkyhookTransport : public ITransportComponent {
public:
    explicit SkyhookTransport(ITransportSdk *sdk, const std::string &roleName);

    virtual ComponentStatus onUserInputReceived(RaceHandle handle, bool answered,
                                                const std::string &response) override;

    virtual TransportProperties getTransportProperties() override;

    virtual LinkProperties getLinkProperties(const LinkID &linkId) override;

    virtual ComponentStatus createLink(RaceHandle handle, const LinkID &linkId) override;

    virtual ComponentStatus loadLinkAddress(RaceHandle handle, const LinkID &linkId,
                                            const std::string &linkAddress) override;

    virtual ComponentStatus loadLinkAddresses(RaceHandle handle, const LinkID &linkId,
                                              const std::vector<std::string> &linkAddress) override;

    virtual ComponentStatus createLinkFromAddress(RaceHandle handle, const LinkID &linkId,
                                                  const std::string &linkAddress) override;

    virtual ComponentStatus destroyLink(RaceHandle handle, const LinkID &linkId) override;

    virtual std::vector<EncodingParameters> getActionParams(const Action &action) override;

    virtual ComponentStatus enqueueContent(const EncodingParameters &params, const Action &action,
                                           const std::vector<uint8_t> &content) override;

    virtual ComponentStatus dequeueContent(const Action &action) override;

    virtual ComponentStatus doAction(const std::vector<RaceHandle> &handles,
                                     const Action &action) override;

    // virtual bool makeObjPuttable(const std::string &uuid, const std::string &bucket);
  
    // TODO make unPUT/GETable
  
protected:
    virtual void handleUserInputResponse(RaceHandle handle, bool answered,
                                         const std::string &response);
    virtual std::shared_ptr<Link> createLinkInstance(const LinkID &linkId,
                                                     const LinkAddress &address,
                                                     const LinkProperties &properties,
                                                     bool isCreator);
    virtual std::string generateRandomString(int byteSsize);

    ITransportSdk *sdk;
    std::string racePersona;
    ChannelProperties channelProperties;
    LinkProperties defaultLinkProperties;

    LinkMap links;

    SkyhookRole role;
  
    bool preLinkCreate(const std::string &logPrefix, RaceHandle handle, const LinkID &linkId,
                       LinkSide invalidRoleLinkSide);
    ComponentStatus postLinkCreate(const std::string &logPrefix, RaceHandle handle,
                                   const LinkID &linkId, const std::shared_ptr<Link> &link,
                                   LinkStatus linkStatus);

    bool ready;
    bool firstCreatedIsSingleReceive;
    RaceHandle regionReqHandle;
    RaceHandle bucketReqHandle;
    RaceHandle seedReqHandle;
    RaceHandle singleReceiveReqHandle;
    std::string region;
    std::string bucket;
    std::string seed;
};

#endif  // __SKYHOOK_TRANSPORT_H__
