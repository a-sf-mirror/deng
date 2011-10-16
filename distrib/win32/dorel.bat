@echo off
:: Does a complete Win32 Binary Release distribution.

:: Build Environment Setup ------------------------------------------------

:: Modify these paths for your system.
set PYTHON_DIR=C:/Python25

set WIX_DIR=C:/Program Files (x86)/Windows Installer XML v3.5

set WINDOWS_SDK_DIR=C:/Program Files/Microsoft SDKs/Windows/v7.0

:: Configure environment.
call ..\..\doomsday\build\win32\envconfig.bat

:: Build and Package Doomsday ---------------------------------------------

cd ..
call %PYTHON_DIR%\python platform_release.py