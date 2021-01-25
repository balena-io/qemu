#!/bin/bash

set -x
set -o errexit

git checkout "$RELEASE_COMMIT"

TARGETS="arm aarch64"

for TARGET in $TARGETS; do
	BINARY_NAME="qemu-$TARGET-static"
	PACKAGE_NAME="qemu-$QEMU_VERSION-$TARGET"

	./configure --target-list="$TARGET-linux-user" --static --extra-cflags="-DCONFIG_RTNETLINK" \
		&& make -j $(nproc) \
		&& strip "build/$TARGET-linux-user/qemu-$TARGET" \
		&& mkdir -p "$PACKAGE_NAME" \
		&& cp "build/$TARGET-linux-user/qemu-$TARGET" "$PACKAGE_NAME/$BINARY_NAME"

	tar -cvzf "$PACKAGE_NAME.tar.gz" "$PACKAGE_NAME"
done

ARM_SHA256=$(sha256sum "qemu-$QEMU_VERSION-arm.tar.gz")
AARCH64_SHA256=$(sha256sum "qemu-$QEMU_VERSION-aarch64.tar.gz")
echo "arm: $ARM_SHA256, aarch64: $AARCH64_SHA256"

if [ -z "$ACCOUNT" ] || [ -z "$REPO" ] || [ -z "$ACCESS_TOKEN" ]; then
	echo "Please set value for ACCOUNT, REPO and ACCESS_TOKEN!"
	exit 1
fi

# Create a release
rm -f request.json response.json
cat > request.json <<-EOF
{
    "tag_name": "v$QEMU_VERSION",
    "target_commitish": "$RELEASE_BRANCH",
    "name": "v$QEMU_VERSION",
    "body": "Release of version v$QEMU_VERSION. \nqemu-$QEMU_VERSION-arm.tar.gz - SHA256: ${ARM_SHA256% *}. \nqemu-$QEMU_VERSION-aarch64.tar.gz - SHA256: ${AARCH64_SHA256% *}",
    "draft": false,
    "prerelease": false
}
EOF
curl --data "@request.json" --header "Content-Type:application/json" \
	"https://api.github.com/repos/$ACCOUNT/$REPO/releases?access_token=$ACCESS_TOKEN" \
	-o response.json
# Parse response
RELEASE_ID=$(cat response.json | jq '.id')
echo "RELEASE_ID=$RELEASE_ID"

# Upload data
curl -H "Authorization:token $ACCESS_TOKEN" -H "Content-Type:application/x-gzip" \
	--data-binary "@qemu-$QEMU_VERSION-arm.tar.gz" \
	"https://uploads.github.com/repos/$ACCOUNT/$REPO/releases/$RELEASE_ID/assets?name=qemu-$QEMU_VERSION-arm.tar.gz"

# Upload data
curl -H "Authorization:token $ACCESS_TOKEN" -H "Content-Type:application/x-gzip" \
	--data-binary "@qemu-$QEMU_VERSION-aarch64.tar.gz" \
	"https://uploads.github.com/repos/$ACCOUNT/$REPO/releases/$RELEASE_ID/assets?name=qemu-$QEMU_VERSION-aarch64.tar.gz"
