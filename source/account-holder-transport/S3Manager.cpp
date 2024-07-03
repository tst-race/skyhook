
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

#include "S3Manager.h"

#include <iostream>
#include "log.h"
#include <nlohmann/json.hpp>
#include <aws/core/VersionConfig.h>
#include <aws/core/Aws.h>
#include <aws/s3/S3ClientConfiguration.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/AccessControlPolicy.h>
#include <aws/s3/model/GetBucketPolicyRequest.h>
#include <aws/s3/model/PutBucketPolicyRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/PutPublicAccessBlockRequest.h>
#include <aws/s3/model/PublicAccessBlockConfiguration.h>

#define PUBLIC_PUTTABLE_STRING "public-puttable-"
#define PUBLIC_GETTABLE_STRING "public-gettable-"
#define PRIVATE_PUTTABLE_STRING "private-puttable-"
#define PRIVATE_GETTABLE_STRING "private-gettable-"


S3Manager::S3Manager() :
  selfPrincipal("bce78df867026bc9f7f8ff98567367bf8c575682e36c097f884d9c48f366092f"),
  policyJson({ {"Version", "2012-10-17"}, {"Id", "RacebucketPolicy"}, {"Statement", {
  }} }) {
}

bool S3Manager::addObjPermission(const std::string &uuid,
                                 const std::string &bucket,
                                 const std::string &statementKey,
                                 const std::string &permission,
                                 const std::string &principal) {
  std::lock_guard<std::mutex> lock{policyLock};
  TRACE_METHOD(uuid, bucket, statementKey, permission);

  auto statementItems = policyJson["Statement"].items();
  auto statement = std::find_if(begin(statementItems), end(statementItems),
                                [&statementKey](const auto &obj){ return obj.value()["Sid"] == statementKey; });

  std::string resourceToAdd = "arn:aws:s3:::" + bucket + "/" + uuid;
  if (statement == end(statementItems)) {
    logInfo("Could not find " + statementKey + " in policy JSON, adding it");
    nlohmann::json statementJson;
    if (principal == "*") {
     statementJson = {
       {"Action", {permission}},
       {"Effect", "Allow"}, {"Principal", "*"},
       {"Resource", {resourceToAdd}},
       {"Sid", statementKey}
     };
    }
    else {
     statementJson = {
       {"Action", {permission}},
       {"Effect", "Allow"}, {"Principal", {{"CanonicalUser", principal}}},
       {"Resource", {resourceToAdd}},
       {"Sid", statementKey}
     };
    }
    policyJson["Statement"].push_back(statementJson);
  } else {
    size_t statementIdx = static_cast<size_t>(std::stoi(statement.key()));
    policyJson["Statement"][statementIdx]["Resource"].push_back(resourceToAdd);   
  }


  updatePolicy(bucket);
  return true;
 
}

bool S3Manager::makeObjGettable(const std::string &uuid, const LinkAddress &address) {
  TRACE_METHOD(uuid, address);
  return addObjPermission(uuid, address.postBucket, PUBLIC_GETTABLE_STRING + address.initialPostObjUuid, "s3:GetObject", "*");
}

bool S3Manager::makeObjPuttable(const std::string &uuid, const LinkAddress &address) {
  TRACE_METHOD(uuid, address);
  return (deleteObject(address.postBucket, uuid) and
          addObjPermission(uuid, address.postBucket,
                           PUBLIC_PUTTABLE_STRING + address.initialFetchObjUuid, "s3:PutObject", "*"));
}

