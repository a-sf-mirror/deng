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
import subprocess
import time
import build_version
import build_number

WIX_DIR = os.getenv('WIX_DIR')
WINDOWS_SDK_DIR = os.getenv('WINDOWS_SDK_DIR')

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

verbose = False
quiet = False


def exit_with_error():
    os.chdir(LAUNCH_DIR)
    sys.exit(1)


def mkdir(n):
    try:
        os.mkdir(n)
    except OSError:
        if not quiet:
            print 'Directory', n, 'already exists.'


def remkdir(n):
    if os.path.exists(n):
        if not quiet:
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
    if not quiet:
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

    CDEFS = ['-dProductVersionMajor=' + str(DOOMSDAY_VERSION_MAJOR),
             '-dProductVersionMinor=' + str(DOOMSDAY_VERSION_MINOR),
             '-dProductVersionRevision=' + str(DOOMSDAY_VERSION_REVISION),
             '-dProductBuildNumber=' + str(DOOMSDAY_BUILD_NUMBER)]

    if locale and isinstance(locale, dict):
        CDEFS += ['-dLanguage=' + "%i" % locale['language'],
                  '-dCodepage=' + "%i" % locale['codepage']]
    else:
        # Assume en-us
        CDEFS += ['-dLanguage=1033', '-dCodepage=1252']

    if not quiet:
        print 'Compiling WiX source files' + ('...', ':')[verbose != 0]
    sys.stdout.flush()

    # Compile sources.
    for file in files:
        # Compose source file name.
        if not quiet and verbose:
            print "  %s..." % os.path.normpath(file['source'] + '.wxs')
        srcFile = os.path.join(SOURCE_DIR, file['source'] + '.wxs')

        # Compose out file name
        outFile = os.path.join(WORK_DIR, file['source'])
        if locale and isinstance(locale, dict):
            outFile += '_' + locale['culture']
        outFile += '.wixobj'

        # Compile.
        cmd = ['candle', '-nologo', '-out', "%s"%outFile, '-wx0099', "%s"%srcFile]
        if subprocess.call(cmd + CDEFS, cwd=WIX_DIR):
            raise Exception("Failed compiling %s" % srcFile)


""" Linking of WiX source files and installable binding """
def link_wix_objfiles(outDir, outFileName, files, locale=None):
    if not (files and isinstance(files, list)):
       raise Exception("link_wix_objfiles: Expected a list for Arg2 but received something else.")

    # Compose the full path to the target file.
    target = os.path.join(outDir, outFileName + '.msi')
    try:
        os.remove(target)
        if not quiet:
            print 'Removed existing target file', target
    except:
        if not quiet:
            print 'Target:', target

    if not quiet:
        print 'Linking WiX object files' + ('...', ':')[verbose != 0]

    # Compose the link file command list.
    LCMDS = []
    for file in files:
        # Compose link object file name.
        fileName = file['source']
        if locale and isinstance(locale, dict):
            fileName += '_' + locale['culture']

        LCMDS += [os.path.join(WORK_DIR, fileName + '.wixobj')]

        if verbose:
            sys.stdout.write("  " + os.path.normpath(fileName + '.wixobj'))

        # Is there a base/default localization file to go with this?
        if 'defLocale' in file:
            LCMDS += ['-loc', os.path.join(SOURCE_DIR, file['defLocale'])]
            if verbose:
                sys.stdout.write(' loc:' + os.path.normpath(file['defLocale']))

        # Is there a localization file to go with this?
        if 'locale' in file:
            LCMDS += ['-loc', os.path.join(SOURCE_DIR, file['locale'])]
            if verbose:
                sys.stdout.write(' loc:' + os.path.normpath(file['locale']))

        if verbose:
            print ""

    # Compose link arguments.
    cmd = ['light', '-nologo', '-b', PRODUCTS_DIR,
           '-out', target,
           '-pdbout', os.path.join(WORK_DIR, outFileName + '.wixpdb'),
           '-ext', 'WixUIExtension', '-ext', 'WixUtilExtension']

    if locale and isinstance(locale, dict):
        cmd += ['-cultures:%s;en-us' % locale['culture']]
    else:
        cmd += ['-cultures:en-us']

    # Link all objects and bind our installables into cabinents.
    sys.stdout.flush()
    if subprocess.call(cmd + LCMDS, cwd=WIX_DIR):
        raise Exception("Failed linking WiX object files.")


