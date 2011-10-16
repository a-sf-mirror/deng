@echo off
:: Does a complete Win32 Binary Release distribution.

:: Build Environment Setup ------------------------------------------------

:: Configure environment.
call ..\..\doomsday\build\win32\envconfig.bat

:: Build and Package Doomsday ---------------------------------------------

:: The common platform_release.py script expects the cwd to be that which
:: contains it due to inter-script references in the implementation.
cd ..

:: Build the release!
call %PYTHON_DIR%\python platform_release.py

:: Return to /win32
cd win32