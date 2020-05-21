#!/bin/bash
set -e
set -o pipefail

docker build -t qemu-builder .

docker run --rm -e ACCOUNT=$ACCOUNT \
				-e REPO=$REPO \
				-e ACCESS_TOKEN=$ACCESS_TOKEN \
				-e QEMU_VERSION=$QEMU_VERSION \
				-e RELEASE_BRANCH=$RELEASE_BRANCH \
				-e RELEASE_COMMIT=$RELEASE_COMMIT qemu-builder
