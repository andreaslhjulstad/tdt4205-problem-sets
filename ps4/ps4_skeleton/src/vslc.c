#include "vslc.h"

#include <getopt.h>

static bool print_full_tree = false;
static bool print_simplified_tree = false;
static bool print_symbol_table_contents = false;

static const char* usage = "Compiler for VSL. The input program is read from stdin."
                           "\n"
                           "Options:\n"
                           "\t -h \t Output this text and exit\n"
                           "\t -t \t Output the abstract syntax tree\n"
                           "\t -T \t Output the abstract syntax tree after constant folding\n"
                           "\t    \t and removing unreachable code\n"
                           "\t -s \t Output the symbol table contents\n";

// Command line option parsing
static void options(int argc, char** argv)
{
  while (true)
  {
    switch (getopt(argc, argv, "htTs"))
    {
    case 'h':
      printf("%s:\n%s", argv[0], usage);
      exit(EXIT_SUCCESS);
    case 't':
      print_full_tree = true;
      break;
    case 'T':
      print_simplified_tree = true;
      break;
    case 's':
      print_symbol_table_contents = true;
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

  constant_fold_syntax_tree();
  remove_unreachable_code_syntax_tree();

  if (print_simplified_tree)
    print_syntax_tree();

  // Operations in symbols.c
  create_tables();
  if (print_symbol_table_contents)
    print_tables();

  destroy_tables();      // In symbols.c
  destroy_syntax_tree(); // In tree.c
}
