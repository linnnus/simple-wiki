#include "creole.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LENGTH(x)  (sizeof(x)/sizeof((x)[0]))

#define DEBUG(...) (fprintf(stderr, __VA_ARGS__), fflush(stderr))

void process(const char *begin, const char *end, bool new_block, FILE *out);
int do_headers(const char *begin, const char *end, bool new_block, FILE *out);
int do_paragraph(const char *begin, const char *end, bool new_block, FILE *out);
int do_replacements(const char *begin, const char *end, bool new_block, FILE *out);

// A parser takes a (sub)string and returns the number of characters consumed, if any.
//
// The parameter `new_block` determines whether `begin` points to the beginning of a new block.
// The sign of the return value determines whether a new block should begin, after the consumed text.
typedef int (* parser_t)(const char *begin, const char *end, bool new_block, FILE *out);

static parser_t parsers[] = { do_headers, do_paragraph, do_replacements };

int do_headers(const char *begin, const char *end, bool new_block, FILE *out) {
	if (!new_block) { // Headers are block-level elements.
		return 0;
	}

	if (*begin != '=') {
		return 0;
	}

	unsigned level = 0;
	const char *start = begin;
	while (*start == '=') {
		level += 1;
		start += 1;
	}
	if (level > 6) {
		return 0;
	}

	while (isspace(*start)) {
		start += 1;
	}

	const char *eol = start;
	while (eol != end && *eol != '\n') {
		eol += 1;
	}

	const char *stop = eol;
	assert(stop > begin);
	while (stop[-1] == '=' || isspace(stop[-1])) {
		stop -= 1;
	}

	fprintf(out, "<h%u>", level);
	process(start, stop, false, out);
	fprintf(out, "</h%u>", level);

	return -(eol - begin);
}

int do_paragraph(const char *begin, const char *end, bool new_block, FILE *out) {
	if (!new_block) { // Paragraphs are block-level elements.
		return 0;
	}

	const char *stop = begin + 1;
	while (stop + 1 < end) {
		if (stop[0] == '\n' && stop[1] == '\n') {
			goto found_double_newline;
		} else {
			stop += 1;
		}
	}
	stop = end;
found_double_newline:

	fputs("<p>", out);
	process(begin, stop, false, out);
	fputs("</p>", out);

	return -(stop - begin);
}

static struct {
	const char *from, *to;
} replacements[] = {
	// Escaped special characters
	// TODO (e.g. "\\{" should become "{" so users can escape and void it being parsed as markup
	// Characters that have special meaning in HTML
	{"<", "&lt;"},
	{">", "&gt;"},
	{"\"", "&quot;"},
	{"&", "&amp;"},
};

int do_replacements(const char *begin, const char *end, bool new_block, FILE *out)
{
	for (unsigned i = 0; i < LENGTH(replacements); ++i) {
		size_t length = strlen(replacements[i].from);
		if (end - begin < length) {
			continue;
		}
		if (strncmp(replacements[i].from, begin, length) == 0) {
			fputs(replacements[i].to, out);
			return length;
		}
	}

	return 0;
}

void process(const char *begin, const char *end, bool new_block, FILE *out) {
	const char *p = begin;
	while (p < end) {
		// Eat all newlines if we're starting a block.
		if (new_block) {
			while (*p == '\n') {
				p += 1;
				if (p == end) {
					return;
				}
			}
		}

		// Greedily try all parsers.
		int affected;
		for (unsigned i = 0; i < LENGTH(parsers); ++i) {
			affected = parsers[i](p, end, new_block, out);
			if (affected) {
				break;
			}
		}
		if (affected) {
			p += abs(affected);
		} else {
			fputc(*p, out);
			p += 1;
		}

		if (p + 1 == end) {
			// Don't print single newline at end.
			if (*p == '\n') {
				return;
			}
		} else {
			// Determine whether we've reached a new block.
			if (p[0] == '\n' && p[1] == '\n') {
				// Double newline characters separate blocks;
				// if we've found them, we're starting a new block
				new_block = true;
			} else {
				// ...otherwise the parser gets to decide.
				new_block = affected < 0;
			}
		}
	}
}

void render_creole(FILE *out, const char *source, size_t source_length)
{
	process(source, source + source_length, true, out);
}
