:<<"::is_bash"
@echo off
goto :is_windows
::is_bash
#
exec $(dirname $0)/gn/build-posix.sh #
echo "gn/build-posix.sh: failure" #
exit 1 #

:is_windows
set startup_dir=%CD%
set depot_tools=
set msys_git=
for %%a in ("%PATH:;=";"%") do call :find_in_path "%%~a"

if not x%depot_tools% == x goto :depot_tools_ok
echo depot_tools not found
exit 1
goto :eof

:find_in_path
set str1="%1"
if not "x%str1:Git=%" == "x%str1%" set msys_git=%1
if not "x%str1:depot_tools=%" == "x%str1%" set depot_tools=%1
goto :eof

:depot_tools_ok
if not x%msys_git% == x goto :msys_git_ok
echo git not found in PATH. Please install the Msys (Git for Windows) toolchain:
echo.
echo https://www.chromium.org/developers/how-tos/install-depot-tools
exit 1
goto :eof

:msys_git_ok
cd %~dp0
cmd /c git submodule update --init --recursive --depth 200
cd buildtools\win
for %%a in (gn) do call :download_tool %%~a
cd ..\..\gn
xcopy /q /y ..\vendor\skia\gn\*.*
cd ..
mklink /j third_party vendor\skia\third_party
gn gen out\Debug --ide=vs

cd %startup_dir%
goto :eof

:download_tool
echo %CD% download_from_google_storage -n -b chromium-%1 -s %1.exe.sha1
cmd /c download_from_google_storage -n -b chromium-%1 -s %1.exe.sha1
goto :eof

:eof
