@echo off
:: Win32 specific build script for generating a Windows Installer for the
:: Doomsday Engine, it's plugins and related deng Team products by leveraging
:: the awesomely powerful Windows Installer XML (WiX) toolset.

:: Build Tools Setup ------------------------------------------------------

echo Setting environment for building Doomsday Engine - Windows Installer...
:: Delayed variable expansion is needed for producing the link object list.
setlocal EnableDelayedExpansion

set PRODUCTSDIR=..\products
set RELEASESDIR=..\releases
set WORKDIR=%cd%\work

:: Take note of the cwd so we can return there post build (tool chaining).
set STARTDIR=%cd%

:: Build Preparation ------------------------------------------------------

:: Ensure environment has been configured
IF %DOOMSDAY_MAJORVERSION% == "" (
echo DOOMSDAY_MAJORVERSION is not set.
GOTO Failure
)

IF %DOOMSDAY_MINORVERSION% == "" (
echo DOOMSDAY_MINORVERSION is not set.
GOTO Failure
)

IF %DOOMSDAY_BUILDVERSION% == "" (
echo DOOMSDAY_BUILDVERSION is not set.
GOTO Failure
)

IF %DOOMSDAY_REVISIONVERSION% == "" (
echo DOOMSDAY_REVISIONVERSION is not set.
GOTO Failure
)

:: Ensure releases directory exists.
pushd "%RELEASESDIR%" 2>NUL && popd
IF NOT %ERRORLEVEL% == 0 (
echo Unable to access releases directory: %RELEASESDIR%
GOTO Failure
)

:: Ensure products directory exists.
pushd "%PRODUCTSDIR%" 2>NUL && popd
IF NOT %ERRORLEVEL% == 0 (
echo Unable to access products directory: %PRODUCTSDIR%
GOTO Failure
)

:: Ensure work directory exists.
pushd "%WORKDIR%" 2>NUL && popd
IF NOT %ERRORLEVEL% == 0 (
echo Creating work directory: %WORKDIR%
md %WORKDIR%
)

:: Compilation ------------------------------------------------------------

set CDEFS=-dProductMajorVersion="%DOOMSDAY_MAJORVERSION%"
set CDEFS=!CDEFS! -dProductMinorVersion="%DOOMSDAY_MINORVERSION%"
set CDEFS=!CDEFS! -dProductBuildVersion="%DOOMSDAY_BUILDVERSION%"
set CDEFS=!CDEFS! -dProductRevisionVersion="%DOOMSDAY_REVISIONVERSION%"
set CDEFS=!CDEFS! -dProductsDir="%PRODUCTSDIR%"

echo Compiling WiX source files...
FOR /F "tokens=1*" %%i IN (installer_component_filelist.rsp) DO (
candle %CDEFS% -nologo -out %WORKDIR%\ -wx0099 %%i.wxs
IF NOT %ERRORLEVEL% == 0 GOTO Failure
)

:: Link -------------------------------------------------------------------
echo Linking WiX object files...

:: Compose outfile name
set OUTFILE=Doomsday_%DOOMSDAY_MAJORVERSION%.%DOOMSDAY_MINORVERSION%.%DOOMSDAY_BUILDVERSION%
IF NOT %DOOMSDAY_BUILD% == "" set OUTFILE=!OUTFILE!_build%DOOMSDAY_BUILD%
set OUTFILE=!OUTFILE!.msi

:: Next we need to build our link object list
set LINKOBJECTS=
FOR /F "tokens=1*" %%i IN (installer_component_filelist.rsp) DO (
set LINKOBJECTS=!LINKOBJECTS! %%i.wixobj
)

:: Link all objects and bind our installables into cabinents.
cd %WORKDIR%\
light -b ..\ -nologo -out %OUTFILE% -ext WixUIExtension -ext WixUtilExtension %LINKOBJECTS%
IF NOT %ERRORLEVEL% == 0 GOTO Failure

:: Return from whence we came...
cd %STARTDIR%

:: Copy installer to the release directory.
copy %WORKDIR%\%OUTFILE% %RELEASESDIR%\%OUTFILE%

:: Clean up the work directory.
rd/s/q %WORKDIR%

:: Build successful lets get out of here...
GOTO Done

:Failure
echo Errors encountered building Doomsday Engine - Windows Installer.
EXIT /b 1

:Done
