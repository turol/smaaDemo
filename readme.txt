
	SMAA Demo

This is a small program demonstrating the use of the Subpixel Morphological Antialiasing implementation from https://github.com/iryoku/smaa/.

Building
========

Linux: Go to /binaries and type make. To change build settings copy example.mk to local.mk in the same directory. You only need to include changed lines in local.mk. The build defaults to Vulkan renderer, to use OpenGL set "RENDERER:=opengl" in local.mk.

Windows: There is a Visual Studio 2015 solution in /windows/SMAADemo.sln. You will need cmake, Python3, SDL2 and Vulkan SDK. You also need to build the following libraries from the included sources under /foreign:
SPIRV-Tools.lib
SPIRV-Tools-comp.lib
SPIRV-Tools-opt.lib
spirv-cross-core.lib
spirv-cross-glsl.lib

You can use the included windows/build-foreign.bat file to automatically build the required libraries with cmake.


Usage
=====

Command line options:
"--debug"            - Enable renderer debugging.
"--trace"            - Enable renderer tracing.
"--nocache"          - Don't load shaders from cache.
"-f", "--fullscreen" - Start in fullscreen mode.
"novsync"            - Disable vsync.
"--width <value>"    - Specify window width.
"--height <value>"   - Specify window height.
"<file path> ..."    - Load specified image(s).

Key commands:
A - Toggle antialiasing on/off
C - Re-color cubes
D - Cycle through debug visualizations. Hold SHIFT to cycle in opposite direction.
F - Toggle fullscreen
H - Print help
M - Change antialiasing method (SMAA/FXAA)
Q - Cycle through AA quality levels. Hold SHIFT to cycle in opposite direction.
V - Toggle vsync
LEFT/RIGHT ARROW - Cycle through scenes
SPACE - Toggle camera rotation
ESC - Quit


Third-party software
====================

GLEW (http://glew.sourceforge.net)
GLM (OpenGL Mathematics) (http://glm.g-truc.net/0.9.8)
glslang (https://github.com/KhronosGroup/glslang)
Dear ImGui (https://github.com/ocornut/imgui)
PCG-Random (http://www.pcg-random.org)
SPIRV-Cross (https://github.com/KhronosGroup/SPIRV-Cross)
SPIRV-Headers (https://github.com/KhronosGroup/SPIRV-Headers)
SPIRV-Tools (https://github.com/KhronosGroup/SPIRV-Tools)
stb_image (https://github.com/nothings/stb/)
TCLAP (http://tclap.sourceforge.net)
Vulkan Memory Allocator (https://gpuopen.com/gaming-product/vulkan-memory-allocator/)


Authors
=======

Turo Lamminen turotl@gmail.com
Tuomas Närväinen tuomas.narvainen@alternativegames.net


Copyright and License
=====================

Copyright (c) 2015-2023 Alternative Games Ltd / Turo Lamminen

This code is licensed under the MIT license (see license.txt).

Third-party code under foreign/ is licensed according to their respective licenses.
