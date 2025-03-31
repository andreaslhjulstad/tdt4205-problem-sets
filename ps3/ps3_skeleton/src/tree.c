#include "vslc.h"

// Global root for abstract syntax tree
node_t *root;

// Declarations of helper functions defined further down in this file
static void node_print(node_t *node, int nesting);
static node_t *constant_fold_subtree(node_t *node);
static bool remove_unreachable_code(node_t *node);
static void destroy_subtree(node_t *discard);

// Initialize a node with the given type and children
node_t *node_create(node_type_t type, size_t n_children, ...)
{
  node_t *result = malloc(sizeof(node_t));

  // Initialize every field in the struct
  *result = (node_t){
      .type = type,
      .n_children = n_children,
      .children = malloc(n_children * sizeof(node_t *)),
  };

  // Read each child node from the va_list
  va_list child_list;
  va_start(child_list, n_children);
  for (size_t i = 0; i < n_children; i++)
  {
    result->children[i] = va_arg(child_list, node_t *);
  }
  va_end(child_list);

  return result;
}

// Append an element to the given LIST node, returns the list node
node_t *append_to_list_node(node_t *list_node, node_t *element)
{
  assert(list_node->type == LIST);

  // Calculate the minimum size of the new allocation
  size_t min_allocation_size = list_node->n_children + 1;

  // Round up to the next power of two
  size_t new_allocation_size = 1;
  while (new_allocation_size < min_allocation_size)
    new_allocation_size *= 2;

  // Resize the allocation
  list_node->children = realloc(list_node->children, new_allocation_size * sizeof(node_t *));

  // Insert the new element and increase child count by 1
  list_node->children[list_node->n_children] = element;
  list_node->n_children++;

  return list_node;
}

// Outputs the entire syntax tree to the terminal
void print_syntax_tree(void)
{
  // If the environment variable GRAPHVIZ_OUTPUT is set, print a GraphViz graph in the dot format
  if (getenv("GRAPHVIZ_OUTPUT") != NULL)
    graphviz_node_print(root);
  else
    node_print(root, 0);
}

// Performs constant folding and removes unconditional conditional branches
void constant_fold_syntax_tree(void)
{
  root = constant_fold_subtree(root);
}

// Removes code that is never reached due to return and break statements.
// Also ensures execution never reaches the end of a function without reaching a return statement.
void remove_unreachable_code_syntax_tree(void)
{
  for (size_t i = 0; i < root->n_children; i++)
  {
    node_t *child = root->children[i];
    if (child->type != FUNCTION)
      continue;

    node_t *function_body = child->children[2];

    bool has_return = remove_unreachable_code(function_body);

    // If the function body is not guaranteed to call return, we wrap it in a BLOCK like so:
    // {
    //   original_function_body
    //   return 0
    // }
    if (!has_return)
    {
      node_t *zero_node = node_create(NUMBER_LITERAL, 0);
      zero_node->data.number_literal = 0;
      node_t *return_node = node_create(RETURN_STATEMENT, 1, zero_node);
      node_t *statement_list = node_create(LIST, 2, function_body, return_node);
      node_t *new_function_body = node_create(BLOCK, 1, statement_list);
      child->children[2] = new_function_body;
    }
  }
}

// Frees all memory held by the syntax tree
void destroy_syntax_tree(void)
{
  destroy_subtree(root);
  root = NULL;
}

// The rest of this file contains private helper functions used by the above functions

// Prints out the given node and all its children recursively
static void node_print(node_t *node, int nesting)
{
  // Indent the line based on how deep the node is in the syntax tree
  printf("%*s", nesting, "");

  if (node == NULL)
  {
    printf("(NULL)\n");
    return;
  }

  printf("%s", NODE_TYPE_NAMES[node->type]);

  // For nodes with extra data, include it in the printout
  switch (node->type)
  {
  case OPERATOR:
    printf(" (%s)", node->data.operator);
    break;
  case IDENTIFIER:
    printf(" (%s)", node->data.identifier);
    break;
  case NUMBER_LITERAL:
    printf(" (%ld)", node->data.number_literal);
    break;
  case STRING_LITERAL:
    printf(" (%s)", node->data.string_literal);
    break;
  case STRING_LIST_REFERENCE:
    printf(" (%zu)", node->data.string_list_index);
    break;
  default:
    break;
  }

  putchar('\n');

  // Recursively print children, with some more indentation
  for (size_t i = 0; i < node->n_children; i++)
    node_print(node->children[i], nesting + 1);
}

