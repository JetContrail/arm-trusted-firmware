#
# Copyright (c) 2020, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Host-side unit tests for lib/libc. Run with: make -C tests/lib/libc

TF_ROOT		:= ../../..
HOSTCC		?= gcc
BUILD_DIR	:= build

TF_CFLAGS	:= -nostdinc -ffreestanding -fno-builtin -std=gnu99	\
		   -Wall -Werror -DLOG_LEVEL=40 -DENABLE_ASSERTIONS=1	\
		   -Iinclude -include test_host.h		\
		   -I$(TF_ROOT)/include				\
		   -I$(TF_ROOT)/include/lib/libc			\
		   -I$(TF_ROOT)/include/lib/libc/aarch64		\
		   -I$(TF_ROOT)/include/arch/aarch64			\
		   -Dsnprintf=tf_snprintf -Dvsnprintf=tf_vsnprintf
SANITIZE	?= -fsanitize=address,undefined -fno-sanitize-recover=all
HOST_CFLAGS	:= -std=gnu99 -Wall -Werror

.PHONY: all test clean

all: test

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/snprintf.o: $(TF_ROOT)/lib/libc/snprintf.c | $(BUILD_DIR)
	$(HOSTCC) $(TF_CFLAGS) $(SANITIZE) -c $< -o $@

$(BUILD_DIR)/test_snprintf: test_snprintf.c $(BUILD_DIR)/snprintf.o
	$(HOSTCC) $(HOST_CFLAGS) $(SANITIZE) $^ -o $@

test: $(BUILD_DIR)/test_snprintf
	./$(BUILD_DIR)/test_snprintf

clean:
	rm -rf $(BUILD_DIR)
