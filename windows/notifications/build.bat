@echo off
setlocal
set "TARGET=%~f1"
if not defined TARGET set "TARGET=%~dp0..\ChristineNotifications.exe"
cd /d "%~dp0" || exit /b 1
set "NETFX=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319"
set "METADATA=%SystemRoot%\System32\WinMetadata"
if exist "%SystemRoot%\Sysnative\WinMetadata\Windows.UI.winmd" set "METADATA=%SystemRoot%\Sysnative\WinMetadata"
if not exist "%NETFX%\csc.exe" goto missing
if not exist "%METADATA%\Windows.UI.winmd" goto missing
"%NETFX%\csc.exe" /nologo /utf8output /codepage:65001 /optimize+ /target:winexe /platform:x64 ^
    /out:"%TARGET%" /r:System.Windows.Forms.dll /r:"%NETFX%\System.Runtime.dll" ^
    /r:"%METADATA%\Windows.UI.winmd" /r:"%METADATA%\Windows.Foundation.winmd" ^
    Program.cs ParentWindow.cs SessionWindow.cs ToastSource.cs ToastTracker.cs
exit /b %errorlevel%
:missing
echo [ERROR] The notification helper needs Windows x64, .NET Framework 4.8 and system WinMetadata.
exit /b 1
