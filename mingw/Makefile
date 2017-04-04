## DO NOT TOUCH THIS FILE
# copy example.mk to local.mk and change that

# load defaults
-include example.mk


OLD_TOPDIR:=$(TOPDIR)


# load customizations if exist
-include local.mk


# if topdir changed, re-read config so CFLAGS etc. go right
ifneq ($(OLD_TOPDIR),$(TOPDIR))

include $(TOPDIR)/binaries/example.mk
include local.mk

endif


VPATH=$(TOPDIR)
include $(TOPDIR)/module.mk
