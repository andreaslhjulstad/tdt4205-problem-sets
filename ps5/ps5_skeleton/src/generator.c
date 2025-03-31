#include "vslc.h"

// This header defines a bunch of macros we can use to emit assembly to stdout
#include "emit.h"

// In the System V calling convention, the first 6 integer parameters are passed in registers
#define NUM_REGISTER_PARAMS 6
static const char *REGISTER_PARAMS[6] = {RDI, RSI, RDX, RCX, R8, R9};

// Takes in a symbol of type SYMBOL_FUNCTION, and returns how many parameters the function takes
#define FUNC_PARAM_COUNT(func) ((func)->node->children[1]->n_children)

static void generate_stringtable(void);
static void generate_global_variables(void);
static void generate_function(symbol_t *function);
static void generate_expression(node_t *expression);
static void generate_statement(node_t *node);
static void generate_main(symbol_t *first);
static int get_stack_offset(size_t sequence_number);

static size_t find_max(size_t a, size_t b)
{
  return (a > b) ? a : b;
}

static size_t find_min(size_t a, size_t b)
{
  return (a < b) ? a : b;
}

// Entry point for code generation
void generate_program(void)
{
  generate_stringtable();
  generate_global_variables();

  // This directive announces that the following assembly belongs to the .text section,
  // which is the section where all executable assembly lives
  DIRECTIVE(".text");

  // For each function in global_symbols, generate it using generate_function ()
  symbol_t *main_function = NULL;
  for (size_t i = 0; i < global_symbols->n_symbols; i++)
  {
    symbol_t *symbol = global_symbols->symbols[i];
    if (symbol->type == SYMBOL_FUNCTION)
    {
      if (main_function == NULL)
      {
        main_function = symbol;
      }
      generate_function(symbol);
    }
  }

  // In VSL, the topmost function in a program is its entry point.
  // We want to be able to take parameters from the command line,
  // and have them be sent into the entry point function.
  //
  // Due to the fact that parameters are all passed as strings,
  // and passed as the (argc, argv)-pair, we need to make a wrapper for our entry function.
  // This wrapper handles string -> int64_t conversion, and is already implemented.
  // call generate_main ( <entry point function symbol> );
  assert(main_function != NULL);
  generate_main(main_function);
}

// Prints one .asciz entry for each string in the global string_list
static void generate_stringtable(void)
{
  // This section is where read-only string data is stored
  // It is called .rodata on Linux, and "__TEXT, __cstring" on macOS
  DIRECTIVE(".section %s", ASM_STRING_SECTION);

  // These strings are used by printf
  DIRECTIVE("intout: .asciz \"%s\"", "%ld");
  DIRECTIVE("strout: .asciz \"%s\"", "%s");
  // This string is used by the entry point-wrapper
  DIRECTIVE("errout: .asciz \"%s\"", "Wrong number of arguments");

  // You have access to the global variables string_list and string_list_len from symbols.c
  for (size_t i = 0; i < string_list_len; i++)
  {
    char *string = string_list[i];
    DIRECTIVE("string%ld: .asciz %s", i, string);
  }
}

// Prints .zero entries in the .bss section to allocate room for global variables and arrays
static void generate_global_variables(void)
{
  // This section is where zero-initialized global variables lives
  // It is called .bss on linux, and "__DATA, __bss" on macOS
  DIRECTIVE(".section %s", ASM_BSS_SECTION);
  DIRECTIVE(".align 8");

  // Give each a label you can find later, and the appropriate size.
  // Regular variables are 8 bytes, while arrays are 8 bytes per element.
  // Remember to mangle the name in some way, to avoid collisions with labels
  // (for example, put a '.' in front of the symbol name)

  // As an example, to set aside 16 bytes and label it .myBytes, write:
  // DIRECTIVE(".myBytes: .zero 16")
  for (size_t i = 0; i < global_symbols->n_symbols; i++)
  {
    symbol_t *symbol = global_symbols->symbols[i];

    if (symbol->type == SYMBOL_FUNCTION)
      continue;

    size_t no_of_bytes = 8;

    if (symbol->type == SYMBOL_GLOBAL_ARRAY)
    {
      node_t *node = symbol->node;

      int64_t no_of_elements = node->children[1]->data.number_literal;

      no_of_bytes = no_of_elements * 8;
    }

    DIRECTIVE(".%s: .zero %ld", symbol->name, no_of_bytes);
  }
}

// Global variable used to make the functon currently being generated accessible from anywhere
static symbol_t *current_function;

