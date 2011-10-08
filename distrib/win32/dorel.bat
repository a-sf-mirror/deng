@echo off
REM -- Does a complete Win32 Binary Release distribution.

REM -- Set up the build environment.
call ..\..\doomsday\build\win32\envconfig.bat

REM -- Build number.
SET DOOMSDAY_BUILD=%1
echo Doomsday build number is %DOOMSDAY_BUILD%.

REM -- Package a Snowberry binary.
cd ..\..\snowberry
call build.bat
cd ..\distrib\win32

REM -- Extra dependencies.
REM copy %windir%\system32\msvcr100.dll ..\products

REM -- Recompile.
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

REM -- Run the Inno Setup Compiler.
REM "C:\Program Files\Inno Setup 5\Compil32.exe" /cc setup.iss
echo Installer creation not presently implemented.
set ERRORLEVEL=1
goto Failure

goto TheEnd

:Failure
echo Failure during build!
exit /b 1

:TheEnd
