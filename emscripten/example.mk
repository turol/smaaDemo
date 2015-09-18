# example of local configuration
# copy to local.mk


# location of source
TOPDIR:=..


LTO:=n
ASAN:=n
TSAN:=n
UBSAN:=n


USE_GLEW:=n
PRELOAD_FILES:=fxaa3_11.h smaa.h utils.h


# compiler options etc
CC:=emcc
CXX:=em++
CFLAGS:=-g -Wall -Wextra -Werror -Wshadow
CFLAGS+=-Wno-unused-local-typedefs
OPTFLAGS:=-O3


# lazy assignment because CFLAGS is changed later
CXXFLAGS=$(CFLAGS)
CXXFLAGS+=-std=c++11


LDFLAGS:=-g
LDFLAGS+=-s TOTAL_MEMORY=67108864
LDFLAGS+=-s OUTLINING_LIMIT=5000
LDLIBS:=


LTOCFLAGS:=--llvm-lto 3
LTOLDFLAGS:=--llvm-lto 3


OBJSUFFIX:=.o
EXESUFFIX:=.js
