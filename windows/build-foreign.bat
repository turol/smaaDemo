@echo off
SET GENERATOR="Visual Studio 16 2019"

cd ../foreign/

cd SPIRV-Tools

mkdir build
cd build
cmake -G%GENERATOR% -A Win32 -DSPIRV-Headers_SOURCE_DIR=%cd:\=/%/../../SPIRV-Headers ..
cmake --build .
cmake --build . --config Release
cd ..

mkdir build64
cd build64
cmake -G%GENERATOR% -A x64 -DSPIRV-Headers_SOURCE_DIR=%cd:\=/%/../../SPIRV-Headers ..
cmake --build .
cmake --build . --config Release
cd ..

cd ..

cd SPIRV-Cross

mkdir build
cd build
cmake -G%GENERATOR%  -A Win32 ..
cmake --build .
cmake --build . --config Release
cd ..

mkdir build64
cd build64
cmake -G%GENERATOR% -A x64 ..
cmake --build .
cmake --build . --config Release
cd ..

cd ..

cd ../windows
