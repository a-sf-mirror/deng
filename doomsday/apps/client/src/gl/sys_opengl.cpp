/** @file sys_opengl.cpp
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "gl/sys_opengl.h"

#include <QSet>
#include <QStringList>
#include <de/libcore.h>
#include <de/concurrency.h>
#include <de/GLInfo>
#include <de/GLState>
#include "sys_system.h"
#include "gl/gl_main.h"

#ifdef WIN32
#   define GETPROC(Type, x)   x = de::function_cast<Type>(wglGetProcAddress(#x))
#elif defined (DENG_X11)
#   include <GL/glx.h>
#   undef None
#   define GETPROC(Type, x)   x = de::function_cast<Type>(glXGetProcAddress((GLubyte const *)#x))
#endif

gl_state_t GL_state;

static dd_bool doneEarlyInit = false;
static dd_bool sysOpenGLInited = false;
static dd_bool firstTimeInit = true;

static void initialize(void)
{
    de::GLInfo::Extensions const &ext = de::GLInfo::extensions();

    if(CommandLine_Exists("-noanifilter"))
    {
        GL_state.features.texFilterAniso = false;
    }

    if (CommandLine_Exists("-texcomp") && !CommandLine_Exists("-notexcomp"))
    {
        GL_state.features.texCompression = true;
    }
#ifdef USE_TEXTURE_COMPRESSION_S3
    // Enabled by default if available.
    if (ext.EXT_texture_compression_s3tc)
    {
        GLint iVal;
        LIBGUI_GL.glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        LIBGUI_ASSERT_GL_OK();
        if (iVal == 0)// || LIBGUI_GL.glGetError() != GL_NO_ERROR)
            GL_state.features.texCompression = false;
    }
#else
    GL_state.features.texCompression = false;
#endif
}

#define TABBED(A, B)  _E(Ta) "  " _E(l) A _E(.) " " _E(Tb) << B << "\n"

de::String Sys_GLDescription()
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    de::String str;
    QTextStream os(&str);

    os << _E(b) "OpenGL information:\n" << _E(.);

    os << TABBED("Version:",  (char const *) LIBGUI_GL.glGetString(GL_VERSION));
    os << TABBED("Renderer:", (char const *) LIBGUI_GL.glGetString(GL_RENDERER));
    os << TABBED("Vendor:",   (char const *) LIBGUI_GL.glGetString(GL_VENDOR));

    LIBGUI_ASSERT_GL_OK();

    os << _E(T`) "Capabilities:\n";

    GLint iVal;

#ifdef USE_TEXTURE_COMPRESSION_S3
    if(de::GLInfo::extensions().EXT_texture_compression_s3tc)
    {
        LIBGUI_GL.glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        LIBGUI_ASSERT_GL_OK();
        os << TABBED("Compressed texture formats:", iVal);
    }
#endif

    os << TABBED("Use texture compression:", (GL_state.features.texCompression? "yes" : "no"));

    os << TABBED("Available texture units:", de::GLInfo::limits().maxTexUnits);

    if(de::GLInfo::extensions().EXT_texture_filter_anisotropic)
    {
        os << TABBED("Maximum texture anisotropy:", de::GLInfo::limits().maxTexFilterAniso);
    }
    else
    {
        os << _E(Ta) "  Variable texture anisotropy unavailable.";
    }

    os << TABBED("Maximum texture size:", de::GLInfo::limits().maxTexSize);

    os << TABBED("Line width granularity:", de::GLInfo::limits().smoothLineWidthGranularity);

    os << TABBED("Line width range:",
                 de::GLInfo::limits().smoothLineWidth.start << "..." <<
                 de::GLInfo::limits().smoothLineWidth.end);

    return str.rightStrip();

#undef TABBED
}

static void printGLUInfo(void)
{
    LOG_GL_MSG("%s") << Sys_GLDescription();

    Sys_GLPrintExtensions();
}

dd_bool Sys_GLPreInit(void)
{
    if(novideo) return true;
    if(doneEarlyInit) return true; // Already been here??

    GL_state.features.texCompression = false;
    GL_state.features.texFilterAniso = true;
    GL_state.currentLineWidth = 1.5f;
    GL_state.currentPointSize = 1.5f;

    doneEarlyInit = true;
    return true;
}

dd_bool Sys_GLInitialize(void)
{
    if(novideo) return true;

    LOG_AS("Sys_GLInitialize");

    assert(doneEarlyInit);

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    assert(!Sys_GLCheckError());

    if(firstTimeInit)
    {
#if defined (DENG_OPENGL)
        const GLubyte* versionStr = LIBGUI_GL.glGetString(GL_VERSION);
        double version = (versionStr? strtod((const char*) versionStr, NULL) : 0);
        if(version == 0)
        {
            LOG_GL_WARNING("Failed to determine OpenGL version; driver reports: %s")
                    << LIBGUI_GL.glGetString(GL_VERSION);
        }
        else if(version < 3.3)
        {
            if(!CommandLine_Exists("-noglcheck"))
            {
                Sys_CriticalMessagef("Your OpenGL is too old!\n"
                                     "  Driver version: %s\n"
                                     "  The minimum supported version is 2.0",
                                     LIBGUI_GL.glGetString(GL_VERSION));
                return false;
            }
            else
            {
                LOG_GL_WARNING("OpenGL may be too old (3.3+ required, "
                               "but driver reports %s)") << LIBGUI_GL.glGetString(GL_VERSION);
            }
        }
#endif

        initialize();
        printGLUInfo();

        firstTimeInit = false;
    }

    // GL system is now fully initialized.
    sysOpenGLInited = true;

    /**
     * We can now (re)configure GL state that is dependent upon extensions
     * which may or may not be present on the host system.
     */

    // Use nice quality for mipmaps please.
    //if(GL_state.features.genMipmap && de::GLInfo::extensions().SGIS_generate_mipmap)
    //LIBGUI_GL.glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

    assert(!Sys_GLCheckError());

    return true;
}

