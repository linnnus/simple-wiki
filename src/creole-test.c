#include "creole.h"

#include <stdio.h>
#include <string.h>

#define COUNT(arr) (sizeof(arr)/sizeof((arr)[0]))

#define strneq(a, b, n) (strncmp(a, b, n) == 0)

struct {
	const char *name, *input, *output;
} tests[] = {
	{
		.name    =  "Empty input produces no output",
		.input   =  "",
		.output  =  ""
	},
	{
		.name    =  "Basic paragraph markup",
		.input   =  "Basic paragraph test with <, >, & and \"",
		.output  =  "<p>Basic paragraph test with &lt;, &gt;, &amp; and &quot;</p>"
	},
	{
		.name    =  "Two paragraphs next to each other.",
		.input   =  "Hello,\n\nworld!",
		.output  =  "<p>Hello,</p><p>world!</p>"
	},
	{
		.name    =  "h1",
		.input   =  "= Header =",
		.output  =  "<h1>Header</h1>"
	},
	{
		.name    =  "h2",
		.input   =  "== Header =",
		.output  =  "<h2>Header</h2>"
	},
	{
		.name    =  "h3",
		.input   =  "=== Header =",
		.output  =  "<h3>Header</h3>"
	},
	{
		.name    =  "h4",
		.input   =  "==== Header =",
		.output  =  "<h4>Header</h4>"
	},
	{
		.name    =  "h5",
		.input   =  "===== Header",
		.output  =  "<h5>Header</h5>"
	},
	{
		.name    =  "h6",
		.input   =  "====== Header =",
		.output  =  "<h6>Header</h6>"
	},
	{
		.name    =  ">h6",
		.input   =  "======= Header =",
		.output  =  "<p>======= Header =</p>"
	},
	{
		.name    =  "Unnamed link",
		.input   =  "[[MyPage]]",
		.output  =  "<p><a href=\"MyPage\">MyPage</a></p>"
	},
	{
		.name    =  "Named link",
		.input   =  "[[MyPage|My page]]",
		.output  =  "<p><a href=\"MyPage\">My page</a></p>"
	},
	{
		.name    =  "Escaped link",
		.input   =  "A paragraph with an ~[[escaped link]].",
		.output  =  "<p>A paragraph with an [[escaped link]].</p>"
	},
	{
		.name    =  "Link with an escaped end",
		.input   =  "[[https://example.com|A link with an escaped ~]] end]]",
		.output  =  "<p><a href=\"https://example.com\">A link with an escaped ]] end</a></p>"
	},
	{
		.name    =  "Link with empty text",
		.input   =  "[[https://example.com|]]",
		.output  =  "<p><a href=\"https://example.com\"></a></p>"
	},
	{
		.name    =  "Link with empty address",
		.input   =  "[[|Hello]]",
		.output  =  "<p><a href=\"\">Hello</a></p>"
	},
	{
		.name    =  "Empty link",
		.input   =  "[[]]",
		.output  =  "<p><a href=\"\"></a></p>"
	},
	{
		.name    =  "Raw HTTP URL",
		.input   =  "Here is a http://example.com/examplepage link.",
		.output  =  "<p>Here is a <a href=\"http://example.com/examplepage\">"
		            "http://example.com/examplepage</a> link.</p>"
	},
	{ // This is interesting because it doesn't contain a "://".
		.name    =  "Raw mailto URL",
		.input   =  "mailto:quandale@dingle.com",
		.output  =  "<p><a href=\"mailto:quandale@dingle.com\">"
		            "mailto:quandale@dingle.com</a></p>"
	},
	{ // This test captures a non-standard (?) special case in the parser.
		.name    =  "Raw URL followed by full stop",
		.input   =  "My favorite website is https://wiki.c2.com/.",
		.output  =  "<p>My favorite website is <a href=\"https://wiki.c2.com/\">"
		            "https://wiki.c2.com/</a>.</p>"
	},
	{
		.name    =  "Unnamed URL",
		.input   =  "[[http //example.com/examplepage]]",
		.output  =  "<p><a href=\"http //example.com/examplepage\">"
		            "http //example.com/examplepage</a></p>"
	},
	{
		.name    =  "Named URL",
		.input   =  "[[http //example.com/examplepage|Example Page]]",
		.output  =  "<p>"
		            "<a href=\"http //example.com/examplepage\">Example Page</a></p>"
	},
#if 0
	{
		.name    =  "Simple unordered list",
		.input   =  "* list item\n*list item 2",
		.output  =  "<ul><li> list item</li>\n<li>list item 2</li></ul>"
	},
	{
		.name    =  "Simple ordered list",
		.input   =  "# list item\n#list item 2",
		.output  =  "<ol><li> list item</li>\n<li>list item 2</li></ol>"
	},
	{
		.name    =  "Unordered item with unordered sublist",
		.input   =  "* Item\n** Subitem",
		.output  =  "<ul><li> Item<ul>\n<li> Subitem</li></ul></li></ul>"
	},
	{
		.name    =  "Unordered sublist without initial tag",
		.input   =  "** Sublist item",
		.output  =  "<p>** Sublist item</p>"
	},
	{
		.name    =  "Ordered item with ordered sublist",
		.input   =  "# Item\n## Subitem",
		.output  =  "<ol><li> Item<ol>\n<li> Subitem</li></ol></li></ol>"
	},
	{
		.name    =  "Ordered sublist without initial tag",
		.input   =  "## Sublist item",
		.output  =  "<p>## Sublist item</p>"
	},
	{
		.name    =  "Unordered item with ordered sublist",
		.input   =  "* Item\n*# Subitem",
		.output  =  "<ul><li> Item<ol>\n<li> Subitem</li></ol></li></ul>"
	},
	{
		.name    =  "Horizontal rule",
		.input   =  "Some text\n----\nSome more text",
		.output  =  "<p>Some text</p><hr /><p>Some more text</p>"
	},
	{
		.name    =  "Preformatted block",
		.input   =  "{{{\nPreformatted block\n}}}",
		.output  =  "<pre>Preformatted block\n</pre>"
	},
	{
		.name    =  "Two preformatted blocks",
		.input   =  "{{{\nPreformatted block\n}}}\n{{{Block 2}}}",
		.output  =  "<pre>Preformatted block\n</pre><pre>Block 2</pre>"
	},
	{
		.name    =  "Tables",
		.input   =  "| A | B |\n| //C// | **D** \\\\ E |",
		.output  =  "<table><tr><td> A </td><td> B </td></tr>"
		            "<tr><td> <em>C</em> </td>"
		            "<td> <strong>D</strong> <br /> E </td></tr></table>"
	},
	{
		.name    =  "Image",
		.input   =  "{{image.gif|my image}}",
		.output  =  "<p><img src=\"image.gif\" alt=\"my image\"/></p>"
	},
	{
		.name    =  "Inline tt",
		.input   =  "Inline {{{tt}}} example {{{here}}}!",
		.output  =  "<p>Inline <tt>tt</tt> example <tt>here</tt>!</p>"
	},
	{
		.name    =  "Strong",
		.input   =  "**Strong**",
		.output  =  "<p><strong>Strong</strong></p>"
	},
	{
		.name    =  "Emphasis",
		.input   =  "//Emphasis//",
		.output  =  "<p><em>Emphasis</em></p>"
	},
	{
		.name    =  "Multi-line emphasis",
		.input   =  "Bold and italics should //be\nable// to cross lines.\n\n"
		            "But, should //not be...\n\n...able// to cross paragraphs.",
		.output  =  "<p>Bold and italics should <em>be\nable</em> to cross lines.\n</p>"
		            "<p>\nBut, should //not be...\n</p>"
		            "<p>\n...able// to cross paragraphs.</p>"
	},
	{
		.name    =  "URL/emphasis ambiguity",
		.input   =  "This is an //italic// text. This is a url  "
		            "http //www.wikicreole.org. This is what can go wrong //this "
		            "should be an italic text//.",
		.output  =  "<p>This is an <em>italic</em> text. This is a url  "
		            "<a href=\"http //www.wikicreole.org\">"
		            "http //www.wikicreole.org</a>. This is what can go wrong "
		            "<em>this should be an italic text</em>.</p>"
	},
	{
		.name    =  "Difficult emphasis #1",
		.input   =  "// http //www.link.org //",
		.output  =  "<p><em> <a href=\"http //www.link.org\">"
		            "http //www.link.org</a> </em></p>"
	},
	{
		.name    =  "Difficult emphasis #2",
		.input   =  "// http //",
		.output  =  "<p><em> http </em></p>"
	},
	{
		.name    =  "Difficult emphasis #3",
		.input   =  "// httphpthtpht //",
		.output  =  "<p><em> httphpthtpht </em></p>"
	},
	{
		.name    =  "Difficult emphasis #4",
		.input   =  "// http  //",
		.output  =  "<p><em> http  </em></p>"
	},
	{
		.name    =  "Difficult emphasis #5",
		.input   =  "// http //",
		.output  =  "<p>// <a href=\"http //\">http //</a></p>"
	},
	{
		.name    =  "Difficult emphasis #6",
		.input   =  "// http ////",
		.output  =  "<p><em> <a href=\"http //\">http //</a></em></p>"
	},
	{
		.name    =  "Difficult emphasis #7",
		.input   =  "//httphpthtphtt//",
		.output  =  "<p><em>httphpthtphtt</em></p>"
	},
	{
		.name    =  "Difficult emphasis #8",
		.input   =  "//http //link.org//",
		.output  =  "<p><em><a href=\"http //link.org\">"
		            "http //link.org</a></em></p>"
	},
	{
		.name    =  "Difficult emphasis #9",
		.input   =  "// ftp //www.link.org //",
		.output  =  "<p><em> <a href=\"ftp //www.link.org\">"
		            "ftp //www.link.org</a> </em></p>"
	},
	{
		.name    =  "Difficult emphasis #10",
		.input   =  "// ftp //",
		.output  =  "<p><em> ftp </em></p>"
	},
	{
		.name    =  "Difficult emphasis #11",
		.input   =  "// fttpfptftpft //",
		.output  =  "<p><em> fttpfptftpft </em></p>"
	},
	{
		.name    =  "Difficult emphasis #12",
		.input   =  "// ftp  //",
		.output  =  "<p><em> ftp  </em></p>"
	},
	{
		.name    =  "Difficult emphasis #13",
		.input   =  "// ftp //",
		.output  =  "<p>// <a href=\"ftp //\">ftp //</a></p>"
	},
	{
		.name    =  "Difficult emphasis #14",
		.input   =  "// ftp ////",
		.output  =  "<p><em> <a href=\"ftp //\">ftp //</a></em></p>"
	},
	{
		.name    =  "Difficult emphasis #15",
		.input   =  "//fttpfptftpftt//",
		.output  =  "<p><em>fttpfptftpftt</em></p>"
	},
	{
		.name    =  "Difficult emphasis #16",
		.input   =  "//ftp //link.org//",
		.output  =  "<p><em><a href=\"ftp //link.org\">"
		            "ftp //link.org</a></em></p>"
	}
#endif
};

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
               for (size_t j = 0; j < COUNT(replacements); ++j) {
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

int print_escaped_ze(FILE *fp, const char *string) {
	return print_escaped(fp, string, strlen(string));
}

int main(void) {
	for (size_t i = 0; i < COUNT(tests); ++i) {
		printf("Running test: \x1b[1m%s\x1b[0m... ", tests[i].name);

		static char buffer[1024];
		FILE *fp = fmemopen(buffer, sizeof(buffer), "wb");
		render_creole(fp, tests[i].input, strlen(tests[i].input));
		long buffer_length = ftell(fp);
		fclose(fp);

		if (!strneq(buffer, tests[i].output, buffer_length)) {
			printf("\x1b[31merror\x1b[0m\n");
			printf("├──── markup: ");
			print_escaped_ze(stdout, tests[i].input);
			putchar('\n');
			printf("├── expected: ");
			print_escaped_ze(stdout, tests[i].output);
			putchar('\n');
			printf("└─────── got: ");
			print_escaped(stdout, buffer, buffer_length);
			putchar('\n');
		} else {
			printf("\x1b[32mok\x1b[0m\n");
		}
	}
}
