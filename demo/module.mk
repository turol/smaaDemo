sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


FILES:= \
	# empty line


smaaDemo_MODULES:=glew
smaaDemo_SRC:=$(dir)/smaaDemo.cpp


PROGRAMS+= \
	smaaDemo \
	# empty line

SRC_$(d):=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