// Prints the entry point. preamble, statements and epilouge of the given function
static void generate_function(symbol_t *function)
{
  current_function = function;
  LABEL(".%s", function->name);

  PUSHQ(RBP);
  MOVQ(RSP, RBP);

  // Tip: use the definitions REGISTER_PARAMS and NUM_REGISTER_PARAMS at the top of this file
  size_t no_of_params_to_push = find_min((size_t)NUM_REGISTER_PARAMS, FUNC_PARAM_COUNT(function));
  for (size_t i = 0; i < no_of_params_to_push; i++)
  {
    PUSHQ(REGISTER_PARAMS[i]);
  }

  symbol_table_t *function_symbols = function->function_symtable;

  for (size_t i = 0; i < function_symbols->n_symbols; i++)
  {
    symbol_t *symbol = function_symbols->symbols[i];

    if (symbol->type == SYMBOL_LOCAL_VAR)
    {
      PUSHQ("$0");
    }
  }

  generate_statement(function->node);

  LABEL(".%s.epilogue", function->name);
  MOVQ(RBP, RSP);
  POPQ(RBP);
  RET;
}

// Generates code for a function call, which can either be a statement or an expression
static void generate_function_call(node_t *call)
{
  node_t *identifier_node = call->children[0];
  symbol_t *function_identifier_symbol = identifier_node->symbol;
  assert(function_identifier_symbol->type == SYMBOL_FUNCTION);
  int64_t param_count = FUNC_PARAM_COUNT(function_identifier_symbol);

  node_t *parameter_list_node = call->children[1];

  for (int i = parameter_list_node->n_children - 1; i >= 0; i--)
  {
    // Push all evaluated arguments to the stack from right to left
    node_t *param_node = parameter_list_node->children[i];
    generate_expression(param_node);
    PUSHQ(RAX);
  }

  // Since param_count can be greater than the number of param registers, we must find the minimum to know
  // how many arguments to pop off the stack and put in the param registers
  int64_t number_of_params_for_registers = find_min(param_count, NUM_REGISTER_PARAMS);
  for (int j = 0; j < number_of_params_for_registers; j++)
  {
    POPQ(REGISTER_PARAMS[j]);
  }

  char *function_label = function_identifier_symbol->name;
  EMIT("call .%s", function_label);
}

// Generates code to evaluate the expression, and place the result in %rax
static void generate_expression(node_t *expression)
{
  if (expression == NULL)
    return;
  // (The candidates are NUMBER_LITERAL, IDENTIFIER, ARRAY_INDEXING, OPERATOR and FUNCTION_CALL)
  switch (expression->type)
  {
  case NUMBER_LITERAL:
    EMIT("movq $%d, %s", (int)expression->data.number_literal, RAX);
    break;
  case IDENTIFIER:
    symbol_t *identifier_symbol = expression->symbol;

    if (identifier_symbol->type == SYMBOL_GLOBAL_VAR)
    {
      EMIT("movq .%s(%s), %s", expression->data.identifier, RIP, RAX);
    }
    else
    {
      int offset = get_stack_offset(identifier_symbol->sequence_number);
      EMIT("movq %d(%s), %s", offset, RBP, RAX);
    }

    break;
  case ARRAY_INDEXING:
    node_t *array_identifier_node = expression->children[0];
    node_t *index_node = expression->children[1];
    
    // Save registers that might be overwritten during index evaluation
    PUSHQ(RCX);  // Save RCX since we'll use it to hold the array base address
    
    // Generate code to evaluate the index expression
    generate_expression(index_node);
  
    // Get the base address of the array and store it in RCX
    EMIT("leaq .%s(%s), %s", array_identifier_node->data.identifier, RIP, RCX);
    
    // Calculate the address of the array element
    EMIT("leaq (%s, %s, 8), %s", RCX, RAX, RCX);
    
    // Load the value from the array
    EMIT("movq (%s), %s", RCX, RAX);
    
    // Restore saved register
    POPQ(RCX);
    break;
  case OPERATOR:
    if (expression->n_children > 1)
    {
      // Binary operator
      // Generate expression from LH-side
      generate_expression(expression->children[0]);
      // Result is stored in %rax, push value to the stack
      PUSHQ(RAX);

      // Generate expression from RH-side
      generate_expression(expression->children[1]);

      // Pop LH-side into RCX
      POPQ(RCX);

      // Depending on operator, do the operation
      const char *operator= expression->data.operator;
      if (strcmp(operator, "+") == 0)
      {
        ADDQ(RCX, RAX);
      }
      else if (strcmp(operator, "-") == 0)
      {
        SUBQ(RAX, RCX); // RCX = RCX - RAX
        MOVQ(RCX, RAX); // Move result from RCX to RAX
      }
      else if (strcmp(operator, "*") == 0)
      {
        IMULQ(RCX, RAX);
      }
      else if (strcmp(operator, "/") == 0)
      {
        CQO;
        // Temporarily store RAX in R8
        MOVQ(RAX, R8);
        // Move RCX into RAX
        MOVQ(RCX, RAX);
        // Divide RDX:RAX by R8
        IDIVQ(R8);
      }
      else
      {
        CMPQ(RAX, RCX);
        if (strcmp(operator, "<") == 0)
        {
          SETL(AL);
        }
        else if (strcmp(operator, "<=") == 0)
        {
          SETLE(AL);
        }
        else if (strcmp(operator, ">") == 0)
        {
          SETG(AL);
        }
        else if (strcmp(operator, ">=") == 0)
        {
          SETGE(AL);
        }
        else if (strcmp(operator, "==") == 0)
        {
          SETE(AL);
        }
        else if (strcmp(operator, "!=") == 0)
        {
          SETNE(AL);
        }
        else
        {
          assert(false && "Unknown binary operator");
        }
        MOVZBQ(AL, RAX);
      }
    }
    else
    {
      // Unary operator
      generate_expression(expression->children[0]);
      const char *operator= expression->data.operator;
      if (strcmp(operator, "-") == 0)
      {
        NEGQ(RAX);
      }
      else if (strcmp(operator, "!") == 0)
      {
        EMIT("test %s, %s", RAX, RAX);
        SETE(AL);
        MOVZBQ(AL, RAX);
      }
      else
      {
        assert(false && "Unknown unary operator");
      }
    }
    break;
  case FUNCTION_CALL:
    generate_function_call(expression);
    break;
  default:
    break;
  }
}

