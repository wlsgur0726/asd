:: %1  :  Platform (Win32 or x64)
:: %2  :  Configuration (Debug or Release)
:: %3  :  OutDir

@echo off

echo Copy ICONV DLL
set ICONV_PATH=%~dp0\..\libiconv\windows\msvc2017\%1\%2
mkdir "%3"
xcopy /Q /Y "%ICONV_PATH%\*.dll" "%3"
xcopy /Q /Y "%ICONV_PATH%\*.pdb" "%3"
