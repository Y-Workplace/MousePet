CXX      ?= g++
BUILD    ?= build
PROTO    := $(BUILD)/proto
WL_PROTO := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
PKGS     := wayland-client cairo luajit

CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++26 -Wall -Wextra -MMD -MP -Isrc -I$(PROTO) $(shell pkg-config --cflags $(PKGS))
CFLAGS   ?= -O2
CFLAGS   += -I$(PROTO) $(shell pkg-config --cflags wayland-client)
LDLIBS   := $(shell pkg-config --libs $(PKGS)) -ldl -lm

# name:xml
PROTOCOLS := \
	wlr-layer-shell-unstable-v1:protocols/wlr-layer-shell-unstable-v1.xml \
	xdg-shell:$(WL_PROTO)/stable/xdg-shell/xdg-shell.xml \
	xdg-output-unstable-v1:$(WL_PROTO)/unstable/xdg-output/xdg-output-unstable-v1.xml \
	viewporter:$(WL_PROTO)/stable/viewporter/viewporter.xml \
	fractional-scale-v1:$(WL_PROTO)/staging/fractional-scale/fractional-scale-v1.xml

proto_name = $(word 1,$(subst :, ,$1))
proto_xml  = $(word 2,$(subst :, ,$1))
PROTO_H    := $(foreach p,$(PROTOCOLS),$(PROTO)/$(call proto_name,$p)-client-protocol.h)
PROTO_O    := $(foreach p,$(PROTOCOLS),$(PROTO)/$(call proto_name,$p)-protocol.o)

ACCESSORY_SRC := $(wildcard src/accessories/*.cpp)
CORE_SRC      := src/Sim.cpp src/Settings.cpp $(ACCESSORY_SRC)

APP_SRC  := $(CORE_SRC) src/Ipc.cpp src/Overlay.cpp src/main.cpp
APP_OBJ  := $(APP_SRC:src/%.cpp=$(BUILD)/obj/%.o)
CORE_OBJ := $(CORE_SRC:src/%.cpp=$(BUILD)/obj/%.o)

PREFIX   ?= $(HOME)/.local
# Files on this project's NTFS/fuse mount are not executable: run build outputs through the loader.
LOADER   := $(firstword $(wildcard /lib64/ld-linux-x86-64.so.2 /lib/ld-linux-aarch64.so.1))

.DEFAULT_GOAL := all
all: $(BUILD)/mousepet plugin

define PROTO_RULES
$(PROTO)/$(call proto_name,$1)-client-protocol.h: $(call proto_xml,$1)
	@mkdir -p $(PROTO)
	wayland-scanner client-header $$< $$@
$(PROTO)/$(call proto_name,$1)-protocol.c: $(call proto_xml,$1)
	@mkdir -p $(PROTO)
	wayland-scanner private-code $$< $$@
endef
$(foreach p,$(PROTOCOLS),$(eval $(call PROTO_RULES,$p)))

$(PROTO)/%-protocol.o: $(PROTO)/%-protocol.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/%.o: src/%.cpp | $(PROTO_H)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/mousepet: $(APP_OBJ) $(PROTO_O)
	$(CXX) $^ -o $@ $(LDLIBS)
	@chmod +x $@ 2>/dev/null || true

PLUGIN_CXXFLAGS := -Wall -Wextra -fPIC -std=c++26 -O2 \
	$(shell pkg-config --cflags hyprland pixman-1 libdrm 2>/dev/null | sed 's#-I\([^ ]*/hyprland\)\($$\| \)#-I\1 -I\1/src #g')

$(BUILD)/mousepet-hyprland.so: plugin/mousepet_plugin.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(PLUGIN_CXXFLAGS) -shared $< -o $@

plugin: $(BUILD)/mousepet-hyprland.so

$(BUILD)/sim_tests: tests/sim_tests.cpp $(CORE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/bridge_tests: tests/bridge_tests.cpp $(BUILD)/obj/Ipc.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

test: $(BUILD)/sim_tests $(BUILD)/bridge_tests $(BUILD)/mousepet
	$(LOADER) ./$(BUILD)/sim_tests
	$(LOADER) ./$(BUILD)/bridge_tests
	python3 tests/snapshot_test.py --binary $(BUILD)/mousepet --out $(BUILD)/snapshots

# Runs MousePet inside a private nested Hyprland (never touches the running session).
sandbox-test: $(BUILD)/mousepet
	python3 tests/sandbox_test.py --binary $(BUILD)/mousepet

install: $(BUILD)/mousepet $(BUILD)/mousepet-hyprland.so
	install -Dm755 $(BUILD)/mousepet $(DESTDIR)$(PREFIX)/bin/mousepet
	ln -sf mousepet $(DESTDIR)$(PREFIX)/bin/cursorpet
	install -Dm755 $(BUILD)/mousepet-hyprland.so $(DESTDIR)$(PREFIX)/lib/mousepet-hyprland.so
	install -Dm644 systemd/mousepet.service $(DESTDIR)$(HOME)/.config/systemd/user/mousepet.service

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mousepet $(DESTDIR)$(PREFIX)/bin/cursorpet
	rm -f $(DESTDIR)$(PREFIX)/lib/mousepet-hyprland.so
	rm -f $(DESTDIR)$(HOME)/.config/systemd/user/mousepet.service

run: $(BUILD)/mousepet
	$(LOADER) ./$(BUILD)/mousepet run

clean:
	rm -rf $(BUILD)

.PHONY: all test sandbox-test install uninstall run clean plugin
-include $(APP_OBJ:.o=.d)
