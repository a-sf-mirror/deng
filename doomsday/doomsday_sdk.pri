# The Doomsday Engine Project
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# qmake .pri file for projects using the Doomsday SDK.
#
# The Doomsday SDK is distributed under the GNU Lesser General Public
# License version 3 (or, at your option, any later version). Please
# visit http://www.gnu.org/licenses/lgpl.html for details.
#
# Variables:
# - DENG_CONFIG     Names of supporting libraries to use (gui, appfw, shell)

DENG_SDK_DIR = $$PWD

exists($$DENG_SDK_DIR/include/doomsday) {
    INCLUDEPATH += $$DENG_SDK_DIR/include/doomsday
}
else {
    INCLUDEPATH += $$DENG_SDK_DIR/include
}

LIBS += -L$$DENG_SDK_DIR/lib

# The core library is always required.
LIBS += -ldeng2

# Supporting libraries are optional.
contains(DENG_CONFIG, gui):   LIBS += -ldeng_gui
contains(DENG_CONFIG, appfw): LIBS += -ldeng_appfw
contains(DENG_CONFIG, shell): LIBS += -ldeng_shell

# Instruct the dynamic linker to load the libs from the SDK lib dir.
*-g++*|*-gcc*|*-clang* {
    QMAKE_LFLAGS += -Wl,-rpath,$$DENG_SDK_DIR/lib
}

defineTest(dengClear) {
    # 1: file to remove
    win32: system(del /q \"$$1\")
     else: system(rm -f \"$$1\")
}

defineTest(dengPack) {
    # 1: path of a .pack file
    # 2: actual root directory
    # 3: files to include, relative to the root
    system(cd \"$$2\" && zip -r \"$$1\" $$3)
}

DENG_MODULES = $$DENG_SDK_DIR/modules/*.de

defineTest(dengPackModules) {
    # 1: path of a .pack file
    dengPack($$1, $$DENG_SDK_DIR, modules/*.de)
}

defineReplace(dengFindLibDir) {
    # Determines the appropriate library directory given prefix $$1
    prefix = $$1
    dir = $$prefix/lib
    contains(QMAKE_HOST.arch, x86_64) {
        exists($$prefix/lib64) {
            dir = $$prefix/lib64
        }
        exists($$prefix/lib/x86_64-linux-gnu) {
            dir = $$prefix/lib/x86_64-linux-gnu
        }
    }
    return($$dir)
}

defineTest(dengDeploy) {
    # 1: app name
    # 2: install prefix
    # 3: base pack file
    prefix = $$2
    INSTALLS += target basepack
    basepack.files = $$3
    contains(DENG_CONFIG, gui):   INSTALLS += libgui
    contains(DENG_CONFIG, appfw): INSTALLS += libappfw
    contains(DENG_CONFIG, shell): INSTALLS += libshell
    win32 {
    }
    else:macx {
    }
    else {
        target.path = $$prefix/bin
        basepack.path = $$prefix/share/$${1}

        libgui.files   = $$DENG_SDK_DIR/lib/libdeng_gui.so
        libappfw.files = $$DENG_SDK_DIR/lib/libdeng_appfw.so
        libshell.files = $$DENG_SDK_DIR/lib/libdeng_shell.so

        libDir = $$dengFindLibDir($$prefix)
        libgui.path   = $$libDir
        libappfw.path = $$libDir
        libshell.path = $$libDir
    }
    export(INSTALLS)
    export(target.path)
    export(basepack.files)
    export(basepack.path)
    export(libgui.files)
    export(libgui.path)
    export(libappfw.files)
    export(libappfw.path)
    export(libshell.files)
    export(libshell.path)
}
