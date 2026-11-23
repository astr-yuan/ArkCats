@echo off
setlocal
cd /d "%~dp0" || exit /b 1

where cl >nul 2>nul
if not errorlevel 1 goto msvc
where g++ >nul 2>nul
if not errorlevel 1 goto mingw
echo [ERROR] Start an x64 MSVC developer prompt or add MinGW-w64 to PATH.
exit /b 1

:msvc
if not exist build\msvc\ mkdir build\msvc
rc /nologo /fobuild\msvc\version.res version.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /W3 /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 ^
    src\main.cpp build\msvc\version.res /Fobuild\msvc\main.obj /FeArkCats.exe ^
    /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest ^
    user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib wtsapi32.lib
if errorlevel 1 exit /b %errorlevel%
goto helper

:mingw
if not exist build\mingw\ mkdir build\mingw
windres -i app.rc -O coff -o build\mingw\app.res.o
if errorlevel 1 exit /b %errorlevel%
g++ -std=c++17 -O2 -mwindows -municode -static -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 ^
    src\main.cpp build\mingw\app.res.o -o ArkCats.exe ^
    -luser32 -lgdi32 -lgdiplus -lshell32 -ladvapi32 -lwtsapi32
if errorlevel 1 exit /b %errorlevel%
:helper
call notifications\build.bat "%~dp0ArkCatsNotifications.exe"
if errorlevel 1 exit /b %errorlevel%
echo [OK] Built ArkCats.exe and ArkCatsNotifications.exe.
exit /b 0
