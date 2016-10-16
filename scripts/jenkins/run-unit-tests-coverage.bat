@echo off

mkdir Coverage >nul
del /Q "Coverage\*.*"

"C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" --sources src\ch --working_dir "bin\Testing Debug" --export_type=cobertura:"Coverage\ch.exe.Coverage.xml" -- "bin\Testing Debug\ch64.exe" || exit /b 1
"C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" --sources src\ictranslate --working_dir "bin\Testing Debug" --export_type=cobertura:"Coverage\ictranslate64.exe.Coverage.xml" -- "bin\Testing Debug\ictranslate64.exe" || exit /b 1

"C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" --sources src\chext --working_dir "bin\Testing Debug" --export_type=cobertura:"Coverage\chext64.dll.Coverage.xml" -- tools\test_runner64.exe "chext64.dll" || exit /b 1

"C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" --sources src\libchcore --working_dir "bin\Testing Debug" --export_type=cobertura:"Coverage\libchcore64ud.dll.Coverage.xml" -- tools\test_runner64.exe "libchcore64ud.dll" || exit /b 1
"C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" --sources src\libictranslate --working_dir "bin\Testing Debug" --export_type=cobertura:"Coverage\libictranslate64ud.dll.Coverage.xml" -- tools\test_runner64.exe "libictranslate64ud.dll" || exit /b 1
"C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe" --sources src\liblogger --working_dir "bin\Testing Debug" --export_type=cobertura:"Coverage\liblogger64ud.dll.Coverage.xml" -- tools\test_runner64.exe "liblogger64ud.dll" || exit /b 1

exit /b 0