bool S3Manager::removeObjPermission(const std::string &uuid,
                                    const std::string &bucket,
                                    const std::string &statementKey) {
  std::lock_guard<std::mutex> lock{policyLock};
  TRACE_METHOD(uuid, bucket, statementKey);

  std::string resourceToRemove = "arn:aws:s3:::" + bucket + "/" + uuid;
  auto statementItems = policyJson["Statement"].items();
  auto statement = std::find_if(begin(statementItems), end(statementItems),
                                [&statementKey](const auto &obj){ return obj.value()["Sid"] == statementKey; });
  if (statement == end(statementItems)) {
    logError("Could not find " + statementKey + " in policy JSON: " + policyJson["Statement"].dump());
    return false;
  }

  size_t statementIdx = static_cast<size_t>(std::stoi(statement.key()));
  auto resourceItems = policyJson["Statement"][statementIdx]["Resource"].items();
  auto resource = std::find_if(begin(resourceItems), end(resourceItems),
                                [&resourceToRemove](const auto &obj){ return obj.value() == resourceToRemove; });
  if (resource == end(resourceItems)) {
    logError("Could not find " + resourceToRemove + " in policy JSON: " + statement.value().dump());
    return false;
  }

  size_t resourceIdx = static_cast<size_t>(std::stoi(resource.key()));
  policyJson["Statement"][statementIdx]["Resource"].erase(resourceIdx);

  if (policyJson["Statement"][statementIdx]["Resource"].empty()) {
    logInfo("Removed last puttable object, deleting policy statement: " + statementKey);
    policyJson["Statement"].erase(statementIdx);
  }

  updatePolicy(bucket);
  return true;
}

bool S3Manager::makeObjUngettable(const std::string &uuid, const LinkAddress &address) {
  TRACE_METHOD(uuid, address);
  const std::string statementKey = PUBLIC_GETTABLE_STRING + address.initialPostObjUuid;
  return (deleteObject(address.postBucket, uuid) and
          removeObjPermission(uuid, address.postBucket, statementKey));
}

bool S3Manager::makeObjUnputtable(const std::string &uuid, const LinkAddress &address) {
  TRACE_METHOD(uuid, address);
  const std::string statementKey = PUBLIC_PUTTABLE_STRING + address.initialFetchObjUuid;

  return (deleteObject(address.fetchBucket, uuid) and
          removeObjPermission(uuid, address.fetchBucket, statementKey));
}


Aws::String GetPolicyString(const Aws::String &bucket) {
    return
            "{\n"
            "   \"Version\":\"2012-10-17\",\n"
            "   \"Statement\":[\n"
            "       {\n"
            "           \"Sid\": \"1\",\n"
            "           \"Effect\": \"Allow\",\n"
            "           \"Principal\": \"*\",\n"
            "           \"Action\": [ \"s3:GetObject\" ],\n"
            "           \"Resource\": [ \"arn:aws:s3:::"
            + bucket +
            "/*\" ]\n"
            "       }\n"
            "   ]\n"
            "}";
}

bool S3Manager::createBucket(const std::string &bucketName, const std::string &region) {
    TRACE_METHOD(bucketName, region);
    Aws::S3::Model::CreateBucketRequest request;
    request.SetBucket(bucketName);

    //TODO(user): Change the bucket location constraint enum to your target Region.
    Aws::S3::Model::CreateBucketConfiguration createBucketConfig;
    if (region != "us-east-1") {
      createBucketConfig.SetLocationConstraint(
                                               Aws::S3::Model::BucketLocationConstraintMapper::GetBucketLocationConstraintForName(
                                                                                                                                  region));
    }
    request.SetCreateBucketConfiguration(createBucketConfig);

    Aws::S3::Model::CreateBucketOutcome outcome = s3Client.CreateBucket(request);
    if (!outcome.IsSuccess()) {
        auto err = outcome.GetError();
        logError("Error: CreateBucket: " + err.GetExceptionName() + ": " + err.GetMessage());
    }
    else {
      logInfo("Created bucket " + bucketName + " in the specified AWS Region.");
      Aws::S3::Model::PutPublicAccessBlockRequest pabRequest = Aws::S3::Model::PutPublicAccessBlockRequest().WithPublicAccessBlockConfiguration(Aws::S3::Model::PublicAccessBlockConfiguration().WithBlockPublicAcls(false)).WithBucket(bucketName);
      Aws::S3::Model::PutPublicAccessBlockOutcome pabOutcome = s3Client.PutPublicAccessBlock(pabRequest);
      if (!pabOutcome.IsSuccess()) {
        auto err = outcome.GetError();
        logError("Error: PutPublicAccessBlock: " + err.GetExceptionName() + ": " + err.GetMessage());
      }
    else {
      logInfo("Successfully PutPublicAccessBlock for " + bucketName);
    }
    }

    return outcome.IsSuccess();
}

