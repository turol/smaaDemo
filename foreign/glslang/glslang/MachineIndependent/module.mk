sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	preprocessor \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	attribute.cpp \
	Constant.cpp \
	glslang_tab.cpp \
	InfoSink.cpp \
	Initialize.cpp \
	Intermediate.cpp \
	intermOut.cpp \
	IntermTraverse.cpp \
	iomapper.cpp \
	limits.cpp \
	linkValidate.cpp \
	parseConst.cpp \
	ParseContextBase.cpp \
	ParseHelper.cpp \
	PoolAlloc.cpp \
	propagateNoContraction.cpp \
	reflection.cpp \
	RemoveTree.cpp \
	Scan.cpp \
	ShaderLang.cpp \
	SymbolTable.cpp \
	Versions.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
