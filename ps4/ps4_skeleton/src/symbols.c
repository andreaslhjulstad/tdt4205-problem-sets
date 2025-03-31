#include <vslc.h>

// Declaration of global symbol table
symbol_table_t *global_symbols;

// Declarations of helper functions defined further down in this file
static void find_globals(void);
static void bind_names(symbol_table_t *local_symbols, node_t *root);
static void print_symbol_table(symbol_table_t *table, int nesting);
static void destroy_symbol_tables(void);

static size_t add_string(char *string);
static void print_string_list(void);
static void destroy_string_list(void);

static symbol_t *create_symbol(
    node_t *symbol_node,
    char *name,
    symtype_t symbol_type,
    symbol_table_t *symbol_table);

/* External interface */

// Creates a global symbol table, and local symbol tables for each function.
// All usages of symbols are bound to their symbol table entries.
// All strings are entered into the string_list
void create_tables(void)
{
  find_globals();

  for (int i = 0; i < global_symbols->n_symbols; i++)
  {
    symbol_t *symbol = global_symbols->symbols[i];

    if (symbol->type == SYMBOL_FUNCTION)
    {
      symbol_table_t *function_symtable = symbol->function_symtable;
      node_t *function_node = symbol->node;
      node_t *parameter_list_node = function_node->children[1];

      if (function_node->n_children < 3)
      {
        printf("Error when binding local symbols: wrong format of function node!");
        exit(EXIT_FAILURE);
      }

      // Create symbols for function parameters
      for (size_t i = 0; i < parameter_list_node->n_children; i++)
      {
        node_t *parameter_node = parameter_list_node->children[i];

        if (parameter_node->type != IDENTIFIER)
        {
          printf("Error when binding local symbols: wrong node type on parameter!");
          exit(EXIT_FAILURE);
        }
        symbol_t *parameter_symbol = create_symbol(parameter_node,
                                                   parameter_node->data.identifier,
                                                   SYMBOL_PARAMETER,
                                                   function_symtable);

        if (parameter_symbol == NULL)
        {
          printf("Error when creating function parameter symbol!");
          exit(EXIT_FAILURE);
        }

        parameter_symbol->function_symtable = function_symtable;
      }

      node_t *block_node = function_node->children[2];

      bind_names(function_symtable, block_node);
    }
  }
}

// Prints the global symbol table, and the local symbol tables for each function.
// Also prints the global string list.
// Finally prints out the AST again, with bound symbols.
void print_tables(void)
{
  print_symbol_table(global_symbols, 0);
  printf("\n == STRING LIST == \n");
  print_string_list();
  printf("\n == BOUND SYNTAX TREE == \n");
  print_syntax_tree();
}

// Cleans up all memory owned by symbol tables and the global string list
void destroy_tables(void)
{
  destroy_symbol_tables();
  destroy_string_list();
}

/* Internal matters */

// Goes through all global declarations, adding them to the global symbol table.
// When adding functions, a local symbol table with symbols for its parameters are created.
static void find_globals(void)
{
  global_symbols = symbol_table_init();
  assert(root->type == LIST);

  for (size_t i = 0; i < root->n_children; i++)
  {
    node_t *global_child_node = root->children[i];
    if (!(global_child_node->type == GLOBAL_DECLARATION || global_child_node->type == FUNCTION))
    {
      continue;
    }

    switch (global_child_node->type)
    {
    case GLOBAL_DECLARATION:
      if (global_child_node->n_children < 1)
      {
        printf("Error when inserting global symbol: wrong format of global declaration node!");
        exit(EXIT_FAILURE);
      }
      node_t *declaration_list_node = global_child_node->children[0]; // Global declaration always has a LIST node as its only child

      // A global declaration can have multiple declarations, go through all of them
      for (size_t j = 0; j < declaration_list_node->n_children; j++)
      {
        node_t *declaration_list_child = declaration_list_node->children[j];

        if (declaration_list_child->type == IDENTIFIER)
        {
          symbol_t *identifier_symbol = create_symbol(declaration_list_child, declaration_list_child->data.identifier, SYMBOL_GLOBAL_VAR, global_symbols);

          if (identifier_symbol == NULL)
          {
            printf("Error creating global variable symbol");
            exit(EXIT_FAILURE);
          }
        }
        else if (declaration_list_child->type == ARRAY_INDEXING)
        {
          // Array always has an IDENTIFIER node as its first child
          node_t *array_identifier_node = declaration_list_child->children[0];
          symbol_t *global_array_symbol = create_symbol(declaration_list_child, array_identifier_node->data.identifier, SYMBOL_GLOBAL_ARRAY, global_symbols);

          if (global_array_symbol == NULL)
          {
            printf("Error creating global variable symbol");
            exit(EXIT_FAILURE);
          }
        }
        else
        {
          continue;
        }
      }
      break;
    case FUNCTION:
      if (global_child_node->n_children < 1)
      {
        printf("Error when inserting global symbol: wrong format of function node!");
        exit(EXIT_FAILURE);
      }

      // Function always has an IDENTIFIER node as its first child
      node_t *function_identifier_node = global_child_node->children[0];

      symbol_t *function_symbol = create_symbol(global_child_node, function_identifier_node->data.identifier, SYMBOL_FUNCTION, global_symbols);

      if (function_symbol == NULL)
      {
        printf("Error creating global variable symbol");
        exit(EXIT_FAILURE);
      }

      symbol_table_t *function_symtable = symbol_table_init();
      symbol_hashmap_t *function_hashmap = function_symtable->hashmap;
      // Lecturer recommends: when creating a local symbol table, set its backup to be the hashmap of the global symbol table
      function_hashmap->backup = global_symbols->hashmap;

      function_symbol->function_symtable = function_symtable;
      break;
    default:
      break;
    }
  }
}

