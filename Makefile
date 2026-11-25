CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror
BUILD_DIR ?= build

.PHONY: all clean check install uninstall

all: $(BUILD_DIR)/z440-hwmio

$(BUILD_DIR)/z440-hwmio: src/z440_hwmio.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

check:
	./tests/static-checks.sh

install:
	sudo ./scripts/install.sh

uninstall:
	sudo ./scripts/uninstall.sh

clean:
	rm -rf -- build
