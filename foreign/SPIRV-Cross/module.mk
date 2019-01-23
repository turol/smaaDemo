sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	spirv_cfg.cpp \
	spirv_cpp.cpp \
	spirv_cross.cpp \
	spirv_cross_parsed_ir.cpp \
	spirv_glsl.cpp \
	spirv_hlsl.cpp \
	spirv_msl.cpp \
	spirv_parser.cpp \
	# empty line


ifeq ($(INTERNAL_spirv-cross),y)

SRC_spirv-cross:=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )

endif  # INTERNAL_spirv-cross


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
