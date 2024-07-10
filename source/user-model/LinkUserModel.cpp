
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

#include "LinkUserModel.h"

#include "JsonTypes.h"

static const double WAIT_TIME = 1.0;

LinkUserModel::LinkUserModel(const LinkID &linkId, std::atomic<uint64_t> &nextActionId) :
    linkId(linkId), nextActionId(nextActionId) {}

ActionTimeline LinkUserModel::getTimeline(Timestamp start, Timestamp end) {
    // First, remove all actions from the cached timeline that occur before the `start` time
    auto iter = std::find_if(cachedTimeline.begin(), cachedTimeline.end(),
                             [start](const auto &val) { return val.timestamp >= start; });
    cachedTimeline.erase(cachedTimeline.begin(), iter);

    Timestamp current = start;
    // If we still have cached actions, start at the timestamp of the last action
    if (not cachedTimeline.empty()) {
        current = cachedTimeline.back().timestamp + 10; // Delay starting to use links for 10s
    }

    // Then add new actions to the timeline until we reach the `end` time
    while (current < end) {
        nlohmann::json fetchAction = ActionJson{
          linkId,
          ACTION_FETCH
        };
        cachedTimeline.push_back({
            current,
            ++nextActionId,
            fetchAction.dump(),
          });
        current += WAIT_TIME;

        // nlohmann::json postAction = ActionJson{
        //   linkId,
        //   ACTION_POST
        // };
        // cachedTimeline.push_back({
        //     current,
        //     ++nextActionId,
        //     postAction.dump(),
        //   });

        // current += WAIT_TIME;
    }

    return cachedTimeline;
}
