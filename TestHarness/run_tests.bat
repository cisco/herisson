call configure_paths.bat
call mvn package || exit /b 666
java -jar target/TestHarness-1.0-SNAPSHOT.jar || exit /b 666
 