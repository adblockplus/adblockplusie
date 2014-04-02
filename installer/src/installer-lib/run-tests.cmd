@echo off
rem Test runner for installer library unit test
rem Switches current directory to location of test MSI database file
pushd %~dp0
cd ..\..\build\x64
.\Debug\installer-ca-tests
popd