static symbol_t *create_symbol(
    node_t *symbol_node,
    char *name,
    symtype_t symbol_type,
    symbol_table_t *symbol_table)
{
  symbol_t *symbol = malloc(sizeof(symbol_t));

  symbol->node = symbol_node;
  symbol->name = name;
  symbol->type = symbol_type;

  insert_result_t res = symbol_table_insert(symbol_table, symbol);

  if (res == INSERT_COLLISION)
  {
    return NULL;
  }

  return symbol;
}

// A recursive function that traverses the body of a function, and:
//  - Adds variable declarations to the function's local symbol table.
//  - Pushes and pops local variable scopes when entering and leaving blocks.
//  - Binds all IDENTIFIER nodes that are not declarations, to the symbol it references.
//  - Moves STRING_LITERAL nodes' data into the global string list,
//    and replaces the node with a STRING_LIST_REFERENCE node.
//    Overwrites the node's data.string_list_index field with with string list index
static void bind_names(symbol_table_t *local_symbols, node_t *node)
{
  // For nodes that have been removed because of unreachable code
  if (node == NULL) {
    return;
  }

  symbol_hashmap_t *scope_hashmap = local_symbols->hashmap;
  switch (node->type)
  {
  case BLOCK:
    // Create a new hashmap for the scope
    symbol_hashmap_t *new_hashmap = symbol_hashmap_init();

    new_hashmap->backup = scope_hashmap;
    local_symbols->hashmap = new_hashmap;

    // When a BLOCK node has two children, the first is a LIST of 
    // LISTs of INDENTIFIERs that declare variables.
    if (node->n_children == 2) {
      node_t* first_child = node->children[0];
      for (size_t i = 0; i < first_child->n_children; i++) {
        node_t* identifier_list_node = first_child->children[i];

        for (size_t j = 0; j < identifier_list_node->n_children; j++) {
          node_t* identifier_node = identifier_list_node->children[j];
          char *name = identifier_node->data.identifier;

          symbol_t *local_var_symbol = create_symbol(node, name, SYMBOL_LOCAL_VAR, local_symbols);
  
          if (local_var_symbol == NULL)
          {
            printf("Error creating local variable symbol");
            exit(EXIT_FAILURE);
          }
        }
      }
      // Go through the rest of the statements in the block
      for (size_t k = 1; k < node->n_children; k++)
      {
        bind_names(local_symbols, node->children[k]);
      }
    } else {
      // Since n_children != 2, there are no local variable declarations,
      // recursively go through all statements in the block
      for (size_t i = 0; i < node->n_children; i++)
      {
        bind_names(local_symbols, node->children[i]);
      }
    }
    
    // Pop hashmap from the "stack" by restoring the old one
    local_symbols->hashmap = scope_hashmap;
    // Destroy the popped hashmap
    symbol_hashmap_destroy(new_hashmap);
    break;
  case IDENTIFIER:
    char *name = node->data.identifier;
    symbol_t *lookup_symbol = symbol_hashmap_lookup(scope_hashmap, name);

    if (lookup_symbol == NULL)
    {
      printf("Error referencing identifier in assignment statement");
      exit(EXIT_FAILURE);
    }
    else
    {
      node->symbol = lookup_symbol;
    }
    break;
  case STRING_LITERAL:
    char* string = node->data.string_literal;
    size_t index = add_string(string);

    node->type = STRING_LIST_REFERENCE;
    node->data.string_list_index = index;
  break;
  default:
    for (size_t j = 0; j < node->n_children; j++)
    {
      bind_names(local_symbols, node->children[j]);
    }
    break;
  }
}

// Prints the given symbol table, with sequence number, symbol names and types.
// When printing function symbols, its local symbol table is recursively printed, with indentation.
static void print_symbol_table(symbol_table_t *table, int nesting)
{
  for (size_t i = 0; i < table->n_symbols; i++)
  {
    symbol_t *symbol = table->symbols[i];

    printf(
        "%*s%ld: %s(%s)\n",
        nesting * 4,
        "",
        symbol->sequence_number,
        SYMBOL_TYPE_NAMES[symbol->type],
        symbol->name);

    // If the symbol is a function, print its local symbol table as well
    if (symbol->type == SYMBOL_FUNCTION)
      print_symbol_table(symbol->function_symtable, nesting + 1);
  }
}

// Frees up the memory used by the global symbol table, all local symbol tables, and their symbols
static void destroy_symbol_tables(void)
{
  for (size_t i = 0; i < global_symbols->n_symbols; i++) {
    symbol_t* symbol = global_symbols->symbols[i];

    if (symbol->type == SYMBOL_FUNCTION) {
      symbol_table_destroy(symbol->function_symtable);
    }
  }
  
  symbol_table_destroy(global_symbols);
}

// Declaration of global string list
char **string_list;
size_t string_list_len;
static size_t string_list_capacity;

// Adds the given string to the global string list, resizing if needed.
// Takes ownership of the string, and returns its position in the string list.
static size_t add_string(char *string)
{
  if (string_list_len + 1 >= string_list_capacity) {
    string_list_capacity = string_list_capacity * 2 + 8;
    string_list = realloc(string_list, string_list_capacity * sizeof(char*));
  } 

  string_list[string_list_len] = string; 

  return string_list_len++;
}

// Prints all strings added to the global string list
static void print_string_list(void)
{
  for (size_t i = 0; i < string_list_len; i++)
    printf("%ld: %s\n", i, string_list[i]);
}

// Frees all strings in the global string list, and the string list itself
static void destroy_string_list(void)
{
  for (size_t i = 0; i < string_list_len; i++) {
    free(string_list[i]);
  }
  free(string_list);
}
