sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	aggressive_dead_code_elim_pass.cpp \
	analyze_live_input_pass.cpp \
	amd_ext_to_khr.cpp \
	basic_block.cpp \
	block_merge_pass.cpp \
	block_merge_util.cpp \
	build_module.cpp \
	ccp_pass.cpp \
	cfg_cleanup_pass.cpp \
	cfg.cpp \
	code_sink.cpp \
	combine_access_chains.cpp \
	compact_ids_pass.cpp \
	composite.cpp \
	constants.cpp \
	const_folding_rules.cpp \
	convert_to_half_pass.cpp \
	convert_to_sampled_image_pass.cpp \
	copy_prop_arrays.cpp \
	dead_branch_elim_pass.cpp \
	dead_insert_elim_pass.cpp \
	dead_variable_elimination.cpp \
	debug_info_manager.cpp \
	decoration_manager.cpp \
	def_use_manager.cpp \
	desc_sroa.cpp \
	desc_sroa_util.cpp \
	dominator_analysis.cpp \
	dominator_tree.cpp \
	eliminate_dead_constant_pass.cpp \
	eliminate_dead_functions_pass.cpp \
	eliminate_dead_functions_util.cpp \
	eliminate_dead_io_components_pass.cpp \
	eliminate_dead_members_pass.cpp \
	eliminate_dead_output_stores_pass.cpp \
	feature_manager.cpp \
	fix_func_call_arguments.cpp \
	fix_storage_class.cpp \
	flatten_decoration_pass.cpp \
	fold.cpp \
	folding_rules.cpp \
	fold_spec_constant_op_and_composite_pass.cpp \
	freeze_spec_constant_value_pass.cpp \
	function.cpp \
	graphics_robust_access_pass.cpp \
	if_conversion.cpp \
	inline_exhaustive_pass.cpp \
	inline_opaque_pass.cpp \
	inline_pass.cpp \
	inst_bindless_check_pass.cpp \
	inst_buff_addr_check_pass.cpp \
	inst_debug_printf_pass.cpp \
	instruction.cpp \
	instruction_list.cpp \
	instrument_pass.cpp \
	interface_var_sroa.cpp \
	interp_fixup_pass.cpp \
	invocation_interlock_placement_pass.cpp \
	ir_context.cpp \
	ir_loader.cpp \
	licm_pass.cpp \
	liveness.cpp \
	local_access_chain_convert_pass.cpp \
	local_redundancy_elimination.cpp \
	local_single_block_elim_pass.cpp \
	local_single_store_elim_pass.cpp \
	loop_dependence.cpp \
	loop_dependence_helpers.cpp \
	loop_descriptor.cpp \
	loop_fission.cpp \
	loop_fusion.cpp \
	loop_fusion_pass.cpp \
	loop_peeling.cpp \
	loop_unroller.cpp \
	loop_unswitch_pass.cpp \
	loop_utils.cpp \
	mem_pass.cpp \
	merge_return_pass.cpp \
	modify_maximal_reconvergence.cpp \
	module.cpp \
	optimizer.cpp \
	pass.cpp \
	pass_manager.cpp \
	pch_source_opt.cpp \
	private_to_local_pass.cpp \
	propagator.cpp \
	reduce_load_size.cpp \
	redundancy_elimination.cpp \
	register_pressure.cpp \
	relax_float_ops_pass.cpp \
	replace_desc_array_access_using_var_index.cpp \
	remove_dontinline_pass.cpp \
	remove_duplicates_pass.cpp \
	remove_unused_interface_variables_pass.cpp \
	replace_invalid_opc.cpp \
	scalar_analysis.cpp \
	scalar_analysis_simplification.cpp \
	scalar_replacement_pass.cpp \
	set_spec_constant_default_value_pass.cpp \
	simplification_pass.cpp \
	spread_volatile_semantics.cpp \
	ssa_rewrite_pass.cpp \
	strength_reduction_pass.cpp \
	strip_debug_info_pass.cpp \
	strip_nonsemantic_info_pass.cpp \
	struct_cfg_analysis.cpp \
	switch_descriptorset_pass.cpp \
	trim_capabilities_pass.cpp \
	type_manager.cpp \
	types.cpp \
	unify_const_pass.cpp \
	upgrade_memory_model.cpp \
	value_number_table.cpp \
	vector_dce.cpp \
	workaround1209.cpp \
	wrap_opkill.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
