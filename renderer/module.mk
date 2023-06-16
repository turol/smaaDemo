sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


FILES:= \
	NullRenderer.cpp \
	OpenGLRenderer.cpp \
	RendererCommon.cpp \
	VulkanMemoryAllocator.cpp \
	VulkanRenderer.cpp \
	# empty line


DEPENDS_renderer:=glslang sdl2 spirv-cross spirv-tools utils xxHash
renderer_SRC:=$(foreach f, $(FILES), $(dir)/$(f))


ifeq ($(RENDERER),opengl)

CFLAGS+=-DRENDERER_OPENGL

ifeq ($(GL_LOADER),glew)
DEPENDS_renderer+=glew
CFLAGS+=-DUSE_GLEW -DGLEW_STATIC -DGLEW_NO_GLU

else ifeq ($(GL_LOADER),epoxy)

DEPENDS_renderer+=epoxy
CFLAGS+=-DUSE_EPOXY

endif  # GL_LOADER

else ifeq ($(RENDERER),null)

CFLAGS+=-DRENDERER_NULL

else ifeq ($(RENDERER),vulkan)

DEPENDS_renderer+=vulkan
CFLAGS+=-DRENDERER_VULKAN

else  # RENDERER

$(error "Unknown render")

endif  # RENDERER


SRC_$(d):=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
