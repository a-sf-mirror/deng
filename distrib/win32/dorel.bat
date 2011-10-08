@echo off
:: Does a complete Win32 Binary Release distribution.

:: Build Environment Setup ------------------------------------------------
call ..\..\doomsday\build\win32\envconfig.bat

:: Build number.
SET DOOMSDAY_BUILD=%1
echo Doomsday build number is %DOOMSDAY_BUILD%.

:: Build and Package Snowberry --------------------------------------------
cd ..\..\snowberry
call build.bat
cd ..\distrib\win32

:: Extra dependencies.
REM copy %windir%\system32\msvcr100.dll ..\products

:: Compile Doomsday -------------------------------------------------------
SET BUILDFAILURE=0
rd/s/q work
md work
cd work
qmake ..\..\..\doomsday\doomsday.pro CONFIG+=release DENG_BUILD=%DOOMSDAY_BUILD%
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
%JOM%
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
%JOM% install
IF NOT %ERRORLEVEL% == 0 SET BUILDFAILURE=1
cd ..
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
