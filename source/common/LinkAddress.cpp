
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

#include "LinkAddress.h"

void to_json(nlohmann::json &destJson, const LinkAddress &srcLinkAddress) {
    destJson = nlohmann::json{
        // clang-format off
        {"region", srcLinkAddress.region},
        {"fetchBucket", srcLinkAddress.fetchBucket},
        {"initialFetchObjUuid", srcLinkAddress.initialFetchObjUuid},
        {"postBucket", srcLinkAddress.postBucket},
        {"initialPostObjUuid", srcLinkAddress.initialPostObjUuid},
        {"openObjects", srcLinkAddress.openObjects},
        {"maxTries", srcLinkAddress.maxTries},
        // clang-format on
    };
}

void from_json(const nlohmann::json &srcJson, LinkAddress &destLinkAddress) {
    // Required
    srcJson.at("region").get_to(destLinkAddress.region);
    srcJson.at("fetchBucket").get_to(destLinkAddress.fetchBucket);
    srcJson.at("initialFetchObjUuid").get_to(destLinkAddress.initialFetchObjUuid);
    srcJson.at("postBucket").get_to(destLinkAddress.postBucket);
    srcJson.at("initialPostObjUuid").get_to(destLinkAddress.initialPostObjUuid);
    // Optional
    destLinkAddress.openObjects = srcJson.value("openObjects", destLinkAddress.openObjects);
    destLinkAddress.maxTries = srcJson.value("maxTries", destLinkAddress.maxTries);
}
