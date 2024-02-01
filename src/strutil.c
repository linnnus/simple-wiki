#include "strutil.h"

#include "arena.h"        // struct arena, new
#include <assert.h>       // assert
#include <stdarg.h>       // va_*
#include <stdbool.h>      // bool, false
#include <stdio.h>        // vsnprintf
#include <string.h>       // strlen, strncmp

int aprintf(struct arena *a, char **out, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vaprintf(a, out, fmt, ap);
	va_end(ap);
	return ret;
}

int vaprintf(struct arena *a, char **out, const char *fmt, va_list args) {
	// Calculate size.
	va_list tmp;
	va_copy(tmp, args);
	int size = vsnprintf(NULL, 0, fmt, args);
	va_end(tmp);

	// If e.g. the format string was broken, we cannot continue.
	if (size < 0) {
		return -1;
	}

	// Arena allocation cannot fail.
	*out = new(a, char, size + 1);

	int t = vsnprintf(*out, size + 1, fmt, args);
	assert(t == size);

	return size;
}

char *joinpath(struct arena *a, const char *path_a, const char *path_b) {
	char *out;
	int ret = aprintf(a, &out, "%s/%s", path_a, path_b);
	assert(ret > 0 && "should be infallible");
	return out;
}

bool endswith(const char *haystack, const char *needle) {
	assert(haystack != NULL);
	assert(needle != NULL);

	size_t haystack_len = strlen(haystack);
	size_t needle_len = strlen(needle);

	if (needle_len > haystack_len) {
		return false;
	}

	return strncmp(haystack + (haystack_len - needle_len), needle, needle_len) == 0;
}
