#!/bin/bash
set -e -x

VERSION="28.0.2"

DMG_FILENAME="obs-studio-${VERSION}-macos-x86_64.dmg"
DMG_URL="https://cdn-fastly.obsproject.com/downloads/${DMG_FILENAME}"
MOUNT_PATH="/Volumes/OBS-${VERSION}-macOS-Intel"
INSTALL_PATH="/Library/CoreMediaIO/Plug-Ins/DAL"

sudo mkdir -p "${INSTALL_PATH}"

curl -L --retry 3 "${DMG_URL}" -o "${DMG_FILENAME}"

sudo hdiutil attach "${DMG_FILENAME}"
sudo cp -r "${MOUNT_PATH}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" "${INSTALL_PATH}"
sudo hdiutil unmount "${MOUNT_PATH}" -force
