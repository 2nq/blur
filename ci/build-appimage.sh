#!/bin/bash
set -e

rm -rf appimage/

# create appdir structure
mkdir -p appimage/usr/bin
mkdir -p appimage/usr/bin/additional
mkdir -p appimage/usr/lib
mkdir -p appimage/usr/share/applications
mkdir -p appimage/usr/share/icons/hicolor/256x256/apps

# download linuxdeploy tool
rm -f linuxdeploy-x86_64.AppImage
wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# copy binaries from arguments
for arg in "$@"; do
  cp "$arg" appimage/usr/bin/
done

# copy desktop file and icon
cp ../resources/blur.desktop appimage/usr/share/applications/blur.desktop
cp ../resources/blur.png appimage/usr/share/icons/hicolor/256x256/apps/blur.png

# copy shared libraries
cp -r out/python/lib/* appimage/usr/lib

# copy vapoursynth plugins
mkdir appimage/usr/bin/vapoursynth-plugins
cp out/vapoursynth-plugins/*.so appimage/usr/bin/vapoursynth-plugins

# copy additional binaries
cp -r out/ffmpeg/* appimage/usr/bin/additional
cp -r out/python/bin/* appimage/usr/bin/additional

# copy vapoursynth scripts
mkdir -p appimage/usr/bin/lib
old_dir=$PWD
cd ../src/vapoursynth
find -name "*.py" -exec cp --parents {} ../../ci/appimage/usr/bin/lib/ \;
cd $old_dir

# set executable permissions for all binaries
chmod +x appimage/usr/bin/*
[ -d appimage/usr/bin/additional ] && chmod +x appimage/usr/bin/additional/*

# build the appimage
export VERSION=$(git describe --tags --always)
./linuxdeploy-x86_64.AppImage --appdir=appimage --output=appimage
