#!/bin/bash
set -e

# Create AppDir structure
mkdir -p appimage/usr/bin
mkdir -p appimage/usr/lib
mkdir -p appimage/usr/share/applications
mkdir -p appimage/usr/share/icons/hicolor/256x256/apps

# Download linuxdeploy tool
wget -c https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# Copy main executables
cp ../bin/Release/blur appimage/usr/bin/
cp ../bin/Release/blur-cli appimage/usr/bin/

# Copy desktop file and icon
cp ../resources/blur.desktop appimage/usr/share/applications/blur.desktop
cp ../resources/blur.png appimage/usr/share/icons/hicolor/256x256/apps/blur.png

# Copy shared libraries
cp -r out/ffmpeg-shared/lib/* /appimage/usr/lib
cp out/vapoursynth-plugins/*.so appimage/usr/lib
cp -r out/python/lib/* /appimage/usr/lib

# Copy additional binaries
mkdir -p appimage/usr/bin/additional
cp -r out/ffmpeg-shared/bin/* /appimage/usr/bin/additional
cp -r out/python/bin/* /appimage/usr/bin/additional

# Set executable permissions for all binaries
chmod +x appimage/usr/bin/*
[ -d appimage/usr/bin/additional ] && chmod +x appimage/usr/bin/additional/*

# Build the AppImage
export VERSION=$(git describe --tags --always)
./linuxdeploy-x86_64.AppImage --appdir=appimage --output=appimage
