:<<"::is_bash"
@echo off
goto :is_windows
::is_bash
#
exec $(dirname $0)/src/gn/build-posix.sh #
echo "src/gn/build-posix.sh: failure" #
exit 1 #

:is_windows
setlocal ENABLEEXTENSIONS

rem
rem Check for Visual Studio
if not "x%VSINSTALLDIR%" == "x" goto :vs_cmd_ok

echo VSINSTALLDIR not set. Please install Visual Studio (there is a no-charge
echo version) and run this command inside a "Command Prompt for Visual Studio"
echo window.
exit /b 1
goto :eof

:vs_cmd_ok
call vsdevcmd -arch=x64
if errorlevel 1 goto :vsdevcmd_failed

rem
rem Check for Visual C++ (it is not guaranteed to be present in a default
rem install of Visual Studio)
set startup_dir=%~dp0
cd %TMP%
echo int main() { return 0; } > t.c
cl /nologo t.c
if errorlevel 1 goto :visual_cpp_failed

del /q t.c t.obj t.exe
cd %startup_dir%

rem
rem Check for git
set found_git=
for %%a in ("%PATH:;=";"%") do call :find_in_path "%%~a"

if not "x%found_git%" == "x" goto :found_git_ok
echo git not found in PATH. Please install Git:
echo.
echo https://git-scm.com
echo.
exit /b 1
goto :eof

:find_in_path
set str1="%1"
if not "x%str1:git\cmd=%" == "x%str1%" set found_git="%1"
goto :eof

:find_python3
if "x%1" == "x" goto :eof
if "x%keyname:Python\PythonCore\3=%" == "x%keyname%" goto :eof
set PythonDir=%1
goto :eof

:find_python
rem keys are parsed as %1
rem values underneath the current key are parsed as:
rem name="%1" type="%2" value="%3"
if not "x%2" == "x" goto :eof
set keyname=%1
rem Search for an "InstallPath" subkey
FOR /F "usebackq tokens=1-3" %%A IN (`REG QUERY %keyname%\InstallPath /ve 2^>nul`) DO call :find_python3 %%C
goto :eof

:vsdevcmd_failed
echo.
echo vsdevcmd failed. Please install Visual Studio (there is a no-charge
echo version). Make sure during the install to click "customize" and check the
echo box for "Visual C++," then start a "Command Prompt for Visual Studio"
echo window and run build.cmd again.
exit /b 1
goto :eof

:visual_cpp_failed
echo.
echo The "cl /nologo t.c" command failed. Please install Visual Studio and make
echo sure to check the box for "Visual C++." Surprisingly, Visual Studio no
echo longer installs Visual C++ by default!
echo.
echo Here is the output of "cl /nologo t.c":
echo ---------------------------------------

cl /nologo t.c
cd %startup_dir%

exit /b 1
goto :eof

:subcmd_failed
exit /b 1
goto :eof

:found_git_ok
rem
rem git was found, clone submodules ("init submodules").
cd %startup_dir%
cmd /c git submodule update --init --recursive

FOR /F "usebackq tokens=1-3" %%A IN (`REG QUERY HKLM\SOFTWARE\Python\PythonCore 2^>nul`) DO call :find_python %%A %%B %%C
FOR /F "usebackq tokens=1-3" %%A IN (`REG QUERY HKCU\SOFTWARE\Python\PythonCore 2^>nul`) DO call :find_python %%A %%B %%C
FOR /F "usebackq tokens=1-3" %%A IN (`REG QUERY HKLM\SOFTWARE\Wow6432Node\Python\PythonCore 2^>nul`) DO call :find_python %%A %%B %%C
if not "x%PythonDir%" == "x" goto :python_ok

echo Python3 not found. Please install Python3 for Windows:
echo.
echo https://python.org (version 3)
exit /b 1
goto :eof

:python_ok
rem
rem Download "deps" for skia
cd vendor\skia
%PythonDir%\python.exe tools/git-sync-deps
cd ..\..

rem
rem Build subgn and then volcano
cd vendor\subgn
if exist out_bootstrap\gn.exe goto :skip_subgn
cmd /c build.cmd
if errorlevel 1 goto :subcmd_failed
:skip_subgn
cd ..\..
xcopy /y vendor\subgn\subninja\ninja.exe vendor\subgn > nul
xcopy /y vendor\subgn\out_bootstrap\gn.exe vendor\subgn > nul

rem
rem Apply patches. These commands should be idempotent: if they are run twice,
rem the results should not change.
rmdir /s /q vendor\glfw\deps\vulkan 2> nul
%PythonDir%\python.exe src/gn/winpatch.py

echo gn gen out/Debug
vendor\subgn\gn.exe gen out/Debug
if errorlevel 1 goto :subcmd_failed

echo gn gen out/DebugDLL
vendor\subgn\gn.exe gen out/DebugDLL --args="use_dynamic_crt = true"
if errorlevel 1 goto :subcmd_failed

cd %startup_dir%
rem DLLs have to be built separately to have use_dynamic_crt = true.
echo ninja -C out/DebugDLL vulkan-1.dll
vendor\subgn\ninja.exe -C out/DebugDLL vulkan-layers vulkan-1.dll
if errorlevel 1 goto :subcmd_failed

if not "x%VOLCANO_NO_OUT%" == "x" goto :eof
xcopy /y /e out\DebugDLL\vulkan-1.* out\Debug
xcopy /y /e out\DebugDLL\vulkan out\Debug

rem Everything else is built separately to have use_dynamic_crt = false.
echo ninja -C out/Debug
vendor\subgn\ninja.exe -C out/Debug
if errorlevel 1 goto :subcmd_failed

:eof
