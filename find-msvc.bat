@echo off
REM Locates Visual Studio with vswhere, enters its x64 developer environment, and puts the
REM CMake and Ninja that ship with it on PATH. Called by configure.bat and build.bat so
REM neither of them has to hardcode an install path.
REM
REM Deliberately not wrapped in setlocal: the caller wants the environment this sets up.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo Could not find vswhere.exe. Is Visual Studio installed?
	exit /b 1
)

set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"

if "%VSPATH%"=="" (
	echo No Visual Studio install with the C++ toolset was found.
	exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
REM The shared build environment (rule 45): strips build-machine paths from the DLL and PDB. Unset on any other machine, so the build is unchanged there.
if not defined MODS_BUILD_ENV for /f "tokens=2,*" %%A in ('reg query "HKCU\Environment" /v MODS_BUILD_ENV 2^>nul ^| findstr /i /c:"MODS_BUILD_ENV"') do call set "MODS_BUILD_ENV=%%B"
if defined MODS_BUILD_ENV if exist "%MODS_BUILD_ENV%" call "%MODS_BUILD_ENV%"
exit /b 0
