#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "table.c"

static_assert(START >= 0 && START < NSTATES, "START must be a valid state");
static_assert(ACCEPT >= 0 && ACCEPT < NSTATES, "ACCEPT must be a valid state");
static_assert(ERROR >= 0 && ERROR < NSTATES, "ERROR must be a valid state");

// This program takes in lines from stdin and validates them line by line.
// If a line is valid, it is ignored.
// Otherwise, it is printed in full,
// followed by a line indicating at which character the error occurs.
int main() {

    // Fill the table first. This function does no input processing only table creation.
    fillTable();
    printf("Table filled!\n");

    // Lines may not be more than 512 lines long
    char line[512];
    int lineNum = 0;

    // For each line in input
    while (fgets(line, sizeof(line), stdin)) {
        lineNum++;

        int state = START;
        int pos = 0;
        bool fatalError = false;
        while (true) {
            unsigned char c = line[pos];

            // If the line ended without a newline, patch it in
            if (c == '\0')
                c = '\n';

            // Use the symbol to go to the next state
            state = table[state][c];

            // Uncomment this for debugging:
            /* printf("State %i got c=%c\n", state, c); */

            if (state < 0 || state >= NSTATES) {
                printf("fatal error: DFS entered an invalid state: %d\n", state);
                fatalError = true;
            }
            else if (state == ACCEPT) {
                // Make sure statement is accepted at new line
                if (c == '\n') {
                    printf("line %4d: accepted: %.*s\n", lineNum, pos, line);

                    // Break inner loop
                    break;
                }

                printf("fatal error: ACCEPT state reached before end of line:\n");
                fatalError = true;
            }
            else if (c == '\n' && state != ERROR) {
                printf("fatal error: Line ended without reaching either ACCEPT or ERROR state: %d\n", state);
                fatalError = true;
            }

            // Provide the position we were at in the string when the DFS reached the ERROR state,
            // or when a fatal error occured. The latter can only happen if the DFS is broken.
            if (state == ERROR || fatalError) {
                printf("line %4d: error: %s", lineNum, line);
                printf("~~~~~~~~~~~~~~~~~~");
                for(int i = 0; i < pos; i++)
                    putc('~', stdout);
                printf("^\n");
                break;
            }

            pos++;
        }

        // Fatal errors means there is something wrong with the DFS table. Stop processing lines!
        if (fatalError) {
            return 1; // return code 1 means error
        }
    }

    return 0;
}
