@echo off
title Input Leap - Servidor Windows (Aprimorado)
color 0A
cls
echo ========================================================
echo   INPUT LEAP - SERVIDOR WINDOWS (COMPILADO COM SUCESSO)
echo ========================================================
echo.
echo [*] Melhorias ativas no executavel:
echo     - PNG Clipboard Nativo (Win / Mac)
echo     - Traducao Inteligente de Atalhos (Ctrl - Cmd)
echo     - Teclado ABNT2 corrigido
echo     - Latencia zero (Stream Flush imediato)
echo.
echo IP Local do seu Windows para colocar no Mac:
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /c:"IPv4"') do (
    echo    ==^> %%a
)
echo.
echo Porta: 24800
echo.
echo ========================================================
echo Servidor em execucao! Aguardando o Mac conectar...
echo (Mantenha esta janela aberta enquanto estiver usando)
echo ========================================================
echo.
"%~dp0build\bin\Release\input-leaps.exe" -f --config "%~dp0inputleap.conf" --address 0.0.0.0:24800 --disable-crypto --debug INFO
if errorlevel 1 (
    echo.
    echo O servidor foi encerrado.
    pause
)
