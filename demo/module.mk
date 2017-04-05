sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


FILES:= \
	# empty line


smaaDemo_MODULES:=
smaaDemo_SRC:=$(foreach f, OpenGLRenderer.cpp smaaDemo.cpp Utils.cpp, $(dir)/$(f))



ifeq ($(RENDERER),opengl)

smaaDemo_MODULES+=glew
CFLAGS+=-DRENDERER_OPENGL -DGLEW_STATIC -DGLEW_NO_GLU

else ifeq ($(RENDERER),null)

CFLAGS+=-DRENDERER_NULL

else ifeq ($(RENDERER),vulkan)

CFLAGS+=-DRENDERER_VULKAN

else

$(error "Unknown render")

endif  # RENDERER



PROGRAMS+= \
	smaaDemo \
	# empty line

SRC_$(d):=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
