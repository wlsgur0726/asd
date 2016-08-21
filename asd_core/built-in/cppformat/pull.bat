@echo off
set SRC_PATH=%~dp0..\..\..\..\cppformat

echo F | xcopy /Q /Y "%SRC_PATH%\cppformat.vcxproj" "%~dp0cppformat.vcxproj"
echo D | xcopy /Q /Y "%SRC_PATH%\fmt" "%~dp0fmt"
