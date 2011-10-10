@echo off
:: Does a complete Win32 Binary Release distribution.

:: Build Preparation -----------------------------------------------------

SET BUILDFAILURE=0

:: Ensure we have all required arguments
IF "%1"=="" goto PrintUsage
goto ConfigEnv

:PrintUsage
echo Usage: %0 ^<buildnum^>
set BUILDFAILURE=1
goto Failure

:: Build Environment Setup ------------------------------------------------
:ConfigEnv
:: Build information.
SET BUILDFAILURE=0
SET DOOMSDAY_BUILD=%1

:: Build and Package Doomsday ---------------------------------------------

:: Configure environment.
call ..\..\doomsday\build\win32\envconfig.bat

:: Prepare the work directory.
echo Preparing work directory...
rd/s/q work
md work

:: Build!
echo Building Doomsday...
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

IF %BUILDFAILURE% == 0 GOTO TheEnd

:Failure
echo Failure during build!
exit /b 1

:TheEnd
