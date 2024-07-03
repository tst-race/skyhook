
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

#ifndef __SKYHOOK_TRANSPORT_PUBLIC_USER_H__
#define __SKYHOOK_TRANSPORT_PUBLIC_USER_H__

#include <ChannelProperties.h>
#include <ITransportComponent.h>
#include <LinkProperties.h>

#include <atomic>

#include "SkyhookTransport.h"

class SkyhookTransportPublicUser : public SkyhookTransport {
public:
    explicit SkyhookTransportPublicUser(ITransportSdk *sdk, const std::string &roleName);

};

#endif  // __SKYHOOK_TRANSPORT_PUBLIC_USER_H__
