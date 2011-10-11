"""
   Win32 specific build script for generating a Windows Installer for the
   Doomsday Engine, it's plugins and related deng Team products by leveraging
   the awesomely powerful Windows Installer XML (WiX) toolset.
"""

#!/usr/bin/python
import sys
import os
import platform
import shutil
import time
import build_version
import build_number

LAUNCH_DIR = os.path.abspath(os.getcwd())
SOURCE_DIR = os.path.join(LAUNCH_DIR, 'win32\\')
WORK_DIR = os.path.join(LAUNCH_DIR, 'work\\')
PRODUCTS_DIR = os.path.join(LAUNCH_DIR, 'products\\')
OUTPUT_DIR = os.path.join(LAUNCH_DIR, 'releases\\')

DOOMSDAY_VERSION_FULL = "0.0.0-Name"
DOOMSDAY_VERSION_FULL_PLAIN = "0.0.0"
DOOMSDAY_VERSION_MAJOR = 0
DOOMSDAY_VERSION_MINOR = 0
DOOMSDAY_VERSION_REVISION = 0
DOOMSDAY_RELEASE_TYPE = "Unstable"
now = time.localtime()
DOOMSDAY_BUILD_NUMBER = build_number.todays_build()
DOOMSDAY_BUILD = 'build' + DOOMSDAY_BUILD_NUMBER
TIMESTAMP = time.strftime('%y-%m-%d')


def exit_with_error():
    os.chdir(LAUNCH_DIR)
    sys.exit(1)

def mkdir(n):
    try:
        os.mkdir(n)
    except OSError:
        print 'Directory', n, 'already exists.'


def remkdir(n):
    if os.path.exists(n):
        print n, 'exists, clearing it...'
        shutil.rmtree(n, True)
    os.mkdir(n)


def find_version():
    build_version.find_version()

    global DOOMSDAY_RELEASE_TYPE
    global DOOMSDAY_VERSION_FULL
    global DOOMSDAY_VERSION_FULL_PLAIN
    global DOOMSDAY_VERSION_MAJOR
    global DOOMSDAY_VERSION_MINOR
    global DOOMSDAY_VERSION_REVISION

    DOOMSDAY_RELEASE_TYPE = build_version.DOOMSDAY_RELEASE_TYPE
    DOOMSDAY_VERSION_FULL_PLAIN = build_version.DOOMSDAY_VERSION_FULL_PLAIN
    DOOMSDAY_VERSION_FULL = build_version.DOOMSDAY_VERSION_FULL
    DOOMSDAY_VERSION_MAJOR = build_version.DOOMSDAY_VERSION_MAJOR
    DOOMSDAY_VERSION_MINOR = build_version.DOOMSDAY_VERSION_MINOR
    DOOMSDAY_VERSION_REVISION = build_version.DOOMSDAY_VERSION_REVISION


def prepare_work_dir():
    remkdir(WORK_DIR)
    print "Work directory prepared."


def clean_work_dir():
    os.system('rd/s/q ' + WORK_DIR)


""" Compilation of WiX source files """
def compile_wix_sources(files, locale=None):
    global DOOMSDAY_VERSION_MAJOR
    global DOOMSDAY_VERSION_MINOR
    global DOOMSDAY_VERSION_REVISION
    global DOOMSDAY_BUILD_NUMBER

    if not (files and isinstance(files, list)):
       raise Exception("compile_wix_sources: Expected a list for Arg0 but received something else.")

    print 'Compiling WiX source files:'
    sys.stdout.flush()

    CDEFS = '-dProductVersionMajor="' + str(DOOMSDAY_VERSION_MAJOR) + '"' \
         + ' -dProductVersionMinor="' + str(DOOMSDAY_VERSION_MINOR) + '"' \
         + ' -dProductVersionRevision="' + str(DOOMSDAY_VERSION_REVISION) + '"' \
         + ' -dProductBuildNumber="' + str(DOOMSDAY_BUILD_NUMBER) + '"'
    if locale and isinstance(locale, dict):
        CDEFS += " -dLanguage=%i" % locale['language']
        CDEFS += " -dCodepage=%i" % locale['codepage']
    else:
        # Assume en-us
        CDEFS += " -dLanguage=1033"
        CDEFS += " -dCodepage=1252"

    # Compile sources.
    for file in files:
        # Compose source file name.
        print "  %s..." % os.path.normpath(file['source'] + '.wxs')
        srcFile = os.path.join(SOURCE_DIR, file['source'] + '.wxs')

        # Compose out file name
        outFile = os.path.join(WORK_DIR, file['source'])
        if locale and isinstance(locale, dict):
            outFile += '_' + locale['culture']
        outFile += '.wixobj'

        # Compile.
        if os.system('candle -nologo ' + CDEFS + ' -out ' + outFile + ' -wx0099 ' + srcFile):
            raise Exception("Failed compiling %s" % srcFile)


