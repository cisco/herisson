@echo ****************************
@echo *Building IP2VF for windows*
@echo ****************************

setlocal
set "source_directory=%cd%"
@echo current directory is: %source_directory%

echo configuring visual studio build environemnt
:return back to build directory
cd %source_directory%
cd ..
md build
cd build
cmake -G "Visual Studio 15 2017 Win64" ..\ip2vf\ || exit /b 666
cmake --build . -- /maxcpucount:8 /p:Configuration=Release || cd ..\ip2vf : exit /b 666
:return to original directory where the build was run
cd %source_directory%
endlocal 
