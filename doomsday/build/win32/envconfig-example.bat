@echo off
:: Sets up the command line environment for building.
:: This is automatically called by dorel.bat.

:: Directory Config -------------------------------------------------------

:: Modify these paths for your system.
set MSVC9_DIR=C:/Program Files/Microsoft Visual Studio 9.0

set QTCREATOR_DIR=c:/QtSDK/QtCreator

set QT_BIN_DIR=C:/QtSDK/Desktop/Qt/4.7.4/msvc2008/bin

set PYTHON_DIR=C:/Python25

set WIX_DIR=C:/Program Files/Windows Installer XML v3.5

set WINDOWS_SDK_DIR=C:/Program Files/Microsoft SDKs/Windows/v7.0

:: Build Tools Setup ------------------------------------------------------

:: Visual C++ environment.
call "%MSVC9_DIR%\vc\vcvarsall.bat"

:: -- Qt environment.
set JOM=%QTCREATOR_DIR%\bin\jom.exe
call "%QT_BIN_DIR%\qtenv2.bat"
