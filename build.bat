@echo off
rem Compila build\version.dll (x86) com o Visual Studio 2022 / Build Tools.
setlocal
cd /d "%~dp0"
if defined VSCMD_VER goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VS=%%i"
if not defined VS set "VS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
:build
if not exist build mkdir build
cl /nologo /O2 /W3 /MT /LD src\autoroute_fix.c /Fo:build\ /Fe:build\version.dll /link /MACHINE:X86 || exit /b 1
certutil -hashfile build\version.dll SHA256 | findstr /v ":"
