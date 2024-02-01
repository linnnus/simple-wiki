#ifndef ARENA_H
#define ARENA_H

//
// This module defines a simple, fixed-size arena allocator.
//

#include <stddef.h> // size_t

struct arena {
	void *root;
	size_t capacity;
	size_t used;
};

// Initialize an arena with the given `capacity`.
// Panics on failure to allocate.
struct arena arena_create(size_t capacity);

// These flags control the behavior of `arena_alloc`.
#define ARENA_NO_ZERO      1
#define ARENA_NO_PANIC     2

// Allocate `size` bytes in `arena`.
// The resulting memory is zeroed unless ARENA_NO_ZERO is passed.
// Unless ARENA_NO_PANIC is specified, the resulting pointer is always valid.
void *arena_alloc(struct arena *arena, size_t size, size_t alignment, unsigned flags);

// Free the memory associated with the arena using the underlying allocator.
void arena_destroy(struct arena *arena);

//
// The `new` macro makes the basic allocation case simple. It uses a bit of
// preprocessor magic to simulate default argument values.
//

#define new(...) newx(__VA_ARGS__,new4,new3,new2)(__VA_ARGS__)
#define newx(a1, a2, a3, a4, a5, ...) a5
#define new2(arena, typ)                  (typ *)arena_alloc(arena,         sizeof(typ), _Alignof(typ), 0)
#define new3(arena, typ, count)           (typ *)arena_alloc(arena, count * sizeof(typ), _Alignof(typ), 0)
#define new4(arena, typ, count, flags)    (typ *)arena_alloc(arena, count * sizeof(typ), _Alignof(typ), flags)

#endif
