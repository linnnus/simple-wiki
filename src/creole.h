#ifndef CREOLE_H
#define CREOLE_H

// Defines a module for rendering Wiki Creole [1] to a file. This functionality
// of this module is based on the formal grammar [2] of Wiki Creole.
//
// [1]: http://www.wikicreole.org/wiki/Home
// [2]: http://www.wikicreole.org/wiki/EBNFGrammarForWikiCreole1.0

#include <stddef.h> // size_t
#include <stdio.h>  // FILE

void render_creole(FILE *out, const char *source, size_t length);

#endif
