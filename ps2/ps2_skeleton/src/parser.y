%{
#include "vslc.h"

// State variables from the flex generated scanner
extern int yylineno;  // The line currently being read
extern char yytext[]; // The text of the last consumed lexeme

// The main flex driver function used by the parser
int yylex(void);

// The function called by the parser when errors occur
int yyerror(const char *error)
{
  fprintf(stderr, "%s on line %d\n", error, yylineno);
  exit(EXIT_FAILURE);
}

// Feel free to define #define macros if you want to
%}

%token FUNC PRINT RETURN BREAK IF THEN ELSE WHILE DO VAR
%token NUMBER_TOKEN IDENTIFIER_TOKEN STRING_TOKEN

// Use operator precedence to ensure order of operations is correct
%left '=' '!'
%left '<' '>'
%left '+' '-'
%left '*' '/'
%right UNARY_OPERATORS

// Resolve the nested if-if-else ambiguity with precedence
%nonassoc IF THEN
%nonassoc ELSE

%%
program :
      global_list { root = $1; }
    ;
global_list :
      global { $$ = node_create(LIST, 1, $1); }
    | global_list global { $$ = append_to_list_node($1, $2); }
    ;
global :
      function { $$ = $1; }
    |  global_declaration { $$ = $1; }
    ;
global_declaration :
      VAR global_variable_list { $$ = node_create(GLOBAL_DECLARATION, 1, $2); }
    ;
global_variable_list :
      global_variable { $$ = node_create(LIST, 1, $1); }
    | global_variable_list ',' global_variable { $$ = append_to_list_node($1, $3); }
    ;
global_variable :
      identifier { $$ = $1; }
    | array_indexing { $$ = $1; }
    ;
array_indexing :
      identifier '[' expression ']' { $$ = node_create(ARRAY_INDEXING, 2, $1, $3); }
    ;
variable_list :
      identifier { $$ = node_create(LIST, 1, $1); }
    | variable_list ',' identifier { $$ = append_to_list_node($1, $3); }
    ;
local_declaration :
      VAR variable_list { $$ = $2; }
    ;
local_declaration_list :
      local_declaration { $$ = node_create(LIST, 1, $1); }
    | local_declaration_list local_declaration { $$ = append_to_list_node($1, $2); }
    ;
parameter_list :
      variable_list { $$ = $1; }
    | %empty { $$ = node_create(LIST, 0); }
    ;
function :
      FUNC identifier '(' parameter_list ')' statement { $$ = node_create(FUNCTION, 3, $2, $4, $6); }
    ;
statement :
      assignment_statement { $$ = $1; }
    | return_statement { $$ = $1; }
    | print_statement { $$ = $1; }
    | if_statement { $$ = $1; }
    | while_statement { $$ = $1; }
    | break_statement { $$ = $1; }
    | function_call { $$ = $1; }
    | block { $$ = $1; }
    ;
block :
     '{' local_declaration_list statement_list '}' { $$ = node_create(BLOCK, 2, $2, $3); }
    |'{' statement_list '}' { $$ = node_create(BLOCK, 1, $2); }
    ;
statement_list :
      statement { $$ = node_create(LIST, 1, $1); }
    | statement_list statement { $$ = append_to_list_node($1, $2); }
    ;
assignment_statement :
      identifier '=' expression { $$ = node_create(ASSIGNMENT_STATEMENT, 2, $1, $3); }
    | array_indexing '=' expression { $$ = node_create(ASSIGNMENT_STATEMENT, 2, $1, $3); }
    ;
return_statement :
      RETURN expression { $$ = node_create(RETURN_STATEMENT, 1, $2); }
    ;
print_statement :
      PRINT print_list { $$ = node_create(PRINT_STATEMENT, 1, $2); }
    ;
print_list :
      print_item { $$ = node_create(LIST, 1, $1); }
    | print_list ',' print_item { $$ = append_to_list_node($1, $3); }
    ;
print_item :
      expression { $$ = $1; }
    | string { $$ = $1; }
    ;
break_statement :
      BREAK { $$ = node_create(BREAK_STATEMENT, 0); }
    ;
if_statement :
      IF expression THEN statement { $$ = node_create(IF_STATEMENT, 2, $2, $4); }
    | IF expression THEN statement ELSE statement { $$ = node_create(IF_STATEMENT, 3, $2, $4, $6); }
    ;
while_statement :
      WHILE expression DO statement { $$ = node_create(WHILE_STATEMENT, 2, $2, $4); }
    ;
expression :
      expression '=' '=' expression 
      { 
        $$ = node_create(OPERATOR, 2, $1, $4);
        $$->data.operator = "==";
      }
    | expression '!' '=' expression       
      { 
        $$ = node_create(OPERATOR, 2, $1, $4);
        $$->data.operator = "!=";
      }
    | expression '<' expression 
      { 
        $$ = node_create(OPERATOR, 2, $1, $3);
        $$->data.operator = "<";
      }
    | expression '<' '=' expression
      { 
        $$ = node_create(OPERATOR, 2, $1, $4);
        $$->data.operator = "<=";
      }
    | expression '>' expression
      { 
        $$ = node_create(OPERATOR, 2, $1, $3);
        $$->data.operator = ">";
      }
    | expression '>' '=' expression 
      { 
        $$ = node_create(OPERATOR, 2, $1, $4);
        $$->data.operator = ">=";
      }
    | expression '+' expression
      { 
        $$ = node_create(OPERATOR, 2, $1, $3);
        $$->data.operator = "+";
      }
    | expression '-' expression
      { 
        $$ = node_create(OPERATOR, 2, $1, $3);
        $$->data.operator = "-";
      }
    | expression '*' expression
      { 
        $$ = node_create(OPERATOR, 2, $1, $3);
        $$->data.operator = "*";
      }
    | expression '/' expression
      { 
        $$ = node_create(OPERATOR, 2, $1, $3);
        $$->data.operator = "/";
      }
    | '-' expression
      { 
        $$ = node_create(OPERATOR, 1, $2);
        $$->data.operator = "-";
      }
    %prec UNARY_OPERATORS
    | '!' expression
      { 
        $$ = node_create(OPERATOR, 1, $2);
        $$->data.operator = "!";
      }
    %prec UNARY_OPERATORS
    | '(' expression ')' { $$ = $2; }
    | number { $$ = $1; }
    | identifier { $$ = $1; }
    | array_indexing { $$ = $1; }
    | function_call { $$ = $1; }
    ;
function_call :
      identifier '(' argument_list ')' { $$ = node_create(FUNCTION_CALL, 2, $1, $3); }
    ;
argument_list :
      expression_list { $$ = $1; }
    | %empty { $$ = node_create(LIST, 0); }
    ;
expression_list :
      expression { $$ = node_create(LIST, 1, $1); }
    | expression_list ',' expression { $$ = append_to_list_node($1, $3); }
    ;
identifier :
      IDENTIFIER_TOKEN
      {
        // Create a node with 0 children to represent the identifier
        $$ = node_create(IDENTIFIER, 0);
        // Allocate a copy of yytext to keep in the syntax tree as data
        $$->data.identifier = strdup(yytext);
      }
    ;
number :
      NUMBER_TOKEN 
      {
        $$ = node_create(NUMBER_LITERAL, 0);
        $$->data.number_literal = (int64_t) atoll(yytext); 
      }
    ;
string :
      STRING_TOKEN
      {
        $$ = node_create(STRING_LITERAL, 0);
        $$->data.string_literal = strdup(yytext);
      }
    ;
%%
