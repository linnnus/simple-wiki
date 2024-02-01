#include "arena.h"

#include <assert.h>         // assert
#include <stdint.h>         // uintptr_t
#include <stdio.h>          // fprintf
#include <stdlib.h>         // abort, malloc
#include <stdnoreturn.h>    // noreturn
#include <string.h>         // memset

static noreturn void arena_panic(const char *reason) {
	fprintf(stderr, "Memory allocation failed: %s", reason);
	abort();
}

struct arena arena_create(size_t capacity) {
	struct arena arena = {
		.root = malloc(capacity),
		.capacity = capacity,
		.used = 0,
	};
	if (arena.root == NULL) {
		arena_panic("cannot allocate system memory");
	}
	return arena;
}

void *arena_alloc(struct arena *arena, size_t size, size_t alignment, unsigned flags) {
	// Alignment must be a power of two.
	// See: https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
	assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
	size_t padding = -(uintptr_t)(arena->root + arena->used) & (alignment - 1);

        // If no more memory is available, we fall back to our error strategy.
	assert(arena->capacity >= arena->used);
        if (arena->used + padding + size > arena->capacity) {
		if (flags & ARENA_NO_PANIC) {
			return NULL;
		} else {
			arena_panic("out of preallocated memory");
		}
	}

	// Reserve memory from arena.
	void *ptr = arena->root + arena->used + padding;
	arena->used += padding + size;

	if (~flags & ARENA_NO_ZERO) {
		memset(ptr, 0, size);
	}

	return ptr;
}

void arena_destroy(struct arena *arena) {
	free(arena->root);
}
