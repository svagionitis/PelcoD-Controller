@echo off
setlocal EnableDelayedExpansion

REM =====================================================================
REM  Pelco-D Controller & Sightline SLA Windows Build Launcher
REM  Dispatches to scripts\Build.ps1 with all supplied arguments
REM =====================================================================

powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%~dp0scripts\Build.ps1" %*
exit /b %ERRORLEVEL%
