
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

#ifndef __SKYHOOK_TRANSPORT_ACCOUNT_HOLDER_H__
#define __SKYHOOK_TRANSPORT_ACCOUNT_HOLDER_H__

#include "SkyhookTransport.h"
#include <ChannelProperties.h>
#include <ITransportComponent.h>
#include <LinkProperties.h>
#include "S3Manager.h"
#include <aws/core/VersionConfig.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>

#include <atomic>

#include "LinkMap.h"
#include "LinkAccountHolder.h"

class SkyhookTransportAccountHolder : public SkyhookTransport {
public:
    explicit SkyhookTransportAccountHolder(ITransportSdk *sdk, const std::string &roleName);

    S3Manager s3Manager;
  
protected:
    virtual std::shared_ptr<Link> createLinkInstance(const LinkID &linkId,
                                                     const LinkAddress &address,
                                                     const LinkProperties &properties,
                                                     bool isCreator) override;
};

#endif  // __SKYHOOK_TRANSPORT_ACCOUNT_HOLDER_H__
