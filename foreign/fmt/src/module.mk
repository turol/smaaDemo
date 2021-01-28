sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	format.cc \
	os.cc \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES))

SRC_fmt:=$(SRC_$(d))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
