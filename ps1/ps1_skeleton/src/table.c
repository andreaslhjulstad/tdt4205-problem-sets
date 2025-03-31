
// The number of states in your table
#define NSTATES 14

// The starting state, at the beginning of each line
#define START 0

// The state to go to after a valid line
// All lines end with the newline character '\n'
#define ACCEPT 12

// The state to jump to as soon as a line is invalid
#define ERROR 13

int table[NSTATES][256];

void fillTable() {

    // Make all states lead to ERROR by default
    for (int i = 0; i < NSTATES; i++) {
        for (int c = 0; c < 256; c++) {
            table[i][c] = ERROR;
        }
    }

    // Skip whitespace
    table[START][' '] = START;

    // If we reach a newline, and are not in the middle of a statement, accept
    table[START]['\n'] = ACCEPT;

    // Accept the statement "go"
    table[START]['g'] = 1;
    table[1]['o'] = 2;
    table[2]['\n'] = ACCEPT;


    // 2.2 Multiple "go"-s
    table[2][' '] = START;
    
    // 2.3 (dx, dy)=<number>
    table[START]['d'] = 3;
    table[3]['x'] = 4;
    table[3]['y'] = 4;
    table[4]['='] = 5;

    table[5]['-'] = 6;
    
    for (char c = '0'; c <= '9'; c++) {
        table[5][c] = 7;
        table[6][c] = 7;
        table[7][c] = 7;
    }

    table[7]['\n'] = ACCEPT;
    table[7][' '] = START;

    // 2.4 Labels
    for (char c = '0'; c <= '9'; c++) {
        table[START][c] = 8;
        table[8][c] = 8;
    }

    table[8][':'] = 9;
    table[9][' '] = START;

    // 2.5 Comments
    table[START]['/'] = 10;
    table[10]['/'] = 11;

    for (int c = 0; c < 255; c++) {
        table[11][c] = 11;
    }
    table[11]['\n'] = ACCEPT;
}
