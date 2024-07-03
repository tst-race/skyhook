
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

#ifndef __COMMS_TWOSIX_TRANSPORT_LINK_ACCOUNT_HOLDER_H__
#define __COMMS_TWOSIX_TRANSPORT_LINK_ACCOUNT_HOLDER_H__

#include <ComponentTypes.h>
#include <LinkProperties.h>
#include <PackageStatus.h>
#include <SdkResponse.h>  // RaceHandle

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Link.h"
class SkyhookTransportAccountHolder;
// #include "SkyhookTransport.h"

class ITransportSdk;

/**
 * @brief A Instance of a link within the twoSixIndirectCpp transport
 */
class LinkAccountHolder : public Link {
public:
    LinkAccountHolder(const LinkID &linkId, const LinkAddress &address, const LinkProperties &properties, bool isCreator, SkyhookTransportAccountHolder *transport, ITransportSdk *sdk);

    virtual ~LinkAccountHolder();

protected:
    virtual std::string fetchOnActionThread(const std::string &fetchObjUuid) override; 
    virtual std::string postOnActionThread(const std::string &postObjUuid, const std::vector<RaceHandle> &handles, uint64_t actionId) override;

    bool creator;
    SkyhookTransportAccountHolder *accountHolderTransport;

  std::deque<std::string> puttableUuids; // objects publicly writable
  std::deque<std::string> fetchableUuids; // objects publicy readable
};

#endif  //  __COMMS_TWOSIX_TRANSPORT_LINK_ACCOUNT_HOLDER_H__