static int get_stack_offset(size_t sequence_number)
{
  size_t param_count = FUNC_PARAM_COUNT(current_function);
  int offset;
  if (sequence_number < param_count)
  {
    // Parameter
    if (sequence_number > 5)
    {
      // Caller pushed (positive offset)
      offset = ((sequence_number - 5) * 8) + 8; // The +8 is to adjust for the return address being at 8(%rbp)
    }
    else
    {
      // Callee pushed (negative offset)
      offset = (sequence_number + 1) * (-8); // Times -8 to indicate a negative shift from (%rbp)
    }
  }
  else
  {
    // Local variable
    size_t num_of_callee_pushed_params = find_max(param_count, NUM_REGISTER_PARAMS);
    offset = (num_of_callee_pushed_params + (sequence_number - param_count) + 1) * (-8);
  }
  return offset;
}

static void generate_assignment_statement(node_t *statement)
{
  // You can assign to both local variables, global variables and function parameters.
  // Use the IDENTIFIER's symbol to find out what kind of symbol you are assigning to.
  // The left hand side of an assignment statement may also be an ARRAY_INDEXING node.
  // In that case, you must also emit code for evaluating the index being stored to
  node_t *left_side = statement->children[0];
  node_t *right_side = statement->children[1];
  // Generate expression for RH-side, it is stored in %rax
  generate_expression(right_side);
  if (left_side->type == IDENTIFIER)
  {
    symbol_t *identifier_symbol = left_side->symbol;

    if (identifier_symbol->type == SYMBOL_GLOBAL_VAR)
    {
      EMIT("movq %s, .%s(%s)", RAX, identifier_symbol->name, RIP);
      return;
    }

    size_t sequence_number = identifier_symbol->sequence_number;
    int offset = get_stack_offset(sequence_number);
    EMIT("movq %s, %d(%s)", RAX, offset, RBP);
  }
  else
  {
    assert(left_side->type == ARRAY_INDEXING);
    symbol_t *array_identifier_symbol = left_side->children[0]->symbol;
    node_t* index_node = left_side->children[1];
    
    // Save the result from the right-hand side expression 
    PUSHQ(RAX);
    
    // Generate code to evaluate the index expression
    generate_expression(index_node);
    
    // Save the computed index
    PUSHQ(RAX);
    
    // Get the base address of the array
    EMIT("leaq .%s(%s), %s", array_identifier_symbol->name, RIP, RCX);
    
    // Restore the index
    POPQ(RAX);
    
    // Calculate the final memory address
    EMIT("leaq (%s, %s, 8), %s", RCX, RAX, RCX);
    
    // Restore the value to store
    POPQ(RAX);
    
    // Store the value at the calculated address
    EMIT("movq %s, (%s)", RAX, RCX);
  }
}

