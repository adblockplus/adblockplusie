@echo off
rem Test runner for installer library unit test
rem Switches current directory to location of test MSI database file
pushd %~dp0
cd ..\..\build\ia32
.\Debug\installer-ca-tests
popd
