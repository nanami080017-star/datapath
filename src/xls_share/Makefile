ROOT := $(abspath ../..)
YOSYS_CONFIG := $(ROOT)/tools/local/bin/yosys-config
BUILD_DIR ?= $(ROOT)/run/xls_share_build
PLUGIN := $(BUILD_DIR)/xls_share.so

.PHONY: all
all: $(PLUGIN)

$(PLUGIN): pass.cc select_lifting.cc visibility.cc resource_sharing.cc macc_sharing.cc area.cc operations.h visibility.h resource_sharing.h area_model.h Makefile
	mkdir -p "$(BUILD_DIR)"
	"$(YOSYS_CONFIG)" --build "$@" -I"$(ROOT)/tools/local/share/yosys/include" pass.cc select_lifting.cc visibility.cc resource_sharing.cc macc_sharing.cc area.cc
