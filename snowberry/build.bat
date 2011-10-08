@echo off
:: This batch file uses Py2exe to create a Windows executable.
:: A beta distribution package is compiled by compressing the
:: executable, the necessary resource files, and the Snowberry 
:: scripts into a self-extracting RAR archive.

:: Build Preparation ------------------------------------------------------
md dist
del /f /q dist\*

:: Build Environment Setup ------------------------------------------------
SET PYTHON_DIR=C:\Python25

:: Build and Package ------------------------------------------------------
echo Building Snowberry...
:: Make the executable.
"%PYTHON_DIR%"\python setup.py py2exe

:: Additional binary dependencies.
echo Copying dependencies to ./dist...

copy %PYTHON_DIR%\lib\site-packages\wx-2.8-msw-ansi\wx\MSVCP71.dll dist
copy %PYTHON_DIR%\lib\site-packages\wx-2.8-msw-ansi\wx\gdiplus.dll dist
