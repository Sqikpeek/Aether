@echo off
chcp 65001 >nul
title Aether Launcher Setup
echo ========================================================
echo               AETHER LAUNCHER INSTALLER                  
echo ========================================================
echo.

set "DEFAULT_DIR=%APPDATA%\AetherLauncher"
set /p "INSTALL_DIR=Choose installation folder [%DEFAULT_DIR%]: "
if "%INSTALL_DIR%"=="" set "INSTALL_DIR=%DEFAULT_DIR%"

echo.
echo Installing Aether Launcher to: %INSTALL_DIR%
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

echo Copying launcher files...
xcopy /E /I /Y "%~dp0*" "%INSTALL_DIR%" >nul

echo Creating desktop shortcut...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut([System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'Aether Launcher.lnk')); $s.TargetPath = '%INSTALL_DIR%\Aether.bat'; $s.WorkingDirectory = '%INSTALL_DIR%'; $s.Save()"

echo.
echo ========================================================
echo            Installation completed successfully!          
echo ========================================================
echo You can now launch Aether from your Desktop shortcut.
timeout /T 3 >nul
exit
