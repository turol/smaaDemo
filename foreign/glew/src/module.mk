sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	glew.c \
	# empty line


SRC_glew:=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
