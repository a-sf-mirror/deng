# Doomsday Project Model Pack Builder
# $Id$
#
# This script will build both the individual object packs and
# the collection packs.
#
# -nofinal: Don't create distribution ZIPs

import os, sys, re, string

def obsolete( fileName ):
	if os.path.exists( fileName ):
		os.remove( fileName )

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

baseDir  = 'C:/Projects/de1.7/doomsday'
outDir   = 'C:/Projects/de1.7/distrib/Out/Models'
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

# First build the full pack by adding all the models.
os.chdir( baseDir )
outFile = os.path.join( outDir, output + '.pk3' )
obsolete( outFile )
os.system( 'wzzip ' + outFile + \
	' -e0 -r -P -x*\\CVS\\*.* Defs\\' + game + '\\Models\\*.* ' + \
	' -x*.exe -x*.bat ' + \
	'Data\\' + game + '\\Models\\*.* ' + \
	'Defs\\' + game + '\\Models.ded' )

# Build the Common pack.
cmFile = os.path.join( outDir, prefix + 'Common.pk3' )
obsolete( cmFile )
os.system( 'wzzip ' + cmFile + \
	' -e0 -r -P -x*\\CVS\\*.* Defs\\' + game + '\\Models\\Common\\*.* ' + \
	' -x*.exe -x*.bat ' + \
	'Data\\' + game + '\\Models\\Common\\*.* ' + \
	'Defs\\' + game + '\\Models.ded' )

# Build a separate pack for each object (i.e. for each DED file).
for ded in build_list( os.path.join( baseDir, 'Defs\\' + game + '\\Models' ) ):
	# Compose the pack name.
	parts = ded.split( '\\' )
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
	obsolete( mFile )
	os.system( 'wzzip ' + mFile +
		' -e0 -r -P -x*\\CVS\\*.* Defs\\' + modPath + '.ded ' + \
		'Data\\' + modPath + '\\*.* ' + \
		'Defs\\' + game + '\\Models.ded' )


os.chdir( origDir )
print "Returning to: " + origDir

if not '-nofinal' in sys.argv:
	# Create a final compressed ZIP file out of each PK3.
	for file in os.listdir( outDir ):

		# Is it a PK3?
		found = re.match( "(?i)(.*)\.pk3$", file )
		if not found: continue

		# Pack into a compressed ZIP.
		packFile = os.path.join( finalDir, found.group(1) + '.zip' )
		print "Packing full pack to: " + packFile
		obsolete( packFile )
		os.system( 'wzzip ' + packFile + ' -ex ' + \
			os.path.join( outDir, file ) + ' Include\\' + game + '\\Readme.txt' )
