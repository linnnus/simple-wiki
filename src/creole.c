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
int do_link(const char *begin, const char *end, bool new_block, FILE *out);
int do_raw_url(const char *begin, const char *end, bool new_block, FILE *out);
int do_emphasis(const char *begin, const char *end, bool new_block, FILE *out);

// Prints string escaped.
void hprint(FILE *out, const char *begin, const char *end) {
	for (const char *p = begin; p != end; p++) {
		if (*p == '&') {
			fputs("&amp;", out);
		} else if (*p == '"') {
			fputs("&quot;", out);
		} else if (*p == '>') {
			fputs("&gt;", out);
		} else if (*p == '<') {
			fputs("&lt;", out);
		} else {
			fputc(*p, out);
		}
	}
}

// A parser takes a (sub)string and returns the number of characters consumed, if any.
//
// The parameter `new_block` determines whether `begin` points to the beginning of a new block.
// The sign of the return value determines whether a new block should begin, after the consumed text.
typedef int (* parser_t)(const char *begin, const char *end, bool new_block, FILE *out);

static parser_t parsers[] = { do_headers, do_paragraph, do_emphasis, do_link, do_raw_url, do_replacements };

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
	{"~[[", "[["},
	{"~]]", "]]"}, // NOTE: This pattern is duplicated in do_link().
	// Characters that have special meaning in HTML
	// NOTE: These rules are duplicated in hprint().
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

int do_link(const char *begin, const char *end, bool new_block, FILE *out)
{
	// Links start with "[[".
	if (begin + 2 >= end || begin[0] != '[' || begin[1] != '[') {
		return 0;
	}
	const char *start = begin + 2;

	// Find the matching, unescaped "]]".
	// Poor man's for...else
	const char *stop = start - 1;
	do {
		stop = strnstr(stop + 1, "]]", end - (stop + 1));
	} while (stop != NULL && stop[-1] == '~');
	if (stop == NULL) {
		return 0;
	}

	// FIXME: How do we handle WikiWord style links? Should we just append ".html" if is_wikiword()?

	const char *pipe = strnstr(start, "|", stop - start);
	if (pipe != NULL) {
		const char *link_address_start = start;
		const char *link_address_stop = pipe;
		fprintf(out, "<a href=\"");
		hprint(out, link_address_start, link_address_stop);
		fprintf(out, "\">");

		const char *link_text_start = pipe + 1;
		const char *link_text_stop = stop;
		process(link_text_start, link_text_stop, false, out);
		fprintf(out, "</a>");
	} else {
		fprintf(out, "<a href=\"");
		hprint(out, start, stop);
		fprintf(out, "\">");
		hprint(out, start, stop); // Don't parse markup when we know it's a link.
		fprintf(out, "</a>");
	}

	return stop - start + 4 /* [[]] */;
}

int do_raw_url(const char *begin, const char *end, bool new_block, FILE *out)
{
	// Eat a scheme followed by a ":". Here are the relevant rules from RFC 3986.
	// - URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
	// - scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	// See: <https://www.rfc-editor.org/rfc/rfc3986#section-3.1>
	const char *p = begin;
	if (!isalpha(*p)) {
		return 0;
	}
	while (p < end && (isalnum(*p) || *p == '+' || *p == '-' || *p == '.')) {
		p += 1;
	}
	if (p >= end || p[0] != ':') {
		return 0;
	}
	p += 1;

        // Eat the remainder of the URI, purely going by what "legal" URI
        // characters it contains.
	// See: <https://stackoverflow.com/a/7109208>
        const char *q = p;
	while (q < end) {
		switch (*q) {
			case '0' ... '9':
			case 'a' ... 'z':
			case 'A' ... 'Z':
			case '-': case '.': case '_': case '~':
			case ':': case '/': case '?': case '#':
			case '[': case ']': case '@': case '!':
			case '$': case '&': case '\'': case '(':
			case ')': case '*': case '+': case ',':
			case ';': case '%': case '=':
				q += 1;
				break;
			default:
				goto end_url;
		}
	}
end_url:

        // If there is nothing following the colon, don't accept it as a raw
        // url. Otherwise we'd incorrectly find a link with the "said" protocol
        // here: "And he said: blah blah".
        if (q == p) {
		return 0;
	}

        // Special case: If we end on a ".", assume it's a full stop at the end
        // of a sentence. Here's an example:
	// My favorite webside is https://cohost.org/.
        if (q[-1] == '.') {
		q -= 1;
	}

	fputs("<a href=\"", out);
	hprint(out, begin, q);
	fputs("\">", out);
	hprint(out, begin, q);
	fputs("</a>", out);

	return q - begin;
}

int do_emphasis(const char *begin, const char *end, bool new_block, FILE *out) {
	if (begin + 2 >= end || begin[0] != '/' || begin[1] != '/') {
		return 0;
	}
	const char *start = begin + 2; /* // */

	const char *stop = start;
	do {
		stop = strnstr(stop + 1, "//", end - (stop + 1));
	} while (stop != NULL && stop[-1] == '~' && stop[-1] == ':');
	if (stop == NULL) {
		return 0;
	}

	fputs("<em>", out);
	process(start, stop, false, out);
	fputs("</em>", out);

	return stop - start + 4; /* //...// */
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
