CXX ?= g++
WINDRES ?= x86_64-w64-mingw32-windres

CXXFLAGS ?= -std=c++20 -O2 -pipe -Iinclude
LDFLAGS ?= -s -static -mwindows -Wl,--no-insert-timestamp
LDLIBS ?= -lws2_32 -luser32 -lshell32

WINDRESFLAGS ?= -Iinclude

WEB_PAGE_GENERATOR := webui/scripts/embed-html.mjs
WEB_PAGE_CPP := build/generated/web_page.cpp
WEB_PAGE_OBJECT := build/generated/web_page.o
WEB_PAGE_SOURCES := $(wildcard webui/*.html webui/src/*.js webui/src/*.css) webui/package.json webui/package-lock.json

RESOURCE_SCRIPT := src/res/rinp.rc
RESOURCE_OBJECT := build/res/rinp_resource.o
RESOURCE_SOURCES := $(RESOURCE_SCRIPT) src/res/resource.h icon/f.ico

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst %.cpp,build/%.o,$(SOURCES))
OBJECTS += $(WEB_PAGE_OBJECT)
OBJECTS += $(RESOURCE_OBJECT)
DEPENDENCIES := $(OBJECTS:.o=.d)
TARGET := dist/rinp.exe

ifeq ($(OS),Windows_NT)
MKDIR_P = if not exist "$(@D)" mkdir "$(@D)"
else
MKDIR_P = mkdir -p "$(@D)"
endif

.PHONY: all clean

all: $(TARGET)

$(WEB_PAGE_CPP): $(WEB_PAGE_GENERATOR) $(WEB_PAGE_SOURCES)
	npm --prefix webui run build
	node $(WEB_PAGE_GENERATOR)

$(WEB_PAGE_OBJECT): $(WEB_PAGE_CPP) include/web_page.h
	$(MKDIR_P)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(RESOURCE_OBJECT): $(RESOURCE_SOURCES)
	$(MKDIR_P)
	$(WINDRES) $(WINDRESFLAGS) -O coff -o $@ $<

$(TARGET): $(OBJECTS)
	$(MKDIR_P)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: %.cpp
	$(MKDIR_P)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDENCIES)

clean:
	$(RM) $(TARGET) $(OBJECTS) $(DEPENDENCIES) $(WEB_PAGE_CPP)