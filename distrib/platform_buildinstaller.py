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
DOOMSDAY_BUILD_NUMBER = str((now.tm_year - 2011)*365 + now.tm_yday)
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
def compile_wix_sources(files):
    global DOOMSDAY_VERSION_MAJOR
    global DOOMSDAY_VERSION_MINOR
    global DOOMSDAY_VERSION_REVISION
    global DOOMSDAY_BUILD_NUMBER

    print 'Compiling WiX source files:'
    sys.stdout.flush()

    CDEFS = '-dProductMajorVersion="' + str(DOOMSDAY_VERSION_MAJOR) + '"' \
         + ' -dProductMinorVersion="' + str(DOOMSDAY_VERSION_MINOR) + '"' \
         + ' -dProductBuildVersion="' + str(DOOMSDAY_VERSION_REVISION) + '"' \
         + ' -dProductRevisionVersion="' + str(DOOMSDAY_BUILD_NUMBER) + '"'

    # Compile sources.
    for file in files:
        fileWithExt = file + '.wxs'
        print "  %s..." % os.path.normpath(fileWithExt)
        srcFile = os.path.normpath(os.path.join(SOURCE_DIR, fileWithExt))
        if os.system('candle -nologo ' + CDEFS + ' -out ' + WORK_DIR + ' -wx0099 ' + srcFile):
            raise Exception("Failed compiling %s" % srcFile)


""" Linking of WiX source files and installable binding """
def link_wix_objfiles(files, outFile):
    print 'Linking WiX object files:'

    # Compose the link object file list.
    objFileList = '';
    for file in files:
        fileWithExt = file + '.wixobj'
        print "  %s..." % os.path.normpath(fileWithExt)
        objFileList += os.path.normpath(os.path.join(WORK_DIR, fileWithExt)) + ' '

    sys.stdout.flush()

    # Link all objects and bind our installables into cabinents.
    if os.system('light -nologo -b ' + PRODUCTS_DIR
               + ' -out ' + os.path.join(OUTPUT_DIR, outFile + '.msi')
               + ' -pdbout ' + os.path.join(WORK_DIR, outFile + '.wixpdb')
               + ' -ext WixUIExtension -ext WixUtilExtension ' + objFileList):
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

    # Compose the file list.
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

    compile_wix_sources(files)
    link_wix_objfiles(files, OUTFILE)

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
