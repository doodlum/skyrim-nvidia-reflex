@echo off

RMDIR dist /S /Q

cmake -S . --preset=fo4 --check-stamp-file "build\fo4\CMakeFiles\generate.stamp"
if %ERRORLEVEL% NEQ 0 exit 1
cmake --build build/fo4 --config Release
if %ERRORLEVEL% NEQ 0 exit 1

xcopy "build\fo4\release\*.dll" "dist\F4SE\Plugins\" /I /Y
xcopy "build\fo4\release\*.pdb" "dist\F4SE\Plugins\" /I /Y

xcopy "package\SKSE" "dist\F4SE" /I /Y /E

pause