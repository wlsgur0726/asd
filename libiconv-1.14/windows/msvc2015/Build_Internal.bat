:: %1 : Win32 or x64
:: %2 : Debug or Release

@echo off

set x64_BinDir=
set x64_LibDir=
set LibFileName=iconv
if "%1"=="x64" set x64_BinDir=x86_amd64\
if "%1"=="x64" set x64_LibDir=amd64\
if "%2"=="Debug" set LibFileName=iconvd

MSBuild.exe "libiconv.sln" /m /t:Clean /p:Platform=%1;Configuration=%2
MSBuild.exe "libiconv.sln" /m /t:Build /p:Platform=%1;Configuration=%2

xcopy /Y %1\%2\%LibFileName%.dll VC\bin\%x64_BinDir%
xcopy /Y %1\%2\%LibFileName%.lib VC\lib\%x64_LibDir%
xcopy /Y %1\%2\%LibFileName%.pdb VC\lib\%x64_LibDir%
xcopy /Y ..\..\include\iconv.h VC\include\