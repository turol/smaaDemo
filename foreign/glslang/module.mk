sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	glslang \
	OGLCompilersDLL \
	SPIRV \
	StandAlone \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	# empty line


ifeq ($(INTERNAL_glslang),y)

SRC_glslang:=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )

endif  # INTERNAL_glslang


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
