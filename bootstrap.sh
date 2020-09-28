#!/usr/bin/env bash

IMAGE_TAG="barectf-win32-platform"

CURDIR=$(dirname "$0")
docker build --build-arg BARECTF_IMAGE_TAG="$IMAGE_TAG" -t "$IMAGE_TAG" "$CURDIR"
if [[ $? -ne 0 ]]; then
    exit $?
fi

docker run --rm "$IMAGE_TAG" > build.sh
if [[ $? -ne 0 ]]; then
    exit $?
fi

chmod +x build.sh
