cmake_minimum_required(VERSION 3.7.2)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)

project(SMAADemo VERSION 1.0.0 LANGUAGES C CXX)
set(VS_STARTUP_PROJECT SMAADemo)

set(RENDERER "vulkan" CACHE STRING "Renderer")
set_property(CACHE RENDERER PROPERTY STRINGS vulkan opengl null)

if(RENDERER STREQUAL "vulkan")

INCLUDE(FindVulkan)

elseif(RENDERER STREQUAL "opengl")

INCLUDE(FindOpenGL)

endif()

set(CMAKE_CXX_STANDARD 17)
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

set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
set(SPIRV_CROSS_ENABLE_HLSL OFF)
set(SPIRV_CROSS_ENABLE_MSL OFF)
set(SPIRV_CROSS_ENABLE_CPP OFF)
set(SPIRV_CROSS_ENABLE_C_API OFF)
add_subdirectory(foreign/SPIRV-Cross)

add_subdirectory(foreign/fmt)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(foreign/nlohmann_json)

set(SOURCE
		demo/smaaDemo.cpp
		demo/par_shapes.cpp
		renderer/NullRenderer.cpp
		renderer/OpenGLRenderer.cpp
		renderer/RendererCommon.cpp
		renderer/VulkanRenderer.cpp
		renderer/VulkanMemoryAllocator.cpp
		utils/Utils.cpp
		foreign/imgui/imgui.cpp
		foreign/imgui/imgui_demo.cpp
		foreign/imgui/imgui_draw.cpp
		foreign/imgui/imgui_tables.cpp
		foreign/imgui/imgui_widgets.cpp
		foreign/xxHash/xxhash.c
	)


if(RENDERER STREQUAL "opengl")

set(SOURCE ${SOURCE} foreign/glew/src/glew.c)

endif()


add_executable(smaaDemo ${SOURCE})

target_include_directories(smaaDemo PRIVATE
		${PROJECT_SOURCE_DIR}
		foreign
		foreign/glm
		foreign/glslang
		foreign/imgui
		foreign/magic_enum/include
		foreign/par
		foreign/pcg-cpp/include
		foreign/SPIRV-Cross
		foreign/stb
		foreign/tclap/include
		foreign/vulkanMemoryAllocator/include
		foreign/xxHash
	)

target_link_libraries(smaaDemo PRIVATE
		fmt
		glslang
		glslang-default-resource-limits
		nlohmann_json::nlohmann_json
		SPIRV
		SPIRV-Tools-opt
		spirv-cross-glsl
		SPVRemapper
		${SDL2_LIBRARIES}
	)


if(RENDERER STREQUAL "vulkan")

target_compile_definitions(smaaDemo PRIVATE
		RENDERER_VULKAN
	)

target_include_directories(smaaDemo PRIVATE
		${Vulkan_INCLUDE_DIRS}
	)

target_link_libraries(smaaDemo PRIVATE
		${Vulkan_LIBRARIES}
	)

elseif(RENDERER STREQUAL "opengl")

target_compile_definitions(smaaDemo PRIVATE
		RENDERER_OPENGL
		USE_GLEW
		GLEW_STATIC
	)

target_include_directories(smaaDemo PRIVATE
		foreign/glew/include
	)

target_link_libraries(smaaDemo PRIVATE
		${OPENGL_LIBRARIES}
	)

elseif(RENDERER STREQUAL "null")

target_compile_definitions(smaaDemo PRIVATE
		RENDERER_NULL
	)

else()

message(FATAL_ERROR "Bad renderer")

endif()


