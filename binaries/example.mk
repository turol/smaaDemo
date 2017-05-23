# example of local configuration
# copy to local.mk


# location of source
TOPDIR:=..


LTO:=n
ASAN:=n
TSAN:=n
UBSAN:=n

RENDERER:=opengl


# compiler options etc
CXX:=g++
CC:=gcc
CFLAGS:=-g -Wall -Wextra -Werror -Wshadow
CFLAGS+=-Wno-unused-local-typedefs
CFLAGS+=$(shell sdl2-config --cflags)
OPTFLAGS:=-O
OPTFLAGS+=-ffast-math
OPTFLAGS+=-fdata-sections -ffunction-sections


# lazy assignment because CFLAGS is changed later
CXXFLAGS=$(CFLAGS)
CXXFLAGS+=-std=c++11


LDFLAGS:=-g -Wl,-rpath,. -Wl,-rpath,/usr/local/lib:./lib32
LDFLAGS+=-Wl,--gc-sections
# enable this if you're using gold linker
#LFDLAGS+=-Wl,--icf=all
LDLIBS:=
LDLIBS_sdl2:=$(shell sdl2-config --libs)
LDLIBS_opengl:=-lGL
LDLIBS_vulkan:=-lvulkan -lX11-xcb
LDLIBS_shaderc:=-L/usr/local/lib -lshaderc_combined -lpthread

LTOCFLAGS:=-flto -fuse-linker-plugin -fno-fat-lto-objects
LTOLDFLAGS:=-flto -fuse-linker-plugin


OBJSUFFIX:=.o
EXESUFFIX:=-bin
