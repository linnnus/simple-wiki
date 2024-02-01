#include "die.h"

#include <assert.h>      // assert
#include <stdio.h>       // fprintf, vfprintf, fputc, stderr
#include <stdlib.h>      // exit, EXIT_FAILURE
#include <stdarg.h>      // va_*
#include <git2.h>        // git_*
#include <string.h>      // strerror
#include <errno.h>       // errno

void die(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	fputc('\n', stderr);

#ifndef NDEBUG
	git_libgit2_shutdown();
#endif
	exit(EXIT_FAILURE);
}

// Die but include the last git error.
void noreturn die_git(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	const git_error *e = git_error_last();
	assert(e != NULL && "die_git called without error");
	fprintf(stderr, ": %s\n", e->message);

#ifndef NDEBUG
	git_libgit2_shutdown();
#endif
	exit(EXIT_FAILURE);
}

// Die but include errno information.
void noreturn die_errno(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	fprintf(stderr, ": %s\n", strerror(errno));

#ifndef NDEBUG
	git_libgit2_shutdown();
#endif
	exit(EXIT_FAILURE);
}
