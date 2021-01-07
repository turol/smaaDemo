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


foreign/glslang/build_info.h: $(d)/build_info.py $(d)/build_info.h.tmpl $(d)/CHANGES.md
	$(PYTHON) $(word 1, $^) $(dir $(word 3, $^)) -i $(word 2, $^) -o $@


$(SRC_glslang:.cpp=$(OBJSUFFIX)): foreign/glslang/build_info.h


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
