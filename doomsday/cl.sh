#!/bin/sh
mkdir -p clientdir
DYLD_LIBRARY_PATH=`pwd` ./dengcl -iwad ~/IWADs/Doom.wad
