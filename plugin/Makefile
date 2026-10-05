PLUGIN_NAME = mousepet-hyprland
CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -fPIC -std=c++26 -O2 \
	$(shell pkg-config --cflags hyprland pixman-1 libdrm 2>/dev/null | sed 's#-I\([^ ]*/hyprland\)\($$\| \)#-I\1 -I\1/src #g')
LDFLAGS ?= -shared

OUTPUT = ../build/$(PLUGIN_NAME).so

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): mousepet_plugin.cpp ../src/SharedBridge.hpp
	@mkdir -p ../build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f $(OUTPUT)
