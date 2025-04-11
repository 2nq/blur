#!/bin/bash
set -e

rm -rf appimage/

# sudo cp -r out/python/bin/* /usr/local/bin
# sudo cp -r out/python/include/* /usr/local/include
# sudo cp -r out/python/lib/* /usr/local/lib

# create appdir structure
mkdir -p appimage/usr/bin
mkdir -p appimage/usr/bin/ffmpeg
mkdir -p appimage/usr/bin/vapoursynth
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

# copy python
mkdir -p appimage/usr/bin/python
cp -r out/python/* appimage/usr/bin/python

# copy shared libraries
cp /usr/local/lib/libvapoursynth* appimage/usr/lib
cp -r out/ffmpeg-shared/lib/* appimage/usr/lib

# this is also required for a vapoursynth plugin that i forget
cp /usr/lib64/libfftw3* appimage/usr/lib

# copy vapoursynth plugins
mkdir appimage/usr/bin/vapoursynth-plugins
cp out/vapoursynth-plugins/*.so appimage/usr/bin/vapoursynth-plugins

# copy additional binaries
cp -r out/ffmpeg/* appimage/usr/bin/ffmpeg
cp -r out/vapoursynth/* appimage/usr/bin/vapoursynth

# copy vapoursynth scripts
mkdir -p appimage/usr/bin/lib
old_dir=$PWD
cd ../src/vapoursynth
find -name "*.py" -exec cp --parents {} ../../ci/appimage/usr/bin/lib/ \;
cd $old_dir

# set executable permissions for all binaries
chmod +x appimage/usr/bin/*
chmod +x appimage/usr/bin/ffmpeg*
chmod +x appimage/usr/bin/vapoursynth*

# build the appimage
# export NO_STRIP=true (fedora)

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./linuxdeploy-x86_64.AppImage --appdir=appimage --output=appimage
