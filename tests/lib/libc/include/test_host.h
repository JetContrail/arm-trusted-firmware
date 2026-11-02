/*
 * Copyright (c) 2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Force-included when building TF-A libc sources for host-side unit tests.
 * It replaces <plat/common/platform.h> and skips the console_t layout
 * assertions, which depend on platform definitions and AArch64 register
 * sizes that do not apply to a host build.
 */

#ifndef TEST_HOST_H
#define TEST_HOST_H

#include <cdefs.h>

#define PLATFORM_H
#define CONSOLE_ASSERTIONS_H

void __dead2 plat_panic_handler(void);

#endif /* TEST_HOST_H */
