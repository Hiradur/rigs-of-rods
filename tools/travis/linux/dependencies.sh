#!/bin/sh

#Initialization
. "$TRAVIS_BUILD_DIR"/tools/travis/linux/config
mkdir -p "$ROR_SOURCE_DIR"
mkdir -p "$ROR_INSTALL_DIR"

# OGRE
cd "$ROR_SOURCE_DIR"
hg clone https://bitbucket.org/sinbad/ogre -b v2-0
cd ogre
cmake \
-DCMAKE_INSTALL_PREFIX="$ROR_INSTALL_DIR" \
-DFREETYPE_INCLUDE_DIR=/usr/include/freetype2/ \
-DCMAKE_BUILD_TYPE:STRING=DEBUG \
-DOGRE_BUILD_SAMPLES:BOOL=OFF .
make $ROR_MAKEOPTS
make install

# MyGUI
cd "$ROR_SOURCE_DIR"
git clone https://github.com/MyGUI/mygui.git
cd mygui
git checkout ogre2
cmake \
-DCMAKE_INSTALL_PREFIX="$ROR_INSTALL_DIR" \
-DFREETYPE_INCLUDE_DIR=/usr/include/freetype2/ \
-DCMAKE_BUILD_TYPE:STRING=RELEASE \
-DMYGUI_BUILD_DEMOS:BOOL=OFF \
-DMYGUI_BUILD_DOCS:BOOL=OFF \
-DMYGUI_BUILD_TEST_APP:BOOL=OFF \
-DMYGUI_BUILD_TOOLS:BOOL=OFF \
-DMYGUI_BUILD_PLUGINS:BOOL=OFF .
make $ROR_MAKEOPTS
make install

# MySocketW
cd "$ROR_SOURCE_DIR"
git clone --depth=1 https://github.com/Hiradur/mysocketw.git
cd mysocketw
git pull
sed -i '/^PREFIX *=/d' Makefile.conf
make $ROR_MAKEOPTS shared
PREFIX="$ROR_INSTALL_DIR" make install
