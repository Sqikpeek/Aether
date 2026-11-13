@echo off
chcp 65001 >nul
cd /d "%~dp0"
if exist "%SystemRoot%\System32\wscript.exe" (
    start "" wscript.exe //B "%~dp0Aether.vbs"
) else (
    start /B node dist\index.js
)
exit