""" Linking of WiX source files and installable binding """
def link_wix_objfiles(outFileName, files, locale=None):
    if not (files and isinstance(files, list)):
       raise Exception("link_wix_objfiles: Expected a list for Arg1 but received something else.")

    # Compose the full path to the target file.
    target = os.path.join(OUTPUT_DIR, outFileName + '.msi')
    try:
        os.remove(target)
        print 'Removed existing target file', target
    except:
        print 'Target:', target

    print 'Linking WiX object files:'

    # Compose the link object file list.
    objFileList = ' ';
    for file in files:
        # Compose link object file name.
        fileWithExt = file['source']
        if locale and isinstance(locale, dict):
            fileWithExt += '_' + locale['culture']
        fileWithExt += '.wixobj'

        print "  %s..." % os.path.normpath(fileWithExt)
        objFileList += os.path.join(WORK_DIR, fileWithExt) + ' '

        # Is there a localization file to go with this?
        if 'locale' in file:
            objFileList += '-loc ' + file['locale'] + ' '

    # Compose link arguments.
    LARGS = '-nologo -b ' + PRODUCTS_DIR \
         + ' -out ' + target \
         + ' -pdbout ' + os.path.join(WORK_DIR, outFileName + '.wixpdb') \
         + ' -ext WixUIExtension -ext WixUtilExtension'

    if locale and isinstance(locale, dict):
        LARGS += ' -cultures:' + locale['culture']
    else:
        LARGS += ' -cultures:' + 'en-us'

    # Link all objects and bind our installables into cabinents.
    sys.stdout.flush()
    if os.system('light ' + LARGS + objFileList):
        raise Exception("Failed linking WiX object files.")


def win_installer():
    # Compose outfile name
    global DOOMSDAY_VERSION_MAJOR
    global DOOMSDAY_VERSION_MINOR
    global DOOMSDAY_VERSION_REVISION
    global DOOMSDAY_BUILD

    OUTFILE = 'doomsday_' + DOOMSDAY_VERSION_MAJOR + '.' + DOOMSDAY_VERSION_MINOR + '.' + DOOMSDAY_VERSION_REVISION
    if DOOMSDAY_BUILD != '':
        OUTFILE += '_' + DOOMSDAY_BUILD

    # Compose the source file list.
    files = ['engine',
             'installer',
             'dehread',
             'directsound',
             'doom',
             'heretic',
             'hexen',
             'openal',
             'wadmapconverter',
             'winmm',
             'snowberry']

    prepare_work_dir()

    # Compile all locale versions.
    locales = [dict(culture = 'en-us', codepage = 1252, language = 1033, base = True)]

    for l in locales:
        fl = []
        # Compose the file list for this locale.
        for f in files:
            locd = dict(source = f)

            # Is there a localization for this file?
            locf = os.path.join(SOURCE_DIR, l['culture'], f + '.wxl')
            if os.path.exists(locf):
                locd['locale'] = locf

            # Add this to the list of files for this locale
            fl.append(locd)

        if 'base' in l and l['base'] == True:
            print 'Compiling default-/base- language installer...'
            outFile = OUTFILE
        else:
            print 'Compiling %s localized installer...' % l['culture']
            outFile = OUTFILE + '_' + l['culture']

        compile_wix_sources(fl, l)
        link_wix_objfiles(outFile, fl, l)

    # Cleanup
    clean_work_dir()


def main():
    find_version()

    print "Checking OS...",

    try:
        if sys.platform != "win32":
            print "Unknown!"
            print "I don't know how to make an installer on this platform."
            exit_with_error()

        print "Windows"
        win_installer()
    except Exception, x:
        print "Errors encountered building Doomsday Engine Installer."
        print x
        exit_with_error()

    os.chdir(LAUNCH_DIR)

if __name__ == '__main__':
    main()
