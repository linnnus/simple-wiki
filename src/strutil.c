#include "strutil.h"

#include "arena.h"        // struct arena, new
#include <assert.h>       // assert
#include <stdarg.h>       // va_*
#include <stdbool.h>      // bool, false
#include <stdio.h>        // vsnprintf
#include <string.h>       // strlen, strncmp
#include <errno.h>        // errno, E* macros

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

char *replace_suffix(struct arena *a, const char *orig, const char *suffix, const char *with)
{
	size_t orig_len = strlen(orig);
	size_t suffix_len = strlen(suffix);
	size_t with_len = strlen(with);

	size_t new_len = orig_len - suffix_len + with_len;
	char *new = new(a, char, new_len + 1);

	memcpy(new, orig, orig_len - suffix_len);
	memcpy(new + orig_len - suffix_len, with, with_len);

	new[new_len] = '\0';

	return new;
}

// Based on <https://stackoverflow.com/a/779960>
char *replace(struct arena *a, const char *orig, const char *rep, const char *with) {
        assert(orig != NULL);
        assert(rep != NULL);

        char *tmp;      // varies

        int len_rep = strlen(rep);
        if (len_rep == 0) {
                errno = EINVAL; // empty rep causes infinite loop during count
                return NULL;
        }

        int len_with;
        if (with == NULL)
                with = "";
        len_with = strlen(with);

        // count the number of replacements needed
        const char *ins; // the next insert point
        int count;       // number of replacements
        ins = orig;
        for (count = 0; (tmp = strstr(ins, rep)) != NULL; ++count) {
                ins = tmp + len_rep;
        }

        char *result;
	tmp = result = new(a, char, strlen(orig) + (len_with - len_rep) * count + 1);

        // first time through the loop, all the variable are set correctly
        // from here on,
        // tmp points to the end of the result string
        // ins points to the next occurrence of rep in orig
        // orig points to the remainder of orig after "end of rep"
        while (count--) {
                ins = strstr(orig, rep);
                int len_front = ins - orig;
                tmp = strncpy(tmp, orig, len_front) + len_front;
                tmp = strcpy(tmp, with) + len_with;
                orig += len_front + len_rep; // move to next "end of rep"
        }
        strcpy(tmp, orig);
        return result;
}
