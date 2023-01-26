#include "common.h"

int main(int argc, char **argv) {
    
    //print out the words from the command line on different lines
    //start from argv[1] since argv[0] would be the program's name
    for(int i=1; i<argc; i++) {
        printf("%s\n", argv[i]);
    }
    
    return 0;
}
