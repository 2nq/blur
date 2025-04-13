#!/bin/bash
set -e

out_dir=out

echo "Building dependencies for Linux"

# clean outputs every run
rm -rf $out_dir
mkdir -p $out_dir

download_archive() {
  local url="$1"
  local dir_name="$2"
  local out_path="$3"
  local subfolder="$4"

  original_dir=$(pwd)

  mkdir -p download
  cd download

  if [ -d "$dir_name" ]; then
    echo "$dir_name already exists. Skipping download."
    cd "$dir_name"
  else
    mkdir -p "$dir_name" && cd "$dir_name"

    echo "Downloading $dir_name"

    if [[ "$url" == *.zip ]]; then
      wget -q "$url" -O "$dir_name.zip"
      unzip "$dir_name.zip"
      rm "$dir_name.zip"
    elif [[ "$url" == *.tar.xz ]]; then
      wget -q "$url" -O "$dir_name.tar.xz"
      tar -xf "$dir_name.tar.xz"
      rm "$dir_name.tar.xz"
    else
      echo "Unsupported archive format: $url"
      cd "$original_dir"
      return 1
    fi
  fi

  dest_path="$original_dir/$out_dir/$out_path"

  echo "Copying files from $subfolder to $dest_path"

  cp -r "${subfolder:=.}" "$dest_path"

  cd "$original_dir"
}

build() {
  local repo="$1"
  local pull_args="$2"
  local name="$3"
  local build_cmd="$4"
  local lib_path="$5"
  local out_path="$6"

  echo "--- Building $name ---"

  mkdir -p build
  cd build

  if [ ! -d "$name" ]; then
    echo "Cloning $name..."
    # shellcheck disable=SC2086
    git clone $pull_args "$repo" "$name"
    cd "$name"
  else
    echo "Updating $name..."
    cd "$name"
    git pull
  fi

  eval "$build_cmd"

  # copy built stuff
  dest_path="../../$out_dir/$out_path"
  mkdir -p "$dest_path"

  if [[ -n "$lib_path" ]]; then
    echo "Copying $name libraries to $dest_path"
    find "$lib_path" -name "*.so" -exec cp {} "$dest_path" \;
  else
    echo "Skipping copy: lib_path is empty"
  fi

  cd ../..
}

# downloads
## ffmpeg (shared) (for building bestsource) (libavutil in apt is outdated)
download_archive \
  "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-linux64-gpl-shared.tar.xz" \
  "ffmpeg-shared" \
  "ffmpeg-shared" \
  "ffmpeg-master-latest-linux64-gpl-shared"

sudo cp -r "$out_dir/ffmpeg-shared/bin/"* /usr/local/bin/
sudo cp -r "$out_dir/ffmpeg-shared/lib/"* /usr/local/lib/
sudo cp -r "$out_dir/ffmpeg-shared/include/"* /usr/local/include/

mkdir -p $out_dir/ffmpeg
cp $out_dir/ffmpeg-shared/bin/ffmpeg $out_dir/ffmpeg

## svpflow
download_archive \
  "https://web.archive.org/web/20190322064557/http://www.svp-team.com/files/gpl/svpflow-4.2.0.142.zip" \
  "svpflow" \
  "vapoursynth-plugins" \
  "svpflow-4.2.0.142/lib-linux"

## python for vapoursynth
mkdir -p download/python
cd download/python

if [ ! -d "python" ]; then
  wget -q https://github.com/astral-sh/python-build-standalone/releases/download/20250317/cpython-3.12.9+20250317-x86_64-unknown-linux-gnu-install_only.tar.gz -O python.tar.gz
  mkdir -p python
  tar -xzf python.tar.gz -C python --strip-components 1
  rm python.tar.gz
fi

# copy python to output directory
python_dest_path="../../$out_dir/python"
mkdir -p "$python_dest_path"
cp -R python/* "$python_dest_path"

cd ../..

$out_dir/python/bin/pip install --upgrade pip
$out_dir/python/bin/pip install cython

# builds
## vapoursynth

PATH="$PWD/$out_dir/python/bin:$PATH"
PYTHON_PREFIX="$PWD/$out_dir/python"

build "https://github.com/vapoursynth/vapoursynth.git" "" "vapoursynth" "
./autogen.sh
PYTHON3_LIBS=\"-L$PYTHON_PREFIX/lib/python3.12 -L$PYTHON_PREFIX/lib -lpython3.12\" \
  PYTHON3_CFLAGS=\"-I$PYTHON_PREFIX/include/python3.12\" \
  ./configure --with-python_prefix=\"$PYTHON_PREFIX\" --with-cython=\"$PYTHON_PREFIX/bin/cython\"
make
sudo make install
" "" "vapoursynth"

### copy vspipe
cp build/vapoursynth/.libs/vspipe $out_dir/vapoursynth

## bestsource
build "https://github.com/vapoursynth/bestsource.git" "--depth 1 --recurse-submodules --shallow-submodules --remote-submodules" "bestsource" "
meson setup build
ninja -C build
" "build" "vapoursynth-plugins"

## mvtools
build "https://github.com/dubhater/vapoursynth-mvtools.git" "" "mvtools" "
meson setup build
ninja -C build
" "build" "vapoursynth-plugins"

## akarin
rm -rf build/akarin
build "https://github.com/Jaded-Encoding-Thaumaturgy/akarin-vapoursynth-plugin.git" "" "akarin" "
git checkout 689cba74e7c71caf808b6feaaba0a32981c1956f
meson build
ninja -C build
" "build" "vapoursynth-plugins"

echo "done"
