#!/usr/bin/python
# Doomsday Project Model Pack Builder
# $Id$
#
# This script will build both the individual object packs and
# the collection packs.
#
# -final: Create the final distribution ZIPs

import os, sys, re, string

# Remove the file if it exists.
def obsolete( fileName ):
	if os.path.exists( fileName ):
		os.remove( fileName )

# Build a list of package names.
def build_list( top, path = None ):
	files = []
	for f in os.listdir( top ):
		if not path:
			name = f
		else:
			name = os.path.join( path, f )
		# Is it a DED file?
		if re.match( "(?i).*\.ded$", f ):
			# Add to the list.
			files.append( name )
		else:
			# Try to recurse.
			try:
				sub = build_list( os.path.join( top, f ), name )
				if len( sub ) > 0: files += sub
			except:
				# Didn't succeed.
				pass
	return files

# Add a new member to a group.
def collection_add( collec, group, member ):
	for i in collec:
		if i[0] == group:
			i[1].append( member )
			return
	collec.append( (group, [member]) )

# Create a zip archive.
def run_zip( dest, files, options ):
	os.system( string.join( ['zip', options, dest, files] ) )

def zip_store( dest, files, options = '', exclude = '' ):
	exOpt = ''
	if exclude != '':
		exOpt = "-x '" + exclude + "'"
	run_zip( dest, string.join( [files, exOpt] ), '-0 ' + options )

def zip_max( dest, files, options = '' ):
	run_zip( dest, files, '-9 ' + options )


# Operating mode.
distroMode = '-final' in sys.argv

# Directory configuration:
baseDir  = os.path.abspath( '../doomsday' )
outDir   = os.path.abspath( 'Out/Models' )
finalDir = os.path.join( outDir, 'Final' )
origDir  = os.getcwd()

print "Source directory: " + baseDir

if len( sys.argv ) < 2:
	print "You must specify the name of the game."
	print "Example: modpak.py jHeretic"
	sys.exit()

# The first argument must be the name of the game.
game = sys.argv[1]
if game == 'jHeretic':
	output = 'jHRP'
	prefix = 'H-'
elif game == 'jHexen':
	output = 'jXRP'
	prefix = 'X-'
else:
	print "Unknown game."
	sys.exit()

# Model directory configuration:
defsDir = os.path.join( 'Defs', game )
dataDir = os.path.join( 'Data', game )
docDir  = os.path.join( 'Doc', game )
modelDefsDir = os.path.join( defsDir, 'Models' )
modelDataDir = os.path.join( dataDir, 'Models' )

os.chdir( baseDir )

if not distroMode:

	# First build the full pack by adding all the models.
	outFile = os.path.join( outDir, output + '.pk3' )
	obsolete( outFile )
	zip_store( outFile, modelDefsDir, '-r', '*/CVS/*' )
	zip_store( outFile, os.path.join( defsDir, 'Models.ded' ) )
	zip_store( outFile, modelDataDir, '-r', '*.bat' )
	zip_store( outFile, os.path.join( docDir, 'Models.txt' ) )

	# Build the Common pack.
	cmFile = os.path.join( outDir, prefix + 'Common.pk3' )
	obsolete( cmFile )
	zip_store( cmFile, os.path.join( modelDefsDir, 'Common' ), '-r', '*/CVS/*' )
	zip_store( cmFile, os.path.join( modelDataDir, 'Common' ), '-r' )
	zip_store( cmFile, os.path.join( defsDir, 'Models.ded' ) )

# We'll build a list of packages for each main class. One collection ZIP
# will be compiled for each class.
collections = []

# Build a separate pack for each object (i.e. for each DED file).
for ded in build_list( os.path.join( baseDir, modelDefsDir ) ):
	# Compose the pack name.
	parts = ded.split( '/' )
	# Start with the prefix.
	modName = prefix
	# The first component is the main object class.
	mClass = parts[0]
	if mClass[-1:] == 's': mClass = mClass[:-1] # Remove plural.
	modName += mClass + '-'
	# The actual name of the object.
	actual = ''
	for i in range( len( parts ) - 1 ):
		p = parts[i + 1].split( '.' )[0]
		actual = p + actual
	modName += actual

	modPath = os.path.join( game, 'Models' )
	for i in range( len( parts ) ):
		modPath = os.path.join( modPath, parts[i].split( '.' )[0] )
	print "modPath: " + modPath

	# Create the PK3.
	mFile = os.path.join( outDir, modName + '.pk3' )
	print "--> " + mFile

	if not distroMode:
		obsolete( mFile )
		zip_store( mFile, os.path.join( 'Defs', modPath + '.ded' ) )
		zip_store( mFile, os.path.join( 'Data', modPath ), '-r' )
		zip_store( mFile, os.path.join( defsDir, 'Models.ded' ) )
		zip_store( mFile, os.path.join( docDir, 'Models.txt' ) )

	# Add to the class collection.
	collection_add( collections, prefix + parts[0], mFile )

# Include the Common package in all collections.
for collec in collections:
	collec[1].append( os.path.join( outDir, prefix + 'Common.pk3' ) )

os.chdir( origDir )
print "Returning to: " + origDir

if distroMode:
	# Create a final compressed ZIP file out of each PK3.
	for file in os.listdir( outDir ):

		# Is it a PK3?
		found = re.match( "(?i)(.*)\.pk3$", file )
		if not found: continue

		# Which game is it?
		fileGame = "jHeretic"
		if file[0] == 'X' or file[:2] == 'jX': fileGame = "jHexen"
		if file[0] == 'j':
			destDir = finalDir
		else:
			destDir = os.path.join( finalDir, 'Single' )
		if fileGame != game: continue

		# Pack into a compressed ZIP.
		packFile = os.path.join( destDir, found.group(1) + '.zip' )
		print "Packing full pack to: " + packFile
		obsolete( packFile )
		zip_max( packFile, string.join( [\
			os.path.join( outDir, file ), \
			os.path.join( 'Include', fileGame, 'Readme.txt' ), \
			os.path.join( baseDir, docDir, 'Models.txt' ) ] ), '-j' )

	# Build collections.
	for collec in collections:

		# Which game is it?
		fileGame = "jHeretic"
		if collec[0][0] == 'X': fileGame = "jHexen"
		if fileGame != game: continue

		# Create a compressed ZIP with multiple PK3s.
		packFile = os.path.join( finalDir, 'Collections', collec[0] + '.zip' )
		print "Packing collection: " + packFile
		obsolete( packFile )
		zip_max( packFile, string.join( collec[1] ), '-j' )
		zip_max( packFile, string.join( [
			os.path.join( 'Include', fileGame, 'Readme.txt' ), \
			os.path.join( baseDir, docDir, 'Models.txt' ) ] ), '-j' )
