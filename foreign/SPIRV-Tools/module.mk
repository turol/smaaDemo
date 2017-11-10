sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	source \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	# empty line


ifeq ($(INTERNAL_spirv-tools),y)

SRC_spirv-tools:=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )

endif  # INTERNAL_spirv-tools


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
