#include "vslc.h"

#include <getopt.h>

static bool print_full_tree = false;

static const char* usage = "Compiler for VSL. The input program is read from stdin."
                           "\n"
                           "Options:\n"
                           "\t -h \tOutput this text and exit\n"
                           "\t -t \tOutput the abstract syntax tree\n";

// Command line option parsing
static void options(int argc, char** argv)
{
  while (true)
  {
    switch (getopt(argc, argv, "ht"))
    {
    case 'h':
      printf("%s:\n%s", argv[0], usage);
      exit(EXIT_SUCCESS);
    case 't':
      print_full_tree = true;
      break;
    case -1:
      return; // Done parsing options
    }
  }
}

// Entry point
int main(int argc, char** argv)
{
  options(argc, argv);

  yyparse();       // Generated from grammar/bison, constructs syntax tree
  yylex_destroy(); // Free buffers used by flex

  // Operations in tree.c
  if (print_full_tree)
    print_syntax_tree();

  destroy_syntax_tree(); // In tree.c
}
