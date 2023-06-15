# example of local configuration
# copy to local.mk


# location of source
TOPDIR:=..


LTO:=n
ASAN:=n
TSAN:=n
UBSAN:=n

RENDERER:=vulkan

USE_GLEW:=y

INTERNAL_glslang:=y
LDLIBS_glslang:=

INTERNAL_spirv-cross:=y
LDLIBS_spirv-cross:=

INTERNAL_spirv-headers:=y

INTERNAL_spirv-tools:=y
LDLIBS_spirv-tools:=


# compiler options etc
CXX:=g++
CC:=gcc
PYTHON:=/usr/bin/python

WIN32:=n


CFLAGS:=-g -Wall -Wextra -Werror -Wshadow
CFLAGS+=-Wno-unused-local-typedefs
CFLAGS+=-Wextra-semi
CFLAGS+=-Wzero-as-null-pointer-constant
CFLAGS+=$(shell sdl2-config --cflags)
OPTFLAGS:=-O
OPTFLAGS+=-ffast-math


# lazy assignment because CFLAGS is changed later
CXXFLAGS=$(CFLAGS)
CXXFLAGS+=-std=c++17


LDFLAGS:=-g -Wl,-rpath,. -Wl,-rpath,/usr/local/lib:./lib32
LDLIBS:=-lpthread
LDLIBS_sdl2:=$(shell sdl2-config --libs)
LDLIBS_opengl:=-lGL
LDLIBS_vulkan:=-lvulkan

LTOCFLAGS:=-flto -fuse-linker-plugin -fno-fat-lto-objects
LTOLDFLAGS:=-flto -fuse-linker-plugin


OBJSUFFIX:=.o
EXESUFFIX:=-bin
