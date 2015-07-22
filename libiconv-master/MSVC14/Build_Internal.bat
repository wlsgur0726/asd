:: %1 : Win32 or x64
:: %2 : Debug or Release

@echo off

set x64_BinDir=
set x64_LibDir=
set Debug_Suffix=
if "%1"=="x64" set x64_BinDir=x86_amd64\
if "%1"=="x64" set x64_LibDir=amd64\
if "%2"=="Debug" set Debug_Suffix=_debug

MSBuild.exe "libiconv.sln" /m /t:Clean /p:Platform=%1;Configuration=%2
MSBuild.exe "libiconv.sln" /m /t:Build /p:Platform=%1;Configuration=%2

xcopy /Y libiconv_dll\%1\%2\libiconv%Debug_Suffix%.dll VC\bin\%x64_BinDir%
xcopy /Y libiconv_dll\%1\%2\libiconv%Debug_Suffix%.lib VC\lib\%x64_LibDir%
xcopy /Y libiconv_dll\%1\%2\libiconv%Debug_Suffix%.pdb VC\lib\%x64_LibDir%
xcopy /Y libiconv_static\%1\%2\libiconv_a%Debug_Suffix%.lib VC\lib\%x64_LibDir%
