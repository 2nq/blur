
mkdir -p appimage/lib

wget -c https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

mkdir -p appimage/usr/bin
mkdir -p appimage/usr/share/applications
mkdir -p appimage/usr/share/icons/hicolor/256x256/apps

cp ../bin/Release/blur appimage/usr/bin/
cp ../bin/Release/blur-cli appimage/usr/bin/

cp ../resources/blur.desktop appimage/usr/share/applications/blur.desktop

# Copy your app icon
cp ../resources/blur.png appimage/usr/share/icons/hicolor/256x256/apps/blur.png

export VERSION=$(git describe --tags --always)
./linuxdeploy-x86_64.AppImage --appdir=appimage --output=appimage