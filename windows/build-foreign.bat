@echo off
SET GENERATOR="Visual Studio 14 2015"
SET GENERATOR64="Visual Studio 14 2015 Win64"
rem SET GENERATOR"Visual Studio 15 2017"
rem SET GENERATOR64="Visual Studio 15 2017 Win64"

cd ../foreign/

cd SPIRV-Tools

mkdir build
cd build
cmake -G%GENERATOR% -DSPIRV-Headers_SOURCE_DIR=%cd%/../../SPIRV-Headers ..
cmake --build .
cmake --build . --config Release
cd ..

mkdir build64
cd build64
cmake -G%GENERATOR64% -DSPIRV-Headers_SOURCE_DIR=%cd%/../../SPIRV-Headers ..
cmake --build .
cmake --build . --config Release
cd ..

cd ..

cd SPIRV-Cross

mkdir build
cd build
cmake -G%GENERATOR% ..
cmake --build .
cmake --build . --config Release
cd ..

mkdir build64
cd build64
cmake -G%GENERATOR64% ..
cmake --build .
cmake --build . --config Release
cd ..

cd ..

cd ../windows
