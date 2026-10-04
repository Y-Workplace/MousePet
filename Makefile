CXX      ?= g++
BUILD    ?= build
PROTO    := $(BUILD)/proto
WL_PROTO := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
PKGS     := wayland-client cairo

CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++20 -Wall -Wextra -MMD -MP -Isrc -I$(PROTO) $(shell pkg-config --cflags $(PKGS))
CFLAGS   ?= -O2
CFLAGS   += -I$(PROTO) $(shell pkg-config --cflags wayland-client)
LDLIBS   := $(shell pkg-config --libs $(PKGS)) -lm

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

CORE_SRC := src/Sim.cpp src/Accessories.cpp src/Settings.cpp
APP_SRC  := $(CORE_SRC) src/Ipc.cpp src/Overlay.cpp src/main.cpp
APP_OBJ  := $(APP_SRC:src/%.cpp=$(BUILD)/obj/%.o)
CORE_OBJ := $(CORE_SRC:src/%.cpp=$(BUILD)/obj/%.o)

PREFIX   ?= $(HOME)/.local
# Files on this project's NTFS/fuse mount are not executable: run build outputs through the loader.
LOADER   := $(firstword $(wildcard /lib64/ld-linux-x86-64.so.2 /lib/ld-linux-aarch64.so.1))

.DEFAULT_GOAL := all
all: $(BUILD)/mousepet

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

$(BUILD)/sim_tests: tests/sim_tests.cpp $(CORE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

test: $(BUILD)/sim_tests $(BUILD)/mousepet
	$(LOADER) ./$(BUILD)/sim_tests
	python3 tests/snapshot_test.py --binary $(BUILD)/mousepet --out $(BUILD)/snapshots

# Runs MousePet inside a private nested Hyprland (never touches the running session).
sandbox-test: $(BUILD)/mousepet
	python3 tests/sandbox_test.py --binary $(BUILD)/mousepet

install: $(BUILD)/mousepet
	install -Dm755 $(BUILD)/mousepet $(DESTDIR)$(PREFIX)/bin/mousepet
	install -Dm644 systemd/mousepet.service $(DESTDIR)$(HOME)/.config/systemd/user/mousepet.service

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mousepet $(DESTDIR)$(HOME)/.config/systemd/user/mousepet.service

run: $(BUILD)/mousepet
	$(LOADER) ./$(BUILD)/mousepet run

clean:
	rm -rf $(BUILD)

.PHONY: all test sandbox-test install uninstall run clean
-include $(APP_OBJ:.o=.d)
