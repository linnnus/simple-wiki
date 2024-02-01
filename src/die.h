#ifndef DIE_H
#define DIE_H

//
// This module defines various utilities for ending program execution
// abnormally.
//

#include <stdnoreturn.h> // noreturn

#ifdef __GNUC__
#define _DIE_PRINTF_ATTR __attribute__((format(printf, 1, 2)))
#else
#define _DIE_PRINTF_ATTR
#endif

// Exit the program, displaying no extra information.
_DIE_PRINTF_ATTR
noreturn void die(const char *msg, ...);

// Exit the program, displaying the last libgit error.
// It is an error to invoke this if there has been no libgit error.
_DIE_PRINTF_ATTR
noreturn void die_git(const char *msg, ...);

// Exit the program, displaying errno message.
// It is NOT an error to invoke this if errno is 0, just pretty weird.
_DIE_PRINTF_ATTR
noreturn void die_errno(const char *msg, ...);

#endif
