@echo off
title Input Leap - Cliente Windows (Aprimorado)
color 0B
cls
echo ========================================================
echo   INPUT LEAP - CLIENTE WINDOWS (Aprimorado)
echo ========================================================
echo.
set /p SERVER_IP="Digite o IP do Servidor Mac (ex: 192.168.100.xxx): "
echo.
echo Conectando ao servidor %SERVER_IP%:24800...
echo.
"%~dp0build\bin\Release\input-leapc.exe" -f --debug INFO %SERVER_IP%:24800
pause
