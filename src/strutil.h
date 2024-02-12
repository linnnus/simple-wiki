#ifndef STRUTIL_H
#define STRUTIL_H

//
// Defines various utilities for working with strings.
//

#include "arena.h"   // struct arena
#include <stdbool.h> // bool
#include <stdarg.h>  // va_list

// Like asprintf except the allocation is made inside the given arena.
// Panics on allocation failure.
int aprintf(struct arena *a, char **out, const char *fmt, ...);

// Same as aprintf, except takes a varargs list.
int vaprintf(struct arena *a, char **out, const char *fmt, va_list args);

// Join the two paths with a directory separator.
// Result is allocated in arena.
char *joinpath(struct arena *a, const char *path_a, const char *path_b);

// Returns boolean indicating if `haystack` ends with `needle`.
bool endswith(const char *haystack, const char *needle);

// Replaces the last occurence of `suffix` with `with`.
// Does NOT check that `orig` ends with `suffix`.
// Result is allocated in arena.
char *replace_suffix(struct arena *a, const char *orig, const char *suffix, const char *with);

// Replace all occurences of `rep` in `orig` with `with`.
// Result is allocated in arena.
char *replace(struct arena *a, const char *orig, const char *rep, const char *with);

#endif