void Sys_GLShutdown(void)
{
    if(!sysOpenGLInited) return;
    // No cleanup.
    sysOpenGLInited = false;
}

void Sys_GLConfigureDefaultState(void)
{
    LIBGUI_ASSERT_GL_OK();

    GLfloat fogcol[4] = { .54f, .54f, .54f, 1 };

    /**
     * @note Only core OpenGL features can be configured at this time
     *       because we have not yet queried for available extensions,
     *       or configured our prefered feature default state.
     *
     *       This means that GL_state.extensions and GL_state.features
     *       cannot be accessed here during initial startup.
     */
    assert(doneEarlyInit);

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glFrontFace(GL_CW);
    LIBGUI_ASSERT_GL_OK();

    de::GLState::current()
            .setCull(de::gl::None)
            .setDepthTest(false)
            .setDepthFunc(de::gl::Less);

    DGL_Disable(DGL_TEXTURE_2D);
    //LIBGUI_GL.glDisable(GL_TEXTURE_CUBE_MAP);

#if defined (DENG_OPENGL)
    LIBGUI_GL.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    LIBGUI_ASSERT_GL_OK();
#endif

    // The projection matrix.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_LoadIdentity();

    // Initialize the modelview matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_LoadIdentity();

    // Also clear the texture matrix.
    DGL_MatrixMode(DGL_TEXTURE);
    DGL_LoadIdentity();

    de::GLInfo::setLineWidth(GL_state.currentLineWidth);

#if defined (DENG_OPENGL)
    // Setup for antialiased lines/points.
    LIBGUI_GL.glEnable(GL_LINE_SMOOTH);
    LIBGUI_ASSERT_GL_OK();
    LIBGUI_GL.glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    LIBGUI_ASSERT_GL_OK();

    LIBGUI_GL.glPointSize(GL_state.currentPointSize);
    LIBGUI_ASSERT_GL_OK();

    // Prefer good quality in texture compression.
    LIBGUI_GL.glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
    LIBGUI_ASSERT_GL_OK();
#endif

    // Default state for the white fog is off.
    DGL_Disable(DGL_FOG);
    DGL_Fogi(DGL_FOG_MODE, GL_LINEAR);
    DGL_Fogi(DGL_FOG_END, 2100); // This should be tweaked a bit.
    DGL_Fogfv(DGL_FOG_COLOR, fogcol);

    LIBGUI_ASSERT_GL_OK();

    // Configure the default GLState (bottom of the stack).
    de::GLState::current()
            .setBlendFunc(de::gl::SrcAlpha, de::gl::OneMinusSrcAlpha);
}

static de::String omitGLPrefix(de::String str)
{
    if(str.startsWith("GL_")) return str.substr(3);
    return str;
}

static void printExtensions(QStringList extensions)
{
    qSort(extensions);

    // Find all the prefixes.
    QSet<QString> prefixes;
    foreach(QString ext, extensions)
    {
        ext = omitGLPrefix(ext);
        int pos = ext.indexOf("_");
        if(pos > 0)
        {
            prefixes.insert(ext.left(pos));
        }
    }

    QStringList sortedPrefixes = prefixes.toList();
    qSort(sortedPrefixes);
    foreach(QString prefix, sortedPrefixes)
    {
        de::String str;
        QTextStream os(&str);

        os << "    " << prefix << " extensions:\n        " _E(>) _E(2);

        bool first = true;
        foreach(QString ext, extensions)
        {
            ext = omitGLPrefix(ext);
            if(ext.startsWith(prefix + "_"))
            {
                ext.remove(0, prefix.size() + 1);
                if(!first) os << ", ";
                os << ext;
                first = false;
            }
        }

        LOG_GL_MSG("%s") << str;
    }
}

void Sys_GLPrintExtensions(void)
{
    LOG_GL_MSG(_E(b) "OpenGL Extensions:");
    QStringList exts;
    foreach (QByteArray extName, QOpenGLContext::currentContext()->extensions())
    {
        exts << extName;
    }
    printExtensions(exts);

    /*
#if WIN32
    // List the WGL extensions too.
    if(wglGetExtensionsStringARB)
    {
        LOG_GL_MSG("  Extensions (WGL):");
        printExtensions(QString((char const *) ((GLubyte const *(__stdcall *)(HDC))wglGetExtensionsStringARB)(wglGetCurrentDC())).split(" ", QString::SkipEmptyParts));
    }
#endif

#ifdef Q_WS_X11
    // List GLX extensions.
    LOG_GL_MSG("  Extensions (GLX):");
    printExtensions(QString(getGLXExtensionsString()).split(" ", QString::SkipEmptyParts));
#endif
    */
}

dd_bool Sys_GLCheckErrorArgs(char const *file, int line)
{
    if (novideo) return false;
#ifdef DENG_DEBUG
    de::GLInfo::checkError(file, line);
#endif
    return false;
}
