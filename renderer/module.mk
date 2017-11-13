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


renderer_MODULES:=shaderc spirv-cross utils
renderer_SRC:=$(foreach f, $(FILES), $(dir)/$(f))


ifeq ($(RENDERER),opengl)

smaaDemo_MODULES+=glew
CFLAGS+=-DRENDERER_OPENGL -DGLEW_STATIC -DGLEW_NO_GLU

else ifeq ($(RENDERER),null)

CFLAGS+=-DRENDERER_NULL

else ifeq ($(RENDERER),vulkan)

smaaDemo_MODULES+=vulkan
CFLAGS+=-DRENDERER_VULKAN

else

$(error "Unknown render")

endif  # RENDERER


SRC_$(d):=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
