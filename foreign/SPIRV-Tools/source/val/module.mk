sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	basic_block.cpp \
	construct.cpp \
	function.cpp \
	instruction.cpp \
	validate_adjacency.cpp \
	validate_annotation.cpp \
	validate_arithmetics.cpp \
	validate_atomics.cpp \
	validate_barriers.cpp \
	validate_bitwise.cpp \
	validate_builtins.cpp \
	validate_capability.cpp \
	validate_cfg.cpp \
	validate_composites.cpp \
	validate_constants.cpp \
	validate_conversion.cpp \
	validate.cpp \
	validate_datarules.cpp \
	validate_debug.cpp \
	validate_decorations.cpp \
	validate_derivatives.cpp \
	validate_execution_limitations.cpp \
	validate_extensions.cpp \
	validate_function.cpp \
	validate_id.cpp \
	validate_image.cpp \
	validate_instruction.cpp \
	validate_interfaces.cpp \
	validate_layout.cpp \
	validate_literals.cpp \
	validate_logicals.cpp \
	validate_memory.cpp \
	validate_memory_semantics.cpp \
	validate_mode_setting.cpp \
	validate_non_uniform.cpp \
	validate_primitives.cpp \
	validate_scopes.cpp \
	validate_type.cpp \
	validation_state.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
