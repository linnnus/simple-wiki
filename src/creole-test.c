#include "creole.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <glob.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
//#include <ftw.h>

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define CYAN    "\x1b[96m"
#define BOLD    "\x1b[39;1m"
#define DIM     "\x1b[39;2m"
#define CLEAR   "\x1b[0m"

#define LENGTH(a) (sizeof(a)/sizeof((a)[0]))

#define CHUNK_SIZE 5

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

// https://stackoverflow.com/a/779960
char *replace(const char *orig, char *rep, char *with) {
	assert(orig != NULL);
	assert(rep != NULL);

	char *tmp;	// varies

	int len_rep = strlen(rep);  // length of rep (the string to remove)
	if (len_rep == 0) {
		errno = EINVAL; // empty rep causes infinite loop during count
		return NULL;
	}

	int len_with; // length of with (the string to replace rep with)
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

	char *result; // the return string
	tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
	if (!result) {
		return NULL;
	}

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

int print_escaped(FILE *fp, const char *string, size_t length) {
	static struct {
		char from;
		const char *to;
	} replacements[] = {
		{ '\t', "\\t" },
		{ '\n', "\\n" },
		{ '"', "\\\"" },
	};

	if (fputc('"', fp) == EOF) {
		return -1;
	}

	for (size_t i = 0; i < length; ++i) {
		for (size_t j = 0; j < LENGTH(replacements); ++j) {
			if (string[i] == replacements[j].from) {
				if (fprintf(fp, "%s", replacements[j].to) < 0) {
					return -1;
				}
				goto next_char;
			}
		}
		if (fputc(string[i], fp) == EOF) {
			return -1;
		}
next_char:
		;
	}

	if (fputc('"', fp) == EOF) {
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		fprintf(stderr, "Takes no arguments, must be invoked in parent of test dir.\n");
		return EXIT_FAILURE;
	}

	glob_t glob_result;
	if (glob("./test/*.input.txt", GLOB_ERR, NULL, &glob_result) < 0) {
		perror("Glob failed");
		return EXIT_FAILURE;
	}

	unsigned ok = 0;
	unsigned failures = 0;
	unsigned errors = 0;

	for (int i = 0; i < glob_result.gl_matchc; ++i) {
		char *input_name = glob_result.gl_pathv[i];

		int input_name_len = strlen(input_name);
		int prefix_len = strlen("./test/");
		int sufix_len = strlen(".input.txt");
		printf("Running: " BOLD "%.*s" CLEAR "... ", input_name_len - prefix_len - sufix_len, input_name + prefix_len);

		char *input_buffer = NULL;
		size_t input_length;
		if (read_file(input_name, &input_buffer, &input_length) < 0) {
			printf(RED "internal error!" CLEAR "\n  " CYAN "error:" CLEAR " reading %s: %s\n", input_name, strerror(errno));
			errors += 1;
			goto fail_input_buffer;
		}

		// TODO: replace() is a bit overkill. Just strcpy to buffer of size (strlen(input_name) - strlen(".input.txt") + strlen(".output.txt")).
		char *output_name = replace(input_name, ".input.txt", ".output.txt");
		if (output_name == NULL) {
			printf(RED "internal error!" CLEAR "\n  " CYAN "error:" CLEAR " generating output name: %s\n", strerror(errno));
			errors += 1;
			goto fail_output_name;
		}
		char *output_buffer = NULL;
		size_t output_length;
		if (read_file(output_name, &output_buffer, &output_length) < 0) {
			printf(RED "internal error!" CLEAR "\n  " CYAN "error:" CLEAR " reading %s: %s\n", output_name, strerror(errno));
			errors += 1;
			goto fail_output_buffer;
		}

		// Do actual render.
		static char buffer[1024];
		FILE *fp = fmemopen(buffer, sizeof(buffer), "wb");
		render_creole(fp, input_buffer, input_length);
		long buffer_length = ftell(fp);
		fclose(fp);

		bool success = strcmp(output_buffer, buffer) == 0;
		if (success) {
			ok += 1;
			printf(GREEN "ok" CLEAR "\n");
		} else {
			failures += 1;
			printf(RED "unexpected output!" CLEAR);
			printf(CYAN "\n  input: " CLEAR);
			print_escaped(stdout, input_buffer, input_length);
			printf(CYAN "\n  want:  " CLEAR);
			print_escaped(stdout, output_buffer, output_length);
			printf(CYAN"\n  got:   " CLEAR);
			print_escaped(stdout, buffer, buffer_length); // TODO: rendered
			putchar('\n');
		}

		free(output_buffer);
fail_output_buffer:
		free(output_name);
fail_output_name:
		free(input_buffer);
fail_input_buffer:
		;
	}

	printf("Summary: " YELLOW "%u" CLEAR " errors, " RED "%u" CLEAR " failures and " GREEN "%u" CLEAR " successes\n", errors, failures, ok);

	globfree(&glob_result);
	return (failures == 0 && errors == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
