sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


FILES:= \
	Utils.cpp \
	# empty line


utils_MODULES:=
utils_SRC:=$(foreach f, $(FILES), $(dir)/$(f))


SRC_$(d):=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
