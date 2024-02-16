#include "creole.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE 128

int read_file(const char *file_path, char **out_buffer, size_t *out_length) {
	assert(out_buffer != NULL && *out_buffer == NULL);
	assert(file_path != NULL);

	FILE *fp = fopen(file_path, "r");
	if (fp == NULL) {
		return -1;
	}

	char *buffer = NULL;
	size_t allocated = 0;
	size_t used = 0;
	while (true) {
		// Grow buffer, if needed.
		if (used + CHUNK_SIZE > allocated) {
			// Grow exponentially to guarantee O(log(n)) performance.
			allocated = (allocated == 0) ? CHUNK_SIZE : allocated * 2;

			// Overflow check. Some ANSI C compilers may optimize this away, though.
			if (allocated <= used) {
				free(buffer);
				fclose(fp);
				errno = EOVERFLOW;
				return -1;
			}

			char *temp = realloc(buffer, allocated);
			if (temp == NULL) {
				int old_errno = errno;
				free(buffer); // free() may not set errno
				fclose(fp);   // fclose() may set errno
				errno = old_errno;
				return -1;
			}
			buffer = temp;
		}

		size_t nread = fread(buffer + used, 1, CHUNK_SIZE, fp);
		if (nread == 0) {
			// End-of-file or errnor has occured.
			// FIXME: Should we be checking (nread < CHUNK_SIZE)?
			//        https://stackoverflow.com/a/39322170
			break;
		}
		used += nread;
	}

	if (ferror(fp)) {
		int old_errno = errno;
		free(buffer); // free() may not set errno
		fclose(fp);   // fclose() may set errno
		errno = old_errno;
		return -1;
	}

	// Reallocate to optimal size.
	char *temp = realloc(buffer, used + 1);
	if (temp == NULL) {
		int old_errno = errno;
		free(buffer); // free() may not set errno
		fclose(fp);   // fclose() may set errno
		errno = old_errno;
		return -1;
	}
	buffer = temp;

        // Null-terminate the buffer. Note that buffers may still contain \0,
	// so strlen(buffer) == length may not be true.
        buffer[used] = '\0';

	// Return buffer.
	*out_buffer = buffer;
	if (out_length != NULL) {
		*out_length = used;
	}
	fclose(fp);
	return 0;
}

int main(void) {
	size_t buffer_length = 0;
	char *buffer = NULL;
	if (read_file("/dev/stdin", &buffer, &buffer_length) < 0) {
		perror("Failed to read stdin");
		return EXIT_FAILURE;
	}

	render_creole(stdout, buffer, buffer_length);

        // The lack of return value makes it painfully obvious that we aren't
        // handling errors at all. This represents my half-hearted attempt to fix that.
	if (ferror(stdout)) {
		perror("Failed to write to stdout");
		return EXIT_FAILURE;
	}

        return EXIT_SUCCESS;
}
