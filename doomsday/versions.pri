defineReplace(findVersion) {
    info = $$system(python \"$$PWD/../distrib/build_version.py\" \"$$PWD/$$1\")
    # Result: versionbase buildnum reltype winver
    # Just use the base version (x.y.z) for now.
    return($$member(info, 0))
}

DENG_VERSION            = $$findVersion(engine/portable/include/dd_version.h)

JDOOM_VERSION           = $$findVersion(plugins/jdoom/include/version.h)
JHERETIC_VERSION        = $$findVersion(plugins/jheretic/include/version.h)
JHEXEN_VERSION          = $$findVersion(plugins/jhexen/include/version.h)

DEHREAD_VERSION         = $$findVersion(plugins/dehread/include/version.h)
WADMAPCONVERTER_VERSION = $$findVersion(plugins/wadmapconverter/include/version.h)
DIRECTSOUND_VERSION     = $$findVersion(plugins/directsound/include/version.h)
OPENAL_VERSION          = $$findVersion(plugins/openal/include/version.h)
WINMM_VERSION           = $$findVersion(plugins/winmm/include/version.h)

JDOOM64_VERSION         = $$findVersion(plugins/jdoom64/include/version.h)