typedef int (*operation_func)(int value1, int value2);

int64_t add(int a, int b) { return a + b; }
int64_t subtract(int a, int b) { return a - b; }
int64_t multiply(int a, int b) { return a * b; }
int64_t divide(int a, int b) { return a / b; }
int64_t equal(int a, int b) { return a == b; }
int64_t not_equal(int a, int b) { return a != b; }
int64_t less_than(int a, int b) { return a < b; }
int64_t less_than_or_equal(int a, int b) { return a <= b; }
int64_t greater_than(int a, int b) { return a > b; }
int64_t greater_than_or_equal(int a, int b) { return a >= b; }
int64_t negate(int a, int _) { return -a; }
int64_t not(int a, int _) { return !a; }

typedef struct
{
  const char *operator;
  int n_operands;
  operation_func func;
} operator_mapping_t;

operator_mapping_t operator_to_func_table[] = {
    {"+", 2, add},
    {"-", 2, subtract},
    {"*", 2, multiply},
    {"/", 2, divide},
    {"==", 2, equal},
    {"!=", 2, not_equal},
    {"<", 2, less_than},
    {"<=", 2, less_than_or_equal},
    {">", 2, greater_than},
    {">=", 2, greater_than_or_equal},
    {"-", 1, negate},
    {"!", 1, not}};

operation_func get_operator_func(const char *operator, int n_operands)
{
  int length = sizeof(operator_to_func_table) / sizeof(operator_mapping_t);
  for (int i = 0; i < length; i++)
  {
    operator_mapping_t operator_mapping = operator_to_func_table[i];
    if (strcmp(operator_mapping.operator, operator) == 0 && operator_mapping.n_operands == n_operands)
    {
      return operator_mapping.func;
    }
  }

  return NULL;
}

// Constant folds the given OPERATOR node, if all children are NUMBER_LITERAL
static node_t *constant_fold_operator(node_t *node)
{
  assert(node->type == OPERATOR);
  int n_operands = node->n_children;

  for (int i = 0; i < n_operands; i++)
  {
    if (node->children[i]->type != NUMBER_LITERAL)
      return node;
  }

  const char *operator= node->data.operator;
  int64_t value1 = node->children[0]->data.number_literal;
  int64_t value2 = 0;
  if (n_operands > 1)
  {
    value2 = node->children[1]->data.number_literal;
  }

  operation_func func = get_operator_func(operator, n_operands);

  if (func == NULL)
  {
    // Invalid operand
    return node;
  }

  // Calculate new value by calling the function
  int64_t new_value = func(value1, value2);

  // Remove the old subtree
  destroy_subtree(node);

  // Create new number literal node and set value
  node_t *new_node = node_create(NUMBER_LITERAL, 0);
  new_node->data.number_literal = new_value;

  return new_node;
}

// If the condition of the given if node is a NUMBER_LITERAL, the if is replaced by the taken
// branch. If the if condition is false, and the if has no else-body, NULL is returned.
static node_t *constant_fold_if(node_t *node)
{
  assert(node->type == IF_STATEMENT);
  node_t *condition_node = node->children[0];
  if (condition_node->type != NUMBER_LITERAL)
  {
    return node;
  }

  // We know that the condition is a number, retrieve its value
  int64_t condition = condition_node->data.number_literal;

  // Initialize new node pointer
  node_t *new_node;
  if (condition != 0)
  {
    // if-route is taken, return then statement
    new_node = node->children[1];
    // Detach
    node->children[1] = NULL;
  }
  else
  {
    // not taken (else)
    if (node->n_children < 3)
    {
      // Node has no else-statement
      new_node = NULL;
    }
    else
    {
      // Return else statement
      new_node = node->children[2];
      // Detach
      node->children[2] = NULL;
    }
  }

  destroy_subtree(node);

  return new_node;
}

