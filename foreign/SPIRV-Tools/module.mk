sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	source \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	# empty line


ifeq ($(INTERNAL_spirv-tools),y)

SRC_spirv-tools:=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )


SPIRV-HEADERS_DIR:=$(d)/../SPIRV-Headers/include/spirv/unified1


SPV_GENERATED:= \
	build-version.inc \
	core.insts-unified1.inc \
	DebugInfo.h \
	enum_string_mapping.inc \
	extension_enum.inc \
	generators.inc \
	glsl.std.450.insts.inc \
	opencl.std.insts.inc \
	OpenCLDebugInfo100.h \
	operand.kinds-unified1.inc \
	# empty line


# $(call spvtools_vendor_tables, VENDOR_TABLE, SHORT_NAME, OPERAND_KIND_PREFIX)
define spvtools_vendor_tables

$1.insts.inc: $(d)/utils/generate_grammar_tables.py $$(SPIRV-HEADERS_DIR)/extinst.$1.grammar.json
	$$(PYTHON) $$(word 1, $$^) --extinst-vendor-grammar=$$(word 2, $$^) --vendor-insts-output=$$@ --vendor-operand-kind-prefix=$3

SPV_GENERATED+=$$(SPV_GENERATED) $1.insts.inc

endef  # spvtools_vendor_tables


$(eval $(call spvtools_vendor_tables,spv-amd-shader-explicit-vertex-parameter,spv-amd-sevp,) )
$(eval $(call spvtools_vendor_tables,spv-amd-shader-trinary-minmax,spv-amd-stm,) )
$(eval $(call spvtools_vendor_tables,spv-amd-gcn-shader,spv-amd-gs,) )
$(eval $(call spvtools_vendor_tables,spv-amd-shader-ballot,spv-amd-sb,) )
$(eval $(call spvtools_vendor_tables,debuginfo,debuginfo,) )
$(eval $(call spvtools_vendor_tables,opencl.debuginfo.100,cldi100,CLDEBUG100_) )
$(eval $(call spvtools_vendor_tables,nonsemantic.clspvreflection,clspvreflection,) )


build-version.inc: $(d)/utils/update_build_version.py $(d)/CHANGES
	$(PYTHON) $(word 1, $^) $(dir $(word 2, $^)) $@
	# update_build_version.py doesn't touch the timestamp unless the file actually changes
	touch $@


core.insts-unified1.inc operand.kinds-unified1.inc: $(d)/utils/generate_grammar_tables.py $(SPIRV-HEADERS_DIR)/spirv.core.grammar.json $(SPIRV-HEADERS_DIR)/extinst.debuginfo.grammar.json $(SPIRV-HEADERS_DIR)/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --spirv-core-grammar=$(word 2, $^) --extinst-debuginfo-grammar=$(word 3, $^) --extinst-cldebuginfo100-grammar=$(word 4, $^) --core-insts-output=core.insts-unified1.inc --operand-kinds-output=operand.kinds-unified1.inc


DebugInfo.h: $(d)/utils/generate_language_headers.py $(SPIRV-HEADERS_DIR)/extinst.debuginfo.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-grammar=$(word 2, $^) --extinst-output-path=$@


OpenCLDebugInfo100.h: $(d)/utils/generate_language_headers.py $(SPIRV-HEADERS_DIR)/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-grammar=$(word 2, $^) --extinst-output-path=$@


enum_string_mapping.inc extension_enum.inc: $(d)/utils/generate_grammar_tables.py $(SPIRV-HEADERS_DIR)/spirv.core.grammar.json $(SPIRV-HEADERS_DIR)/extinst.debuginfo.grammar.json $(SPIRV-HEADERS_DIR)/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --spirv-core-grammar=$(word 2, $^) --extinst-debuginfo-grammar=$(word 3, $^) --extinst-cldebuginfo100-grammar=$(word 4, $^) --extension-enum-output=extension_enum.inc --enum-string-mapping-output=enum_string_mapping.inc


generators.inc: $(d)/utils/generate_registry_tables.py $(d)/../SPIRV-Headers/include/spirv/spir-v.xml
	$(PYTHON) $(word 1, $^) --xml=$(word 2, $^) --generator-output=$@


glsl.std.450.insts.inc: $(d)/utils/generate_grammar_tables.py $(SPIRV-HEADERS_DIR)/extinst.glsl.std.450.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-glsl-grammar=$(word 2, $^) --glsl-insts-output=$@


opencl.std.insts.inc: $(d)/utils/generate_grammar_tables.py $(SPIRV-HEADERS_DIR)/extinst.opencl.std.100.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-opencl-grammar=$(word 2, $^) --opencl-insts-output=$@


$(SRC_spirv-tools:.cpp=$(OBJSUFFIX)): $(SPV_GENERATED)


endif  # INTERNAL_spirv-tools


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
