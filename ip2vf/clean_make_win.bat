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
md windows
cd windows

:For some strange reason this is required when running from cygwin on the build server
: the %_% variable is used to detect if running inside a cygwin ssh process
IF DEFINED _ ECHO fixing cmake and cygwin conflicts
IF DEFINED _ call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall" amd64
echo on

@echo printing all environment variables:
set
@echo ----------------------------------------------------

cmake -G "Visual Studio 15 2017 Win64" ..\..\ip2vf\ || exit /b 666
cmake --build . -- /maxcpucount:8 /p:Configuration=Release || exit /b 666
:return to original directory where the build was run
cd %source_directory%
endlocal 
