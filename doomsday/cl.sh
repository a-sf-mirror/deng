#!/bin/sh
mkdir -p clientdir
mkdir -p serverdir
export LD_LIBRARY_PATH=`pwd`
./dengcl -iwad ~/IWADs/Doom.wad -wnd -nomusic
