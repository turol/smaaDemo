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
	opencl.std.insts.inc \
	opencl.debuginfo.100.insts.inc \
	OpenCLDebugInfo100.h \
	operand.kinds-unified1.inc \
	spv-amd-gcn-shader.insts.inc \
	spv-amd-shader-ballot.insts.inc \
	spv-amd-shader-explicit-vertex-parameter.insts.inc \
	spv-amd-shader-trinary-minmax.insts.inc \
	# empty line


build-version.inc: $(d)/CHANGES $(d)/utils/update_build_version.py
	$(PYTHON) $(word 2, $^) $(dir $<) $@
	# update_build_version.py doesn't touch the timestamp unless the file actually changes
	touch $@


core.insts-unified1.inc operand.kinds-unified1.inc: $(d)/../SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 3, $^) --spirv-core-grammar=$< --extinst-debuginfo-grammar=$(word 2, $^) --extinst-cldebuginfo100-grammar=$(word 4, $^) --core-insts-output=core.insts-unified1.inc --operand-kinds-output=operand.kinds-unified1.inc


debuginfo.insts.inc: $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-vendor-grammar=$< --vendor-insts-output=$@


DebugInfo.h: $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json $(d)/utils/generate_language_headers.py
	$(PYTHON) $(word 2, $^) --extinst-grammar=$< --extinst-output-path=$@


OpenCLDebugInfo100.h: $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json $(d)/utils/generate_language_headers.py
	$(PYTHON) $(word 2, $^) --extinst-grammar=$< --extinst-output-path=$@


enum_string_mapping.inc extension_enum.inc: $(d)/../SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.debuginfo.grammar.json $(d)/utils/generate_grammar_tables.py $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json
	$(PYTHON) $(word 3, $^) --spirv-core-grammar=$< --extinst-debuginfo-grammar=$(word 2, $^) --extinst-cldebuginfo100-grammar=$(word 4, $^) --extension-enum-output=extension_enum.inc --enum-string-mapping-output=enum_string_mapping.inc


generators.inc: $(d)/../SPIRV-Headers/include/spirv/spir-v.xml $(d)/utils/generate_registry_tables.py
	$(PYTHON) $(word 2, $^) --xml=$< --generator-output=$@


glsl.std.450.insts.inc: $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.glsl.std.450.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-glsl-grammar=$< --glsl-insts-output=$@


opencl.debuginfo.100.insts.inc: $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.debuginfo.100.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-vendor-grammar=$< --vendor-insts-output=$@ --vendor-operand-kind-prefix=CLDEBUG100_

opencl.std.insts.inc: $(d)/../SPIRV-Headers/include/spirv/unified1/extinst.opencl.std.100.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-opencl-grammar=$< --opencl-insts-output=$@

spv-amd-gcn-shader.insts.inc: $(d)/source/extinst.spv-amd-gcn-shader.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-vendor-grammar=$< --vendor-insts-output=$@


spv-amd-shader-ballot.insts.inc: $(d)/source/extinst.spv-amd-shader-ballot.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-vendor-grammar=$< --vendor-insts-output=$@


spv-amd-shader-explicit-vertex-parameter.insts.inc: $(d)/source/extinst.spv-amd-shader-explicit-vertex-parameter.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-vendor-grammar=$< --vendor-insts-output=$@


spv-amd-shader-trinary-minmax.insts.inc: $(d)/source/extinst.spv-amd-shader-trinary-minmax.grammar.json $(d)/utils/generate_grammar_tables.py
	$(PYTHON) $(word 2, $^) --extinst-vendor-grammar=$< --vendor-insts-output=$@


$(SRC_spirv-tools:.cpp=$(OBJSUFFIX)): $(SPV_GENERATED)


endif  # INTERNAL_spirv-tools


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
