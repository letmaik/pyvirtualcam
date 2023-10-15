#!/bin/bash
set -e -x

# VERSION="28.0.2"
# ARCH="x86_64"
# ARCH_MOUNT="Intel"

VERSION="30.0.0-rc1"
ARCH="intel" # "apple"
ARCH_MOUNT="Intel" 

DMG_FILENAME="obs-studio-${VERSION}-macos-${ARCH}.dmg"
DMG_URL="https://cdn-fastly.obsproject.com/downloads/${DMG_FILENAME}"
MOUNT_PATH="/Volumes/OBS-${VERSION}-macOS-${ARCH_MOUNT}"
MOUNT_PATH2="/Volumes/OBS Studio ${VERSION} (${ARCH_MOUNT})"
INSTALL_PATH="/Library/CoreMediaIO/Plug-Ins/DAL"

sudo mkdir -p "${INSTALL_PATH}"

curl -L --retry 3 "${DMG_URL}" -o "${DMG_FILENAME}"

brew install tree

sudo hdiutil attach "${DMG_FILENAME}"
ls -al $MOUNT_PATH || true
ls -al "${MOUNT_PATH}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" || true
tree "${MOUNT_PATH}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" || true
ls -al "$MOUNT_PATH2" || true
ls -al "${MOUNT_PATH2}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" || true
tree "${MOUNT_PATH2}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" || true
sudo cp -r "${MOUNT_PATH}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" "${INSTALL_PATH}"
ls -al "$INSTALL_PATH" || true
tree "$INSTALL_PATH" || true
sudo hdiutil unmount "${MOUNT_PATH}" -force
