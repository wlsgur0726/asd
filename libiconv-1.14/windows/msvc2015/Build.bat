@echo off

:: VS2015
set VisualStudioVersion=14.0

:: 이 배치파일이 위치한 경로로 이동
set SlnPath=%~dp0
cd "%SlnPath%"

call Build_Internal.bat Win32 Debug
call Build_Internal.bat Win32 Release
call Build_Internal.bat x64 Debug
call Build_Internal.bat x64 Release

xcopy /Y ..\source\include\iconv.h VC\include\
