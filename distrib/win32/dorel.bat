@echo off
:: Does a complete Win32 Binary Release distribution.

:: Build Preparation -----------------------------------------------------
:: Ensure we have all required arguments
IF "%1"=="" goto PrintUsage
IF "%2"=="" goto PrintUsage
IF "%3"=="" goto PrintUsage
IF "%4"=="" goto PrintUsage
goto ConfigEnv

:PrintUsage
echo Usage: %0 ^<majorver^> ^<minorver^> ^<buildver^> ^<revisionver^> ^[^<buildnum^>^]
set BUILDFAILURE=1
goto Failure

:: Build Environment Setup ------------------------------------------------
:ConfigEnv
:: Build versioning information.
SET DOOMSDAY_MAJORVERSION=%1
SET DOOMSDAY_MINORVERSION=%2
SET DOOMSDAY_BUILDVERSION=%3
SET DOOMSDAY_REVISIONVERSION=%4
SET DOOMSDAY_BUILD=%5

echo Doomsday version is: Major:%DOOMSDAY_MAJORVERSION% Minor:%DOOMSDAY_MINORVERSION% Build:%DOOMSDAY_BUILDVERSION% Revision:%DOOMSDAY_REVISIONVERSION%
echo Doomsday logical build number is: %DOOMSDAY_BUILD%

:: Build and Package Snowberry --------------------------------------------

cd ..\..\snowberry
call build.bat
cd ..\distrib\win32

:: Build and Package Doomsday ---------------------------------------------

echo Building Doomsday...
:: Configure environment.
call ..\..\doomsday\build\win32\envconfig.bat

:: Prepare the work directory.
SET BUILDFAILURE=0
rd/s/q work
md work

:: Build!
cd work
qmake ..\..\..\doomsday\doomsday.pro CONFIG+=release DENG_BUILD=%DOOMSDAY_BUILD%
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
%JOM%
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
%JOM% install
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
cd ..

:: Clean up the work directory.
rd/s/q work

IF %BUILDFAILURE% == 1 GOTO Failure

:: Build Installer --------------------------------------------------------

call buildinstaller.bat
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
IF %BUILDFAILURE% == 1 GOTO Failure
goto TheEnd

:Failure
echo Failure during build!
exit /b 1

:TheEnd
