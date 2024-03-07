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
long do_headers(const char *begin, const char *end, bool new_block, FILE *out);
long do_paragraph(const char *begin, const char *end, bool new_block, FILE *out);
long do_replacements(const char *begin, const char *end, bool new_block, FILE *out);
long do_link(const char *begin, const char *end, bool new_block, FILE *out);
long do_raw_url(const char *begin, const char *end, bool new_block, FILE *out);
long do_emphasis(const char *begin, const char *end, bool new_block, FILE *out);
long do_bold(const char *begin, const char *end, bool new_block, FILE *out);
long do_nowiki_inline(const char *begin, const char *end, bool new_block, FILE *out);
long do_nowiki_block(const char *begin, const char *end, bool new_block, FILE *out);

// Prints string with special HTML characters escaped.
//
// Unlike many other functions, this function does not assume that (end >=
// begin). This simplifies some logic in callers since bracket-matching is prone
// to off-by-one errors when the brackets are empty.
void hprint(FILE *out, const char *begin, const char *end) {
	for (const char *p = begin; p < end; p++) {
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

bool starts_with(const char *haystack_begin, const char *haystack_end, const char *needle) {
	size_t needle_len = strlen(needle);
	size_t haystack_len = haystack_end - haystack_begin;
	if (needle_len > haystack_len) {
		return false;
	} else {
		return memcmp(haystack_begin, needle, needle_len) == 0;
	}
}


// A parser takes a (sub)string and returns the number of characters consumed, if any.
//
// The parameter `new_block` determines whether `begin` points to the beginning of a new block.
// The sign of the return value determines whether a new block should begin, after the consumed text.
typedef long (* parser_t)(const char *begin, const char *end, bool new_block, FILE *out);

static parser_t parsers[] = {
	// Block-level elements
	do_headers,
	do_nowiki_block,
	do_paragraph, // <p> should be last as it eats anything

	// Inline-level elements
	do_emphasis,
	do_bold,
	do_link,
	do_raw_url,
	do_nowiki_inline,
	do_replacements
};

long do_headers(const char *begin, const char *end, bool new_block, FILE *out) {
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

long do_paragraph(const char *begin, const char *end, bool new_block, FILE *out) {
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
	{"~//", "//"},
	{"~**", "**"},
	{"~{{{", "{{{"},
	// Characters that have special meaning in HTML
	// NOTE: These rules are duplicated in hprint().
	{"<", "&lt;"},
	{">", "&gt;"},
	{"\"", "&quot;"},
	{"&", "&amp;"},
};

long do_replacements(const char *begin, const char *end, bool new_block, FILE *out)
{
	for (unsigned i = 0; i < LENGTH(replacements); ++i) {
		size_t length = strlen(replacements[i].from);
		if ((size_t)(end - begin) < length) {
			continue;
		}
		if (strncmp(replacements[i].from, begin, length) == 0) {
			fputs(replacements[i].to, out);
			return length;
		}
	}

	return 0;
}

long do_link(const char *begin, const char *end, bool new_block, FILE *out)
{
	// Links start with "[[".
	if (!starts_with(begin, end, "[[")) {
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

long do_raw_url(const char *begin, const char *end, bool new_block, FILE *out)
{
	const char *p = begin;

	// This piece of spaghetti is necessary to handle escaped urls.
	// These should not actually be turned into anchor tags.
	// See: <http://www.wikicreole.org/wiki/Creole1.0#section-Creole1.0-EscapeCharacter>
	bool escaped = false;
	if (*begin == '~') {
		escaped = true;
		p += 1;
	}

	// Eat a scheme followed by a ":". Here are the relevant rules from RFC 3986.
	// - URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
	// - scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	// See: <https://www.rfc-editor.org/rfc/rfc3986#section-3.1>
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

	if (escaped) {
		hprint(out, begin + 1 /* ~ */, q);
	} else {
		fputs("<a href=\"", out);
		hprint(out, begin, q);
		fputs("\">", out);
		hprint(out, begin, q);
		fputs("</a>", out);
	}

	return q - begin;
}

long do_emphasis(const char *begin, const char *end, bool new_block, FILE *out) {
	if (!starts_with(begin, end, "//")) {
		return 0;
	}
	const char *start = begin + 2; /* // */

	const char *stop = start;
	do {
		stop = strnstr(stop + 1, "//", end - (stop + 1));
	} while (stop != NULL && (stop[-1] == '~' || stop[-1] == ':'));
	if (stop == NULL) {
		return 0;
	}

	fputs("<em>", out);
	process(start, stop, false, out);
	fputs("</em>", out);

	return stop - start + 4; /* //...// */
}

// FIXME: This is //almost// just a copy/paste of do_emphasis. Not very DRY...
//        The one difficult part is that : should only be treated as an escape character for //.
long do_bold(const char *begin, const char *end, bool new_block, FILE *out) {
	if (!starts_with(begin, end, "**")) {
		return 0;
	}
	const char *start = begin + 2; /* // */

	const char *stop = start;
	do {
		stop = strnstr(stop + 1, "**", end - (stop + 1));
	} while (stop != NULL && stop[-1] == '~');
	if (stop == NULL) {
		return 0;
	}

	fputs("<strong>", out);
	process(start, stop, false, out);
	fputs("</strong>", out);

	return stop - start + 4; /* **...** */
}

// The inline-level nowiki element.
// This is specified together with the block-level nowiki element in the spec, but for this parser it makes more sense to treat them as separate.
// See: <http://www.wikicreole.org/wiki/Creole1.0#section-Creole1.0-NowikiPreformatted>
long do_nowiki_inline(const char *begin, const char *end, bool new_block, FILE *out) {
	if (!starts_with(begin, end, "{{{")) {
		return 0;
	}
	const char *start = begin + 3;

	const char *stop = strnstr(start, "}}}", end - start);
	if (stop == NULL) {
		return 0;
	}

	// Include trailing closing braces in the span.
	while (stop + 3 < end && stop[3] == '}') {
		stop += 1;
	}

	const char *trim_start = start;
	while (isspace(*trim_start)) {
		trim_start += 1;
	}
	const char *trim_stop = stop;
	while (isspace(trim_stop[-1]) && trim_start <= trim_stop - 1) {
		trim_stop -= 1;
	}

	fputs("<tt>", out);
	hprint(out, trim_start, trim_stop);
	fputs("</tt>", out);

	return 3 + (stop - start) + 3; /* {{{...}}} */
}

long do_nowiki_block(const char *begin, const char *end, bool new_block, FILE *out) {
	if (!(new_block && starts_with(begin, end, "{{{\n"))) {
		return 0;
	}
	const char *start = begin + 4;

	const char *stop = strnstr(start - 1, "\n}}}", end - (start - 1));
	if (stop == NULL) {
		return 0;
	}

	fputs("<pre><code>", out);
	hprint(out, start, stop);
	fputs("</code></pre>", out);

	return -(stop - start + 8);
}

void process(const char *begin, const char *end, bool new_block, FILE *out) {
	assert(begin <= end);

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
		long affected;
		for (unsigned i = 0; i < LENGTH(parsers); ++i) {
			affected = parsers[i](p, end, new_block, out);
			if (affected) {
				break;
			}
		}
		if (affected) {
			p += labs(affected);
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
