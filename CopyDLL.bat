:: %1  :  Platform (Win32 or x64)
:: %2  :  Configuration (Debug or Release)
:: %3  :  OutDir

@echo off
mkdir "%3"

echo Copy ICONV DLL
set ICONV_PATH=%~dp0..\libiconv\windows\msvc2017\%1\%2
xcopy /Q /Y "%ICONV_PATH%\*.dll" "%3"
xcopy /Q /Y "%ICONV_PATH%\*.pdb" "%3"

echo Copy asd_redis DLL
set ASD_REDIS_PATH=%~dp0asd_redis\%1\%2
xcopy /Q /Y "%ASD_REDIS_PATH%\*.dll" "%3"
xcopy /Q /Y "%ASD_REDIS_PATH%\*.pdb" "%3"

