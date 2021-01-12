cmake_minimum_required(VERSION 3.7.2)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)

project(SMAADemo VERSION 1.0.0 LANGUAGES C CXX)

INCLUDE(FindVulkan)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

set(SPIRV_HEADERS_SKIP_EXAMPLES ON)
set(SPIRV_HEADERS_SKIP_INSTALL ON)
add_subdirectory(foreign/SPIRV-Headers)

set(SPIRV_WERROR OFF)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV_SKIP_TESTS ON)
add_subdirectory(foreign/SPIRV-Tools)

set(BUILD_SHARED_LIBS OFF)
set(ENABLE_CLANG_BINARIES OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_OPT OFF)
set(ENABLE_GLSLANG_BINARIES OFF)
set(SKIP_GLSLANG_INSTALL ON)
add_subdirectory(foreign/glslang)

set(SHADERC_SKIP_INSTALL ON)
set(SHADERC_ENABLE_TESTS OFF)
set(SHADERC_ENABLE_WERROR_COMPILE OFF)
set(SHADERC_SKIP_TESTS ON)
add_subdirectory(foreign/shaderc)

set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
set(SPIRV_CROSS_ENABLE_HLSL OFF)
set(SPIRV_CROSS_ENABLE_MSL OFF)
set(SPIRV_CROSS_ENABLE_CPP OFF)
set(SPIRV_CROSS_ENABLE_C_API OFF)
add_subdirectory(foreign/SPIRV-Cross)

set(SOURCE
		demo/smaaDemo.cpp
		renderer/NullRenderer.cpp
		renderer/OpenGLRenderer.cpp
		renderer/RendererCommon.cpp
		renderer/VulkanRenderer.cpp
		renderer/VulkanMemoryAllocator.cpp
		utils/Utils.cpp
		foreign/imgui/imgui.cpp
		foreign/imgui/imgui_demo.cpp
		foreign/imgui/imgui_draw.cpp
		foreign/imgui/imgui_widgets.cpp
		foreign/xxHash/xxhash.c
	)

add_executable(smaaDemo ${SOURCE})

target_compile_definitions(smaaDemo PRIVATE
		RENDERER_VULKAN
	)

target_include_directories(smaaDemo PRIVATE
		${PROJECT_SOURCE_DIR}
		${Vulkan_INCLUDE_DIRS}
		foreign
		foreign/glm
		foreign/imgui
		foreign/pcg-cpp/include
		foreign/SPIRV-Cross
		foreign/stb
		foreign/tclap/include
		foreign/vulkanMemoryAllocator/src
	)

target_link_libraries(smaaDemo
		${SDL2_LIBRARIES}
		${Vulkan_LIBRARIES}
		spirv-cross-glsl
		SPVRemapper
		shaderc
	)
