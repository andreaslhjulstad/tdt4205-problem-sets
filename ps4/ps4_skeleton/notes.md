# Recitation lecture notes

## Symbol table

- Have a symbol table and a corresponding hashmap used for look-up
- Go through the entire syntax tree and add all globals as global symbols in the symbol table
- Functions belong to the global symbol table, and also has its own local symbol table
  - Symbols with type function have a field function_symtable that is a pointer to a local symbol table, which stores parameters and local variables (and only these)
  - Each local symbol table has its own hashmap (or multiple). Managing which hashmap to use will be the most difficult part of the exercise
  - We get chained lists of hashmaps
  - Each nested map has a pointer to a "backup" hashmap, which is used in case the variable identifier can't be found in the current one
  - When assigning a value, put it in the topmost hashmap
  - Once a scope is finished, its hashmap must be destroyed and the backup will be restored
  - Lecturer recommends: when creating a local symbol table, set its backup to be the hashmap of the global symbol table

## Global string list

- Go through the syntax tree and "steal" the data from the string and put it into a dynamic global string list
- Replace the string in the syntax tree with a string list node

## Printing symbol tables

- Should implement printing of symbol tables
- Look at the print syntax tree function or print symbol table

## Hints

- Hints in symbols.c