def build_file_list(files, defCulture, localeCulture=None):
    if not (files and isinstance(files, list)):
       raise Exception("build_file_list: Expected a list for Arg0 but received something else.")

    fl = []
    # Compose the file list for this locale.
    for f in files:
        locd = dict(source = f)

        # Is there a base/default localization for this file?
        locf = os.path.join(defCulture, f + '.wxl')
        if os.path.exists(os.path.join(SOURCE_DIR, locf)):
            locd['defLocale'] = locf

        # Is there a localization for this file?
        if localeCulture:
            locf = os.path.join(localeCulture, f + '.wxl')
            if os.path.exists(os.path.join(SOURCE_DIR, locf)):
                locd['locale'] = locf

        # Add this to the list of files for this locale
        fl.append(locd)
    return fl


def win_installer():
    if not os.path.exists(WIX_DIR):
        raise Exception("Failed to locate WiX toolset. Check WIX_DIR is set and correct.\nWIX_DIR=%s" % WIX_DIR)

    # Compose the path to the substorage script.
    subStoreScript = os.path.join(WINDOWS_SDK_DIR, 'Samples', 'sysmgmt', 'msi', 'scripts', 'WiSubStg.vbs')
    if not os.path.exists(subStoreScript):
        raise Exception("Failed to locate WiSubStg.vbs script in the Windows SDK. Check WINDOWS_SDK_DIR is set and correct.\nWINDOWS_SDK_DIR=%s" % WINDOWS_SDK_DIR)

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

    # Compile the base language installer.
    if not quiet:
        print 'Compiling base-/default- language installer...'

    base = dict(culture = 'en-us', codepage = 1252, language = 1033)
    fl = build_file_list(files, 'en-us')
    compile_wix_sources(fl, base)
    link_wix_objfiles(OUTPUT_DIR, OUTFILE, fl, base)

    # Compile all localized versions.
    locales = [dict(culture = 'da-dk', codepage = 1252, language = 1030),
               dict(culture = 'fr-fr', codepage = 1252, language = 1036),
               dict(culture = 'ro-ro', codepage = 1250, language = 1048)]
    for l in locales:
        if not quiet:
            print 'Compiling %s localized installer...' % l['culture']

        fl = build_file_list(files, 'en-us', l['culture'])
        compile_wix_sources(fl, l)
        link_wix_objfiles(WORK_DIR, OUTFILE + '_' + l['culture'], fl, l)

        # Construct the locale transform.
        if not quiet:
            print 'Constructing ms transform...'

        baseMSI = os.path.join(OUTPUT_DIR, OUTFILE + '.msi')
        localeMSI = os.path.join(WORK_DIR, OUTFILE + '_' + l['culture'] + '.msi')
        outFile   = os.path.join(WORK_DIR, OUTFILE + '_' + l['culture'] + '.mst')

        cmd = ['torch', '-nologo', '-t', 'language', baseMSI, localeMSI, '-out', outFile]
        if subprocess.call(cmd, cwd=WIX_DIR):
            raise Exception("Failed building ms transform for %s" % localeMSI)

        # Merge the transform into the base installer.
        if not quiet:
            print 'Merging transform into base-/default- language installer...'

        cmd = ['cscript', '/nologo', subStoreScript, baseMSI, outFile, str(l['culture'])]
        if subprocess.call(cmd):
            raise Exception("Failed merging ms transform %s into %s" % (outFile, baseMSI))

    # Cleanup
    clean_work_dir()


def main():
    global quiet
    global verbose

    find_version()

    quiet = '-quiet' in sys.argv
    if not quiet:
        verbose = '-verbose' in sys.argv

    if not quiet:
        print "Checking OS...",

    try:
        if sys.platform != "win32":
            print "Unknown!"
            print "I don't know how to make an installer on this platform."
            exit_with_error()

        if not quiet:
            print "Windows"
        win_installer()
    except Exception, x:
        print "Errors encountered building Doomsday Engine Installer."
        print x
        exit_with_error()

    os.chdir(LAUNCH_DIR)

if __name__ == '__main__':
    main()
