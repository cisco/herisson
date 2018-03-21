echo configuring visual studio build environemnt
PATH="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC";%PATH%
call vcvarsall.bat || exit /b 666

echo cleaning and building windows version of project
cd IP2VideoFrame
echo removing old files, this is better than clean
rmdir /s /q Debug
rmdir /s /q Release
rmdir /s /q x64

echo building release version
MSBuild IP2VideoFrame.sln /m:4 /t:Clean /property:Configuration=Release /property:Platform=x64 || exit /b 666
MSBuild IP2VideoFrame.sln /m:4 /property:Configuration=Release /property:Platform=x64 || exit /b 666

echo building debug version
MSBuild IP2VideoFrame.sln /m:4 /t:Clean /property:Configuration=Debug /property:Platform=x64 || exit /b 666
MSBuild IP2VideoFrame.sln /m:4 /property:Configuration=Debug /property:Platform=x64 || exit /b 666

cd ..
Echo success!