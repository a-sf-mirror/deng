#!/bin/sh
mkdir -p clientdir
export LD_LIBRARY_PATH=`pwd`
./dengcl -iwad ~/IWADs/Doom.wad -wnd -nomusic
