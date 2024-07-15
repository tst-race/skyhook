# **Skyhook - Cloud Storage-based Censorship Circumvention Plugin for Raceboat **

## **Introduction**

Skyhook is a plugin for the [Raceboat censorship circumvention framework](https://github.com/tst-race/raceboat) that provides the ability to send and receive messages transmitted through AWS S3 cloud storage without any _initial shared secrets_ and _without the client having an AWS account_. The design details and rationale are laid out in the [research paper](https://www.petsymposium.org/foci/2024/foci-2024-0011.pdf).

#### What Is Inside

Skyhook is implemented as a User Model and a pair of Transport components in the Raceboat decomposed channel APIs. It can be combined with an arbitrary Encoding component, since S3 objects support writing arbitrary content.

## **How To Build**

Skyhook follows a containerized build process based on the rest of the Raceboat framework. The below commands should build both the AWS SDK and the Skyhook code itself.

**NOTE 1**: the CPP version of the AWS SDK is included as a submodule, so git clone must be run with `--recurse-submodules` or the submodule must be initialized after cloning in order to build the "server" (Account Holder) side of the Skyhook code.

**NOTE 2**: the below build command will pull a Raceboat compile image which may take some time on the first execution.

**NOTE 3**: Skyhook is a _decomposed_ channel which uses an Encoding component provided by a different plugin, you can get that plugin [here]().

```bash

git clone https://github.com/tst-race/skyhook --recurse-submodules
cd skyhook
./build_artifacts_in_docker_image.sh

```

## **How To Run**

Skyhook has two distinct sides: a "client" or "PublicUser" side which does not need any account and a "server" or "AccountHolder" side which needs a paid AWS S3 account. The AccountHolder side needs to provide an AWS canonical ID for the account (to enable it to set private read/write permissions properly) and credentials for an AWS account (to authenticate with the AWS SDK and automate creation/deletion/permissioning of S3 buckets and objects).

The below commands use a dockerized runtime environment for ease of testing / portability.

### Server / AccountHolder Side

The below command assumes three directories exist:
1. `$(pwd)/kits` is assumed to contain the PluginSkyhook and PluginCommsTwoSixStubDecomposed plugin directories which will be loaded by Raceboat at runtime to actually perform the communication.
2. `$(pwd)/../aws-keys` is assumed to contain AWS config files.
3. `$(pwd)/scripts/example-responses.txt` containing individual lines of responses (e.g. bridge lines)

The `aws-keys` directory should contain the following:

A file named `config` containing data like:
```
[default]
region=us-west-2
output=json
```

A file named `credentials` containing data like:
```
[default]
aws_access_key_id=ASIAIOSFODNN7EXAMPLE
aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
aws_session_token = IQoJb3JpZ2luX2IQoJb3JpZ2luX2IQoJb3JpZ2luX2IQoJb3JpZ2luX2IQoJb3JpZVERYLONGSTRINGEXAMPLE
```

#### Run Command
Note the below command needs a few things replaced:
1. `YOUR CANONICAL ID` replaced by your AWS account canonical ID
2. `BUCKET OF CHOICE` replaced by an available bucket name of your choice

```bash
docker run --rm -it --name=rbserver --network=bridge \
       -v $(pwd)/kits:/server-kits \
       -v $(pwd)/../aws-keys:/root/.aws \
       -v $(pwd)/scripts/:/scripts/ \
       ghcr.io/tst-race/raceboat/raceboat:latest bash -c \
       'bridge-distro \
       --passphrase bridge-please \
       --responses-file /scripts/example-responses.txt \
       --dir /server-kits \
       --recv-channel=skyhookBasicComposition \
       --send-channel=skyhookBasicComposition \
       --param skyhookBasicComposition.canonicalId="YOUR CANONICAL ID" \
       --param skyhookBasicComposition.region="eu-west-2" \
       --recv-address="{\"region\": \"eu-west-2\", \"fetchBucket\": \"BUCKET OF YOUR CHOICE\", \"initialFetchObjUuid\": \"aws-public-private-serv-to-cli\", \"postBucket\": \"BUCKET OF YOUR CHOICE\", \"initialPostObjUuid\": \"aws-public-private-cli-to-serv\", \"maxTries\": 5, \"openObjects\": 1, \"singleReceive\": true}" --debug \
       | tee raceboat-distro.log \
       | grep -a -e "RECEIVED\|PASSPHRASE\|RESPONDING"'
```

This will run the raceboat bridge-distro mode that will repeatedly return one line from example-responses.txt each time someone makes a request to it with the message `bridge-please`.

### Client / PublicUser Side

The client side is simpler, since there is no AWS authentication. It just needs `$(pwd)/kits` to contain the PluginSkyhook and PluginCommsTwoSixStubDecomposed plugin directories which will be loaded by Raceboat at runtime to actually perform the communication.

```bash
docker run --rm -it --name=rbclient --network=bridge -v $(pwd)/../kits:/client-kits/ -v $(pwd):/code -w /code ghcr.io/tst-race/raceboat/raceboat:latest bash -c \
       'echo "bridge-please" | race-cli \
       --dir /client-kits \
       -m --send-recv \
       --send-channel skyhookBasicComposition \
       --recv-channel=skyhookBasicComposition \
       --param skyhookBasicComposition.region="eu-west-2" \
       --send-address="{\"region\": \"eu-west-2\", \
       \"fetchBucket\": \"eu-west-2-race-bucket-10\", \
       \"initialFetchObjUuid\": \"aws-public-private-serv-to-cli\", \
       \"postBucket\": \"eu-west-2-race-bucket-10\", \
       \"initialPostObjUuid\": \"aws-public-private-cli-to-serv\", \
       \"maxTries\": 5, \
       \"openObjects\": 1, \
       \"singleReceive\": true}" \
       --quiet'
```
