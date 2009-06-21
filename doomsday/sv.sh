#!/bin/sh
mkdir -p serverdir
DYLD_LIBRARY_PATH=`pwd` ./dengsv -iwad ~/IWADs/Doom.wad
