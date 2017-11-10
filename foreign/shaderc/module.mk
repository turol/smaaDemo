sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	libshaderc \
	libshaderc_util \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	# empty line


ifeq ($(INTERNAL_shaderc),y)

SRC_shaderc:=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )

endif  # INTERNAL_shaderc


DEPENDS_shaderc:=glslang spirv-tools


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
