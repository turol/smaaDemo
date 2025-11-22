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

vpath %.json $(TOPDIR)/$(SPIRV-HEADERS_DIR) $(TOPDIR)/$(d)/source

SPV_GENERATOR:=$(d)/utils/ggt.py


SPV_GENERATED:= \
	build-version.inc \
	core_tables_body.inc \
	core_tables_header.inc \
	generators.inc \
	# empty line


core_tables_body.inc core_tables_header.inc: $(SPV_GENERATOR) $(TOPDIR)/$(SPIRV-HEADERS_DIR)/*.json
	$(PYTHON) $(word 1, $^)                                                                          \
	--core-tables-body-output=core_tables_body.inc                                                   \
	--core-tables-header-output=core_tables_header.inc                                               \
	--spirv-core-grammar=$(TOPDIR)/$(SPIRV-HEADERS_DIR)/spirv.core.grammar.json                                \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.glsl.std.450.grammar.json                                \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.opencl.std.100.grammar.json                              \
	--extinst=CLDEBUG100_,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.opencl.debuginfo.100.grammar.json             \
	--extinst=SHDEBUG100_,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.nonsemantic.shader.debuginfo.100.grammar.json \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.spv-amd-shader-explicit-vertex-parameter.grammar.json    \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.spv-amd-shader-trinary-minmax.grammar.json               \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.spv-amd-gcn-shader.grammar.json                          \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.spv-amd-shader-ballot.grammar.json                       \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.debuginfo.grammar.json                                   \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.nonsemantic.clspvreflection.grammar.json                 \
	--extinst=,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.nonsemantic.vkspreflection.grammar.json                  \
	--extinst=TOSA_,$(TOPDIR)/$(SPIRV-HEADERS_DIR)/extinst.tosa.001000.1.grammar.json                          \
	# empty line


# $(call spvtools_extinst_lang_headers, NAME, GRAMMAR_FILE)
define spvtools_extinst_lang_headers

$1.h: $(d)/utils/generate_language_headers.py $2
	$$(PYTHON) $$(word 1, $$^) --extinst-grammar=$$(word 2, $$^) --extinst-output-path=$$@

SPV_GENERATED:=$$(SPV_GENERATED) $1.h

endef  # spvtools_extinst_lang_headers


$(eval $(call spvtools_extinst_lang_headers,DebugInfo,extinst.debuginfo.grammar.json) )
$(eval $(call spvtools_extinst_lang_headers,OpenCLDebugInfo100,extinst.opencl.debuginfo.100.grammar.json) )
$(eval $(call spvtools_extinst_lang_headers,NonSemanticShaderDebugInfo100,extinst.nonsemantic.shader.debuginfo.100.grammar.json) )



## SPIRV-Tools/utils/generate_registry_tables.py --xml=/home/turo/softaa/SPIRV-Tools/external/spirv-headers/include/spirv/spir-v.xml --generator-output=/home/turo/softaa/SPIRV-Tools/build/generators.inc

build-version.inc: $(d)/utils/update_build_version.py $(d)/CHANGES
	$(PYTHON) $(word 1, $^) $(word 2, $^) $@
	# update_build_version.py doesn't touch the timestamp unless the file actually changes
	touch $@


generators.inc: $(d)/utils/generate_registry_tables.py $(d)/../SPIRV-Headers/include/spirv/spir-v.xml
	$(PYTHON) $(word 1, $^) --xml=$(word 2, $^) --generator-output=$@


$(SRC_spirv-tools:.cpp=$(OBJSUFFIX)): $(SPV_GENERATED)


endif  # INTERNAL_spirv-tools


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
