:: %1  :  Platform (Win32 or x64)
:: %2  :  Configuration (Debug or Release)
:: %3  :  OutDir

@echo off

echo Copy ICONV DLL
cd "%~dp0\..\libiconv-1.14\windows\msvc2015\%1\%2"
xcopy /Y *.dll "%3"
xcopy /Y *.pdb "%3"
