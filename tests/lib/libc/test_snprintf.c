/*
 * Copyright (c) 2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Host-side unit tests for the TF-A reduced snprintf() in lib/libc/snprintf.c.
 * The implementation is built against TF-A headers with its symbols renamed
 * to tf_snprintf/tf_vsnprintf, so it can be compared with the host libc.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int tf_snprintf(char *s, size_t n, const char *fmt, ...);

static int failures;
static int checks;
static jmp_buf panic_env;
static int panicked;

void tf_log(const char *fmt, ...)
{
	(void)fmt;
}

void __assert(const char *file, unsigned int line)
{
	fprintf(stderr, "unexpected assert at %s:%u\n", file, line);
	failures++;
	longjmp(panic_env, 1);
}

void plat_panic_handler(void)
{
	panicked = 1;
	longjmp(panic_env, 1);
}

#define CHECK(cond)							\
	do {								\
		checks++;						\
		if (!(cond)) {						\
			failures++;					\
			fprintf(stderr, "%s:%d: CHECK failed: %s\n",	\
				__FILE__, __LINE__, #cond);		\
		}							\
	} while (0)

/* Format into a large buffer and compare output and return value. */
#define EXPECT_FMT(expected, ...)					\
	do {								\
		char buf[128];						\
		int ret;						\
		memset(buf, 'X', sizeof(buf));				\
		ret = tf_snprintf(buf, sizeof(buf), __VA_ARGS__);	\
		CHECK(strcmp(buf, (expected)) == 0);			\
		CHECK(ret == (int)strlen(expected));			\
		if (strcmp(buf, (expected)) != 0) {			\
			fprintf(stderr, "  got \"%s\", want \"%s\"\n",	\
				buf, (expected));			\
		}							\
	} while (0)

static void test_specifiers(void)
{
	EXPECT_FMT("", "");
	EXPECT_FMT("plain text", "plain text");
	EXPECT_FMT("hello world", "hello %s", "world");
	EXPECT_FMT("0", "%d", 0);
	EXPECT_FMT("42", "%d", 42);
	EXPECT_FMT("-42", "%i", -42);
	EXPECT_FMT("2147483647", "%d", 2147483647);
	EXPECT_FMT("-2147483648", "%d", (int)-2147483647 - 1);
	EXPECT_FMT("4294967295", "%u", 4294967295U);
	EXPECT_FMT("deadbeef", "%x", 0xdeadbeefU);
	EXPECT_FMT("DEADBEEF", "%X", 0xdeadbeefU);
	EXPECT_FMT("0x1234", "%p", (void *)0x1234);
	EXPECT_FMT("0", "%p", (void *)0);
	EXPECT_FMT("a=1 b=ff c=str", "a=%d b=%x c=%s", 1, 0xff, "str");
}

static void test_padding(void)
{
	EXPECT_FMT("00042", "%05d", 42);
	EXPECT_FMT("   42", "%5d", 42);
	EXPECT_FMT("42   |", "%-5d|", 42);
	EXPECT_FMT("000000ff", "%08x", 0xffU);
	EXPECT_FMT("12345", "%3d", 12345);
	EXPECT_FMT("0x00001234", "%010p", (void *)0x1234);
	EXPECT_FMT("0x1234    |", "%-10p|", (void *)0x1234);
}

static void test_truncation(void)
{
	char buf[16];
	int ret;

	/* Return value is the untruncated length; output is NUL terminated. */
	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 6, "%s", "hello world");
	CHECK(ret == 11);
	CHECK(strcmp(buf, "hello") == 0);
	CHECK(buf[6] == 'X');

	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 6, "hello world");
	CHECK(ret == 11);
	CHECK(strcmp(buf, "hello") == 0);
	CHECK(buf[6] == 'X');

	/* Exact fit. */
	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 6, "%d", 12345);
	CHECK(ret == 5);
	CHECK(strcmp(buf, "12345") == 0);

	/* Numbers are truncated like strings, never past the buffer. */
	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 4, "%d", 12345);
	CHECK(ret == 5);
	CHECK(strcmp(buf, "123") == 0);
	CHECK(buf[4] == 'X');

	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 4, "%08x", 0x1U);
	CHECK(ret == 8);
	CHECK(strcmp(buf, "000") == 0);
	CHECK(buf[4] == 'X');

	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 4, "%d", -5000);
	CHECK(ret == 5);
	CHECK(strcmp(buf, "-50") == 0);
	CHECK(buf[4] == 'X');

	/* Right padding is counted and bounded by the buffer size. */
	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 4, "%-10d", 1);
	CHECK(ret == 10);
	CHECK(strcmp(buf, "1  ") == 0);
	CHECK(buf[4] == 'X');

	/* n == 1: only the terminator is written. */
	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 1, "abc%d", 7);
	CHECK(ret == 4);
	CHECK(buf[0] == '\0');
	CHECK(buf[1] == 'X');

	/* n == 0: the buffer is not touched at all. */
	memset(buf, 'X', sizeof(buf));
	ret = tf_snprintf(buf, 0, "abc%d", 7);
	CHECK(ret == 4);
	CHECK(buf[0] == 'X');

	/* Size query with a NULL buffer and n == 0. */
	ret = tf_snprintf(NULL, 0, "%s-%u", "abc", 123U);
	CHECK(ret == 7);
}

static void test_unsupported_specifier_panics(void)
{
	char buf[16];

	panicked = 0;
	if (setjmp(panic_env) == 0) {
		(void)tf_snprintf(buf, sizeof(buf), "%f", 1.0);
	}
	CHECK(panicked == 1);
}

int main(void)
{
	test_specifiers();
	test_padding();
	test_truncation();
	test_unsupported_specifier_panics();

	printf("snprintf: %d checks, %d failures\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
