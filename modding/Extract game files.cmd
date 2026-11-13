@echo off
rem Emperor Spice - copies the game files out of its archives into mods\_Original (see MODDING.md)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Extract-GameFiles.ps1" %*
pause
