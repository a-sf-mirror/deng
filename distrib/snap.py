# Doomsday Project Snapshot Builder
# $Id$
#
# Command line options specify which files to include.
# Files will be copied to a temporary directory, from which
# a RAR archive is created.
#
# Options:
# -nogame		Exclude game DLLs
# -nodll		Exclude non-game DLLs
# -norend		Exclude rendering DLLs
# -nosnd		Exclude sound DLLs
# -ogl			Exclude Direct3D
# -def			Include definition files
# -all			Include ALL definitions files
# -test			Don't upload

import os, tempfile, sys, shutil, re

baseDir = 'C:/Projects/de1.7/doomsday'
outDir  = 'C:/Projects/de1.7/distrib/out'

print "Source directory: " + baseDir

# Create the temporary directory.
archDir = tempfile.mktemp()
print "Using temporary directory: " + archDir
os.mkdir( archDir )
neededDirs = ['Bin']
if '-def' in sys.argv:
	neededDirs += ['Defs', 'Defs\jDoom', 'Defs\jHeretic', 'Defs\jHexen']
for sub in neededDirs:
	os.mkdir( os.path.join( archDir, sub ) )

files = [ ('Bin/Release/Doomsday.exe', 'Bin/Doomsday.exe') ]

if not '-nogame' in sys.argv:
	files += [ ('Bin/Release/jDoom.dll', 'Bin/jDoom.dll'),
               ('Bin/Release/jHeretic.dll', 'Bin/jHeretic.dll'),
               ('Bin/Release/jHexen.dll', 'Bin/jHexen.dll') ]

includeRender = not '-norend' in sys.argv

# Select the appropriate files.
if not '-nodll' in sys.argv:
	if includeRender:
		files.append( ('Bin/Release/drOpenGL.dll', 'Bin/drOpenGL.dll') )
	files.append( ('Bin/Release/dpDehRead.dll', 'Bin/dpDehRead.dll') )
	if not '-nosnd' in sys.argv:
		files += [ ('Bin/Release/dsA3D.dll', 'Bin/dsA3D.dll'),
		           ('Bin/Release/dsCompat.dll', 'Bin/dsCompat.dll') ]
	if not '-ogl' in sys.argv and includeRender:
		files.append( ('Bin/Release/drD3D.dll', 'Bin/drD3D.dll') )

# These defs will be included, if present.
includeDefs = ['Anim.ded', 'Finales.ded', 'jDoom.ded', 'jHeretic.ded',
               'jHexen.ded', 'TNTFinales.ded', 'PlutFinales.ded', 'Flags.ded',
               'Maps.ded']

excludeDefs = ['Objects.ded', 'Players.ded', 'HUDWeapons.ded', 'Weapons.ded',
               'Items.ded', 'Monsters.ded', 'Technology.ded', 'Nature.ded',
               'Decorations.ded', 'FX.ded', 'Models.ded',
               'TextureParticles.ded', 'Details.ded', 'Doom1Lights.ded',
               'Doom2Lights.ded']

if '-all' in sys.argv:
	allDefs = True
else:
	allDefs = False

if '-def' in sys.argv:
	for game in ['.', 'jDoom', 'jHeretic', 'jHexen']:
		gameDir = os.path.join( 'Defs', game )
		defDir = os.path.join( baseDir, gameDir )
		for file in os.listdir( defDir ):
			# Should this be included?
			if file in includeDefs or (allDefs and not file in excludeDefs \
			and re.match( "(?i).*\.ded$", file )):
				loc = os.path.join( gameDir, file )
				files.append( (loc, loc) )

# Copy the files.
for src, dest in files:
	shutil.copyfile( os.path.join( baseDir, src ),
	                 os.path.join( archDir, dest ) )

# Make the archive.
outFile = os.path.join( outDir, 'ddsnapshot.rar' )
try:
	os.remove( outFile )
except:
	# Never mind.
	pass
os.system( "rar -r -ep1 -m5 a %s %s\\*" % (outFile, archDir) )

print "Removing temporary directory: " + archDir
shutil.rmtree( archDir )

if not '-test' in sys.argv:
	# Upload.
	print "Uploading to the Mirror..."
	os.system( "ftpscrpt -f c:\projects\doomsday\scripts\mirror-snapshot.scp" )
	print "Uploading to Fourwinds..."
	os.system( "ftpscrpt -f c:\projects\doomsday\scripts\snapshot.scp" )
else:
	print "--- THIS WAS JUST A TEST ---"

print "Done!"
