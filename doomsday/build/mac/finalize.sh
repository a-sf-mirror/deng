#!/bin/sh

#TARG=build/final 
#chmod -R u+w $TARG/Doomsday.app
#rm -rf $TARG/Doomsday.app
#xcodebuild -target Everything -configuration Development \
#  clean install \
#  SYMROOT=Build/Release \
#  DSTROOT=$TARG \
#  SEPARATE_STRIP=YES \
#  STRIP="/usr/bin/strip -x -u -A"

DEPLOY=.
TARG=$DEPLOY/Doomsday.app/Contents
RES=$TARG/Resources

# Copy frameworks.
rm -rf $TARG/Frameworks
mkdir -p $TARG/Frameworks
cp -R $HOME/Library/Frameworks/{SDL,SDL_mixer,SDL_net}.framework \
  $TARG/Frameworks

# Copy resources.
cp $DEPLOY/doomsday.pk3 $DEPLOY/Doomsday.app/Contents/Resources/
cp $DEPLOY/jdoom.pk3 $DEPLOY/jDoom.bundle/Contents/Resources/
cp $DEPLOY/jheretic.pk3 $DEPLOY/jHeretic.bundle/Contents/Resources/
cp $DEPLOY/jhexen.pk3 $DEPLOY/jHexen.bundle/Contents/Resources/