bool S3Manager::updatePolicy(const std::string &bucketName) {
  TRACE_METHOD(bucketName);
  logInfo("New Policy: " + policyJson.dump());
 
  std::shared_ptr<Aws::StringStream> request_body =
    Aws::MakeShared<Aws::StringStream>("");
  *request_body << policyJson;
    
  Aws::S3::Model::PutBucketPolicyRequest request;
  request.SetBucket(bucketName);
  request.SetBody(request_body);
  logInfo("updating to: " + bucketName);
  
  Aws::S3::Model::PutBucketPolicyOutcome outcome =
    s3Client.PutBucketPolicy(request);
  
    if (!outcome.IsSuccess()) {
      const Aws::S3::S3Error &err = outcome.GetError();
      logError("Error: PutBucketPolicyRequest: "
               + err.GetExceptionName() + ": " + err.GetMessage());
    }

    return outcome.IsSuccess();
}

bool S3Manager::getObject(const std::string &bucketName,
                          const std::string &objectUuid,
                          std::vector<uint8_t> &data) {
  TRACE_METHOD(bucketName, objectUuid);

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucketName);
    request.SetKey(objectUuid);

    Aws::S3::Model::GetObjectOutcome outcome = s3Client.GetObject(request);

    if (!outcome.IsSuccess()) {
        const Aws::S3::S3Error &err = outcome.GetError();
        logInfo("Error: GetObject(" + bucketName + "/" + objectUuid + "): " + 
                 err.GetExceptionName() + ": " + err.GetMessage());
        return false;
    }
    else {
        logInfo("Successfully retrieved " + bucketName + "/" + objectUuid);

        auto result = outcome.GetResultWithOwnership();
        if (result.GetContentLength() == 0) {
          return false;
        }
        std::ostringstream ss;
        ss << result.GetBody().rdbuf();

        std::string str = ss.str();
        std::copy(str.begin(), str.end(), std::back_inserter(data));
        logInfo("Retrieved Data: " + std::string(data.begin(), data.end()));
       
        return true;
    }
}

bool S3Manager::deleteObject(const std::string &bucketName,
                             const std::string &objectUuid) {
    TRACE_METHOD(bucketName, objectUuid);

    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(bucketName);
    request.SetKey(objectUuid);

    Aws::S3::Model::DeleteObjectOutcome outcome = s3Client.DeleteObject(request);

    if (!outcome.IsSuccess()) {
        const Aws::S3::S3Error &err = outcome.GetError();
        logError("Error: DeleteObject(" + bucketName + "/" + objectUuid + "): " + 
                 err.GetExceptionName() + ": " + err.GetMessage());
        return false;
    }
    else {
        logInfo("Successfully deleted " + bucketName + "/" + objectUuid);
        return true;
    }
}

bool S3Manager::putObject(const std::string &bucketName,
                          const std::string &objectUuid,
                          std::vector<uint8_t> &data) {
  TRACE_METHOD(bucketName, objectUuid);

  logInfo("Data to be posted: " + std::string(data.begin(), data.end()));
  Aws::S3::Model::PutObjectRequest request;
  request.SetBucket(bucketName);
  request.SetKey(objectUuid);
  // std::shared_ptr<std::stringstream> input_data = std::make_shared<std::stringstream>(reinterpret_cast<const char*>(data.data()));
  std::shared_ptr<std::stringstream> input_data = std::make_shared<std::stringstream>(std::string(data.begin(), data.end()));
  logInfo("Putting object: " + input_data->str());
  request.SetBody(input_data);

  Aws::S3::Model::PutObjectOutcome outcome =
    s3Client.PutObject(request);

  if (!outcome.IsSuccess()) {
    logError("Error: PutObject: " + outcome.GetError().GetMessage());
    return false;
  }
  return true;
}

