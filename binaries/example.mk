# example of local configuration
# copy to local.mk


# location of source
TOPDIR:=..


LTO:=n
ASAN:=n
TSAN:=n
UBSAN:=n


# compiler options etc
CXX:=g++
CC:=gcc
CFLAGS:=-g -Wall -Wextra -Werror -Wshadow
CFLAGS+=-Wno-unused-local-typedefs
OPTFLAGS:=-O -march=native
OPTFLAGS+=-ffast-math
OPTFLAGS+=-fdata-sections -ffunction-sections


# lazy assignment because CFLAGS is changed later
CXXFLAGS=$(CFLAGS)
CXXFLAGS+=-std=c++11


LDFLAGS:=-g -Wl,-rpath,. -Wl,-rpath,/usr/local/lib:./lib32
LDFLAGS+=-Wl,--gc-sections,--icf=all
LDLIBS:=

LTOCFLAGS:=-flto -fuse-linker-plugin -fno-fat-lto-objects
LTOLDFLAGS:=-flto -fuse-linker-plugin


OBJSUFFIX:=.o
EXESUFFIX:=-bin
