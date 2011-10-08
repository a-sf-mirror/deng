@echo off
:: Win32 specific build script for generating a Windows Installer for the
:: Doomsday Engine, it's plugins and related deng Team products by leveraging
:: the awesomely powerful Windows Installer XML (WiX) toolset.

:: Build Tools Setup ------------------------------------------------------

echo Setting environment for building Doomsday Engine - Windows Installer...
:: Delayed variable expansion is needed for producing the link object list.
setlocal EnableDelayedExpansion

set DOOMSDAY_MAJORVERSION=1
set DOOMSDAY_MINORVERSION=9
set DOOMSDAY_BUILDVERSION=7
set DOOMSDAY_REVISIONVERSION=0
set DOOMSDAY_BUILDNAME=build
set DOOMSDAY_BUILD=273

set PRODUCTSDIR=../products
set WORKDIR=work

:: Take note of the cwd so we can return there post build (tool chaining).
set STARTDIR=%cd%

:: Build Preparation ------------------------------------------------------

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
IF NOT %DOOMSDAY_BUILD% == 0 set OUTFILE=!OUTFILE!_%DOOMSDAY_BUILDNAME%%DOOMSDAY_BUILD%

:: Next we need to build our link object list
set LINKOBJECTS=
FOR /F "tokens=1*" %%i IN (installer_component_filelist.rsp) DO (
set LINKOBJECTS=!LINKOBJECTS! %%i.wixobj
)

:: Link all objects and bind our installables into cabinents.
cd %WORKDIR%\
light -b ../ -nologo -out %OUTFILE%.msi -ext WixUIExtension -ext WixUtilExtension %LINKOBJECTS%
IF NOT %ERRORLEVEL% == 0 GOTO Failure

:: Return from whence we came...
cd %STARTDIR%
GOTO Done

:Failure
echo Errors encountered building Doomsday Engine - Windows Installer.
EXIT /b 1

:Done
