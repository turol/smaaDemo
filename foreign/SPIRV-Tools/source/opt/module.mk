sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	aggressive_dead_code_elim_pass.cpp \
	basic_block.cpp \
	block_merge_pass.cpp \
	build_module.cpp \
	compact_ids_pass.cpp \
	dead_branch_elim_pass.cpp \
	def_use_manager.cpp \
	eliminate_dead_constant_pass.cpp \
	flatten_decoration_pass.cpp \
	fold_spec_constant_op_and_composite_pass.cpp \
	freeze_spec_constant_value_pass.cpp \
	function.cpp \
	inline_pass.cpp \
	insert_extract_elim.cpp \
	instruction.cpp \
	ir_loader.cpp \
	local_access_chain_convert_pass.cpp \
	local_single_block_elim_pass.cpp \
	local_single_store_elim_pass.cpp \
	local_ssa_elim_pass.cpp \
	module.cpp \
	optimizer.cpp \
	pass_manager.cpp \
	set_spec_constant_default_value_pass.cpp \
	strip_debug_info_pass.cpp \
	type_manager.cpp \
	types.cpp \
	unify_const_pass.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
