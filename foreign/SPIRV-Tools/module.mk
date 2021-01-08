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


SPV_GENERATED:= \
	build-version.inc \
	core.insts-unified1.inc \
	debuginfo.insts.inc \
	DebugInfo.h \
	enum_string_mapping.inc \
	extension_enum.inc \
	generators.inc \
	glsl.std.450.insts.inc \
	nonsemantic.clspvreflection.insts.inc \
	opencl.std.insts.inc \
	opencl.debuginfo.100.insts.inc \
	OpenCLDebugInfo100.h \
	operand.kinds-unified1.inc \
	spv-amd-gcn-shader.insts.inc \
	spv-amd-shader-ballot.insts.inc \
	spv-amd-shader-explicit-vertex-parameter.insts.inc \
	spv-amd-shader-trinary-minmax.insts.inc \
	# empty line


build-version.inc: $(d)/utils/update_build_version.py $(d)/CHANGES
	$(PYTHON) $(word 1, $^) $(dir $(word 2, $^)) $@
	# update_build_version.py doesn't touch the timestamp unless the file actually changes
	touch $@


core.insts-unified1.inc operand.kinds-unified1.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --spirv-core-grammar=$(word 2, $^) --extinst-debuginfo-grammar=$(word 3, $^) --extinst-cldebuginfo100-grammar=$(word 4, $^) --core-insts-output=core.insts-unified1.inc --operand-kinds-output=operand.kinds-unified1.inc


debuginfo.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@


DebugInfo.h: $(d)/utils/generate_language_headers.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-grammar=$(word 2, $^) --extinst-output-path=$@


OpenCLDebugInfo100.h: $(d)/utils/generate_language_headers.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-grammar=$(word 2, $^) --extinst-output-path=$@


enum_string_mapping.inc extension_enum.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --spirv-core-grammar=$(word 2, $^) --extinst-debuginfo-grammar=$(word 3, $^) --extinst-cldebuginfo100-grammar=$(word 4, $^) --extension-enum-output=extension_enum.inc --enum-string-mapping-output=enum_string_mapping.inc


generators.inc: $(d)/utils/generate_registry_tables.py $(d)/../SPIRV-Headers/include/spirv/spir-v.xml
	$(PYTHON) $(word 1, $^) --xml=$(word 2, $^) --generator-output=$@


glsl.std.450.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.glsl.std.450.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-glsl-grammar=$(word 2, $^) --glsl-insts-output=$@


nonsemantic.clspvreflection.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.nonsemantic.clspvreflection.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@ --vendor-operand-kind-prefix=

opencl.debuginfo.100.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@ --vendor-operand-kind-prefix=CLDEBUG100_

opencl.std.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.std.100.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-opencl-grammar=$(word 2, $^) --opencl-insts-output=$@

spv-amd-gcn-shader.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.spv-amd-gcn-shader.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@


spv-amd-shader-ballot.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.spv-amd-shader-ballot.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@


spv-amd-shader-explicit-vertex-parameter.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.spv-amd-shader-explicit-vertex-parameter.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@


spv-amd-shader-trinary-minmax.insts.inc: $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.spv-amd-shader-trinary-minmax.grammar.json
	$(PYTHON) $(word 1, $^) --extinst-vendor-grammar=$(word 2, $^) --vendor-insts-output=$@


$(SRC_spirv-tools:.cpp=$(OBJSUFFIX)): $(SPV_GENERATED)


endif  # INTERNAL_spirv-tools


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
