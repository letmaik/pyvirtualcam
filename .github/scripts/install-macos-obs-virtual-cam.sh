#!/bin/bash
set -e -x

VERSION="28.0.2"

if [[ "$PYTHON_ARCH" == "arm64" ]]; then
    ARCH_MOUNT="Apple"
else
    ARCH_MOUNT="Intel"
fi

DMG_FILENAME="obs-studio-${VERSION}-macos-${PYTHON_ARCH}.dmg"
DMG_URL="https://cdn-fastly.obsproject.com/downloads/${DMG_FILENAME}"
MOUNT_PATH="/Volumes/OBS-${VERSION}-macOS-${ARCH_MOUNT}"
INSTALL_PATH="/Library/CoreMediaIO/Plug-Ins/DAL"

sudo mkdir -p "${INSTALL_PATH}"

curl -L --retry 3 "${DMG_URL}" -o "${DMG_FILENAME}"

sudo hdiutil attach "${DMG_FILENAME}"
sudo cp -r "${MOUNT_PATH}/OBS.app/Contents/resources/obs-mac-virtualcam.plugin" "${INSTALL_PATH}"
sudo hdiutil unmount "${MOUNT_PATH}" -force
