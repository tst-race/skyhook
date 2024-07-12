
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

#ifndef __S3_MANAGER_H__
#define __S3_MANAGER_H__

#include <atomic>
#include <aws/core/VersionConfig.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include "LinkAddress.h"
#include <nlohmann/json.hpp>
#include <mutex>          // std::mutex, std::lock_guard

class S3Manager {
public:
  S3Manager();
  virtual ~S3Manager() {
      // Aws::ShutdownAPI(options);
  }
  virtual bool makeObjGettable(const std::string &uuid, const LinkAddress &address);
  virtual bool makeObjPuttable(const std::string &uuid, const LinkAddress &address);
  virtual bool makeObjUngettable(const std::string &uuid, const LinkAddress &address);
  virtual bool makeObjUnputtable(const std::string &uuid, const LinkAddress &address);
  virtual bool addObjPermission(const std::string &uuid,
                                const std::string &bucket,
                                const std::string &statementKey,
                                const std::string &permission,
                                const std::string &principal);
  virtual bool removeObjPermission(const std::string &uuid, const std::string &bucket, const std::string &statementKey) ;
  virtual bool createBucket(const std::string &bucketName, const std::string &region);
  virtual bool deleteBucket(const std::string &bucketName, const std::string &region);
  // virtual bool GetBucketPolicy(const std::string &bucketName);
  virtual bool getObject(const std::string &bucketName,
                          const std::string &objectUuid,
                         std::vector<uint8_t> &data);
  virtual bool deleteObject(const std::string &bucketName,
                             const std::string &objectUuid);
  virtual bool putObject(const std::string &bucketName,
                          const std::string &objectUuid,
                          std::vector<uint8_t> &data);


  std::string selfPrincipal;
private:
  virtual bool updatePolicy(const std::string &bucket);
  
  std::unordered_map<std::string, nlohmann::json> policyJsonMap;
  Aws::S3::S3Client s3Client;
  std::mutex policyLock;
};

#endif  // __S3_MANAGER_H__
