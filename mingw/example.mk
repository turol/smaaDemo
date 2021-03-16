# example of local configuration
# copy to local.mk


# location of source
TOPDIR:=..


LTO:=n
ASAN:=n
TSAN:=n
UBSAN:=n

RENDERER:=vulkan


INTERNAL_glslang:=y
LDLIBS_glslang:=

INTERNAL_spirv-cross:=y
LDLIBS_spirv-cross:=

INTERNAL_spirv-headers:=y

INTERNAL_spirv-tools:=y
LDLIBS_spirv-tools:=


# compiler options etc
CC:=i686-w64-mingw32-gcc
CXX:=i686-w64-mingw32-g++
PYTHON:=/usr/bin/python

WIN32:=y


CFLAGS:=-gstabs -mwindows
CFLAGS+=-Wall -Wextra -Wshadow -Werror
CFLAGS+=-I.
CFLAGS+=-D_WIN32_WINNT=0x0600
OPTFLAGS:=-O
OPTFLAGS+=-ffast-math


# lazy assignment because CFLAGS is changed later
CXXFLAGS=$(CFLAGS)
CXXFLAGS+=-std=gnu++14


LDFLAGS:=-mwindows
LDFLAGS+=-gstabs
LDFLAGS+=-L.
LDFLAGS+=-static-libstdc++ -static-libgcc
LDLIBS:=-lSDL2main -lSDL2 -limm32
LDLIBS_opengl:=-lopengl32
LDLIBS_vulkan:=vulkan-1.lib


LTOCFLAGS:=-flto
LTOLDFLAGS:=-flto


OBJSUFFIX:=.o
EXESUFFIX:=.exe
