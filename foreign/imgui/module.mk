sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	imgui.cpp \
	imgui_demo.cpp \
	imgui_draw.cpp \
	imgui_tables.cpp \
	imgui_widgets.cpp \
	# empty line


SRC_imgui:=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