static void generate_print_statement(node_t *statement)
{
  // Remember to call safe_printf instead of printf
  node_t *list_node = statement->children[0];

  for (size_t i = 0; i < list_node->n_children; i++)
  {
    node_t *child_node = list_node->children[i];
    if (child_node->type == STRING_LIST_REFERENCE)
    {
      // String
      EMIT("leaq strout(%s), %s", RIP, RDI);
      EMIT("leaq string%ld(%s), %s", child_node->data.string_list_index, RIP, RSI);
    }
    else
    {
      generate_expression(child_node);
      EMIT("leaq intout(%s), %s", RIP, RDI);
      MOVQ(RAX, RSI);
    }

    EMIT("call safe_printf");
  }
  // Print newline
  MOVQ("$0x0A", RDI);
  EMIT("call putchar");
}

static void generate_return_statement(node_t *statement)
{
  generate_expression(statement->children[0]);
  EMIT("jmp .%s.epilogue", current_function->name);
}

// Recursively generate the given statement node, and all sub-statements.
static void generate_statement(node_t *node)
{
  if (node == NULL)
    return;
  // The candidates are BLOCK, ASSIGNMENT_STATEMENT, PRINT_STATEMENT, RETURN_STATEMENT,
  // FUNCTION_CALL
  switch (node->type)
  {
  case ASSIGNMENT_STATEMENT:
    generate_assignment_statement(node);
    break;
  case PRINT_STATEMENT:
    generate_print_statement(node);
    break;
  case RETURN_STATEMENT:
    generate_return_statement(node);
    break;
  case FUNCTION_CALL:
    generate_function_call(node);
    break;
  default:
    break;
  }
  for (size_t i = 0; i < node->n_children; i++)
  {
    node_t *child = node->children[i];
    generate_statement(child);
  }
}

static void generate_safe_printf(void)
{
  LABEL("safe_printf");

  PUSHQ(RBP);
  MOVQ(RSP, RBP);
  // This is a bitmask that abuses how negative numbers work, to clear the last 4 bits
  // A stack pointer that is not 16-byte aligned, will be moved down to a 16-byte boundary
  ANDQ("$-16", RSP);
  EMIT("call printf");
  // Cleanup the stack back to how it was
  MOVQ(RBP, RSP);
  POPQ(RBP);
  RET;
}

// Generates the scaffolding for parsing integers from the command line, and passing them to the
// entry point of the VSL program. The VSL entry function is specified using the parameter "first".
static void generate_main(symbol_t *first)
{
  // Make the globally available main function
  LABEL("main");

  // Save old base pointer, and set new base pointer
  PUSHQ(RBP);
  MOVQ(RSP, RBP);

  // Which registers argc and argv are passed in
  const char *argc = RDI;
  const char *argv = RSI;

  const size_t expected_args = FUNC_PARAM_COUNT(first);

  SUBQ("$1", argc); // argc counts the name of the binary, so subtract that
  EMIT("cmpq $%ld, %s", expected_args, argc);
  JNE("ABORT"); // If the provdied number of arguments is not equal, go to the abort label

  if (expected_args == 0)
    goto skip_args; // No need to parse argv

  // Now we emit a loop to parse all parameters, and push them to the stack,
  // in right-to-left order

  // First move the argv pointer to the vert rightmost parameter
  EMIT("addq $%ld, %s", expected_args * 8, argv);

  // We use rcx as a counter, starting at the number of arguments
  MOVQ(argc, RCX);
  LABEL("PARSE_ARGV"); // A loop to parse all parameters
  PUSHQ(argv);         // push registers to caller save them
  PUSHQ(RCX);

  // Now call strtol to parse the argument
  EMIT("movq (%s), %s", argv, RDI); // 1st argument, the char *
  MOVQ("$0", RSI);                  // 2nd argument, a null pointer
  MOVQ("$10", RDX);                 // 3rd argument, we want base 10
  EMIT("call strtol");

  // Restore caller saved registers
  POPQ(RCX);
  POPQ(argv);
  PUSHQ(RAX); // Store the parsed argument on the stack

  SUBQ("$8", argv);        // Point to the previous char*
  EMIT("loop PARSE_ARGV"); // Loop uses RCX as a counter automatically

  // Now, pop up to 6 arguments into registers instead of stack
  for (size_t i = 0; i < expected_args && i < NUM_REGISTER_PARAMS; i++)
    POPQ(REGISTER_PARAMS[i]);

skip_args:

  EMIT("call .%s", first->name);
  MOVQ(RAX, RDI);    // Move the return value of the function into RDI
  EMIT("call exit"); // Exit with the return value as exit code

  LABEL("ABORT"); // In case of incorrect number of arguments
  EMIT("leaq errout(%s), %s", RIP, RDI);
  EMIT("call puts"); // print the errout string
  MOVQ("$1", RDI);
  EMIT("call exit"); // Exit with return code 1

  generate_safe_printf();

  // Declares global symbols we use or emit, such as main, printf and putchar
  DIRECTIVE("%s", ASM_DECLARE_SYMBOLS);
}
