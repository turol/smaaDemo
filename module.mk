.PHONY: default all bindirs clean distclean


.SUFFIXES:

#initialize these
PROGRAMS:=
ALLSRC:=
# directories which might contain object files
# used for both clean and bindirs
ALLDIRS:=

default: all


ifeq ($(ASAN),y)

OPTFLAGS+=-fsanitize=address
LDFLAGS_asan?=-fsanitize=address
LDFLAGS+=$(LDFLAGS_asan)

endif


ifeq ($(TSAN),y)

CFLAGS+=-DPIC
OPTFLAGS+=-fsanitize=thread -fpic
LDFLAGS_tsan?=-fsanitize=thread -pie
LDFLAGS+=$(LDFLAGS_tsan)

endif


ifeq ($(UBSAN),y)

OPTFLAGS+=-fsanitize=undefined -fno-sanitize-recover
LDFLAGS+=-fsanitize=undefined -fno-sanitize-recover

endif


ifeq ($(LTO),y)

CFLAGS+=$(LTOCFLAGS)
LDFLAGS+=$(LTOLDFLAGS) $(OPTFLAGS)

endif


CFLAGS+=-DGLEW_STATIC -DGLEW_NO_GLU


LDFLAGS+=$(foreach f, $(PRELOAD_FILES), --preload-file $(TOPDIR)/$(f)@$(f))


CFLAGS+=$(OPTFLAGS)

CFLAGS+=-I$(TOPDIR)
CFLAGS+=-isystem$(TOPDIR)/foreign/glew/include
CFLAGS+=-isystem$(TOPDIR)/foreign/glm
CFLAGS+=-isystem$(TOPDIR)/foreign/stb
CFLAGS+=-isystem$(TOPDIR)/foreign/tclap/include


# (call directory-module, dirname)
define directory-module

# save old
DIRS_$1:=$$(DIRS)

dir:=$1
include $(TOPDIR)/$1/module.mk

ALLDIRS+=$1

ALLSRC+=$$(SRC_$1)

# restore saved
DIRS:=$$(DIRS_$1)

endef

DIRS:= \
	demo \
	foreign \
	# empty line
$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


TARGETS:=$(foreach PROG,$(PROGRAMS),$(EXEPREFIX)$(PROG)$(EXESUFFIX))

all: $(TARGETS)


# check if a directory needs to be created
# can't use targets with the same name as directory because unfortunate
# interaction with VPATH (targets always exists because source dir)
#  $(call missingdir, progname)
define missingdir

ifneq ($$(shell test -d $1 && echo n),n)
MISSINGDIRS+=$1
endif

endef # missingdir

MISSINGDIRS:=
$(eval $(foreach d, $(ALLDIRS), $(call missingdir,$(d)) ))


# create directories which might contain object files
bindirs:
ifneq ($(MISSINGDIRS),)
	mkdir -p $(MISSINGDIRS)
endif


clean:
	rm -f $(TARGETS) $(foreach dir,$(ALLDIRS),$(dir)/*$(OBJSUFFIX))

distclean: clean
	rm -f $(foreach dir,$(ALLDIRS),$(dir)/*.d)
	-rmdir -p --ignore-fail-on-non-empty $(ALLDIRS)


# rules here

%$(OBJSUFFIX): %.cpp | bindirs
	$(CXX) -c -MF $*.d -MP -MMD $(CXXFLAGS) -o $@ $<

%$(OBJSUFFIX): %.c | bindirs
	$(CC) -c -MF $*.d -MP -MMD $(CFLAGS) -o $@ $<


# no warnings in foreign code
foreign/%$(OBJSUFFIX): foreign/%.cpp | bindirs
	$(CXX) -c -MF foreign/$*.d -MP -MMD $(CXXFLAGS) -w -o $@ $<

foreign/%$(OBJSUFFIX): foreign/%.c | bindirs
	$(CC) -c -MF foreign/$*.d -MP -MMD $(CFLAGS) -w -o $@ $<


# $(call resolve-modules, progname)
define resolve-modules

OLD_MODULES:=$$($1_MODULES)

$1_MODULES:=$$(sort $$($1_MODULES) $$(foreach MODULE, $$($1_MODULES), $$(DEPENDS_$$(MODULE))) )

# recurse if changed
ifneq ($$($1_MODULES),$$(OLD_MODULES))
$$(eval $$(call resolve-modules,$1) )
endif

endef


$(eval $(foreach PROGRAM,$(PROGRAMS), $(call resolve-modules,$(PROGRAM)) ) )


# $(call program-target, progname)
define program-target

ALLSRC+=$$(filter %.cpp,$$($1_SRC))
ALLSRC+=$$(filter %.c,$$($1_SRC))

$1_SRC+=$$(foreach module, $$($1_MODULES), $$(SRC_$$(module)))
$1_OBJ:=$$($1_SRC:.cpp=$(OBJSUFFIX))
$1_OBJ:=$$($1_OBJ:.c=$(OBJSUFFIX))
$(EXEPREFIX)$1$(EXESUFFIX): $$($1_OBJ) | bindirs
	$(CXX) $(LDFLAGS) -o $$@ $$^ $$(foreach module, $$($1_MODULES), $$(LDLIBS_$$(module))) $$($1_LIBS) $(LDLIBS)

endef


$(eval $(foreach PROGRAM,$(PROGRAMS), $(call program-target,$(PROGRAM)) ) )


export CXX
export CXXFLAGS
export TOPDIR


compile_commands.json: $(ALLSRC)
	@echo '[' > $@
	@$(TOPDIR)/compile_commands.sh $(filter %.cpp,$(ALLSRC)) >> $@
	@echo ']' >> $@


-include $(foreach FILE,$(ALLSRC),$(patsubst %.c,%.d,$(patsubst %.cpp,%.d,$(FILE))))
