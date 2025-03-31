%{
#include <vslc.h>
// The tokens defined in parser.y
#include "parser.h"

// parser.h contains some unused functions, ignore that
#pragma GCC diagnostic ignored "-Wunused-function"
%}
%option noyywrap
%option array
%option yylineno

WHITESPACE [\ \t\v\r\n]
COMMENT \/\/[^\n]+
QUOTED \"([^\"\n]|\\\")*\"
NUMBER [0-9]+

FUNC func
VAR var
RETURN return
PRINT print
IF if
THEN then
ELSE else
WHILE while
DO do
BREAK break

IDENTIFIER [_a-zA-Z][_a-zA-Z0-9]*


%%
{WHITESPACE}+           { /* Eliminate whitespace */ }
{COMMENT}               { /* Eliminate comments */ }
{QUOTED}                { return STRING_TOKEN; }
{NUMBER}                { return NUMBER_TOKEN; }

{FUNC}                  { return FUNC; }
{VAR}                   { return VAR; }
{RETURN}                { return RETURN; }
{PRINT}                 { return PRINT; }
{IF}                    { return IF; }
{THEN}                  { return THEN; }
{ELSE}                  { return ELSE; }
{WHILE}                 { return WHILE; }
{DO}                    { return DO; }
{BREAK}                 { return BREAK; }

{IDENTIFIER}            { return IDENTIFIER_TOKEN; }

  /* Unknown chars get returned as single char tokens */
.                       { return yytext[0]; }
%%