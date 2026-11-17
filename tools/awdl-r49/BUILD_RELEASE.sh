#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
R49_BUILD_ROOT=${R49_BUILD_ROOT:-/private/tmp/R49-FINAL-STABLE-BOOT-BUILD}
xcodebuild -project itlwm.xcodeproj -target AirportItlwm-Tahoe \
 -configuration Release ARCHS=x86_64 -jobs 2 MODULE_VERSION=2.0.0 \
 SYMROOT="$R49_BUILD_ROOT/products" OBJROOT="$R49_BUILD_ROOT/intermediates" \
 SHARED_PRECOMPS_DIR="$R49_BUILD_ROOT/precompiled" MODULE_CACHE_DIR="$R49_BUILD_ROOT/module-cache" \
 CLANG_MODULE_CACHE_PATH="$R49_BUILD_ROOT/module-cache" \
 CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO clean build