// If the condition of the given while node is a NUMBER_LITERAL, and it is false (0),
// we remove the entire while node and return NULL instead.
// Loops that look like while(true) { ... } are kept as is. They may have a break inside
static node_t *constant_fold_while(node_t *node)
{
  assert(node->type == WHILE_STATEMENT);

  node_t *condition_node = node->children[0];
  if (condition_node->type != NUMBER_LITERAL)
  {
    return node;
  }

  // We know that the condition is a number, retrieve its value
  int64_t condition = condition_node->data.number_literal;

  if (condition == 0)
  {
    destroy_subtree(node);
    return NULL;
  }

  return node;
}

// Does constant folding on the subtreee rooted at the given node.
// Returns the root of the new subtree.
// Any node that is detached from the tree by this operation must be freed, to avoid memory leaks.
static node_t *constant_fold_subtree(node_t *node)
{
  if (node == NULL)
    return node;

  for (size_t i = 0; i < node->n_children; i++)
  {
    node_t *child = node->children[i];
    if (child != NULL)
    {
      node_t *folded_child = constant_fold_subtree(child);
      node->children[i] = folded_child;
    }
  }

  switch (node->type)
  {
  case OPERATOR:
    return constant_fold_operator(node);
    break;
  case IF_STATEMENT:
    return constant_fold_if(node);
    break;
  case WHILE_STATEMENT:
    return constant_fold_while(node);
    break;
  default:
    return node;
  }
}

// Operates on the statement given as node, and any sub-statements it may have.
// Returns true if execution of the given statement is guaranteed to interrupt execution
// through either a return statement or a break statement.
// When node is a BLOCK, any statements that come after such an interrupting statement are removed.
static bool remove_unreachable_code(node_t *node)
{
  if (node == NULL)
    return false;

  switch (node->type)
  {
  case RETURN_STATEMENT:
  case BREAK_STATEMENT:
    return true;
  case IF_STATEMENT:
  {
    if (node->n_children == 2)
    {
      // If the if only has a then-statement, it can not terminate execution
      remove_unreachable_code(node->children[1]);
      return false;
    }
    else
    {
      // If both the then-statement and the else-statement are interrupted
      // we know that the if itself is interrupting as well
      bool then_interrupts = remove_unreachable_code(node->children[1]);
      bool else_interrupts = remove_unreachable_code(node->children[2]);
      return then_interrupts && else_interrupts;
    }
  }
  case WHILE_STATEMENT:
  {
    // Even if the body of the while contains interrupting statements,
    // that is not a guarantee that the code after the while is unreachable.
    // The while may never be entered, for example, or the interrupting statement may be BREAK.
    remove_unreachable_code(node->children[1]);
    return false;
  }
  case BLOCK:
  {
    // The list of statements in a BLOCK is always the last child node
    node_t *statement_list = node->children[node->n_children - 1];

    int no_of_statements = statement_list->n_children;

    for (int i = 0; i < no_of_statements; i++)
    {
      node_t *statement = statement_list->children[i];
      if (remove_unreachable_code(statement))
      {
        statement_list->n_children = i + 1;
        for (int j = i + 1; j < no_of_statements; j++)
        {
          destroy_subtree(statement_list->children[j]);
        }

        return true;
      }
    }

    // If we get here, none of the statements in the block are interrupting.
    return false;
  }
  default:
    return false;
  }
}

// Frees the memory owned by the given node, but does not touch its children
static void node_finalize(node_t *discard)
{
  if (discard == NULL)
    return;

  // Only free data if the data field is owned by the node
  switch (discard->type)
  {
  case IDENTIFIER:
    free(discard->data.identifier);
    break;
  case STRING_LITERAL:
    free(discard->data.string_literal);
    break;
  default:
    break;
  }
  free(discard->children);
  free(discard);
}

// Recursively frees the memory owned by the given node, and all its children
static void destroy_subtree(node_t *discard)
{
  if (discard == NULL)
    return;

  for (size_t i = 0; i < discard->n_children; i++)
    destroy_subtree(discard->children[i]);
  node_finalize(discard);
}

// Definition of the global string array NODE_TYPE_NAMES
const char *NODE_TYPE_NAMES[NODE_TYPE_COUNT] = {
#define NODE_TYPE(node_type) #node_type
#include "nodetypes.h"
};
