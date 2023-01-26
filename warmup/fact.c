#include "common.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//for valid input n
//recursion to calculate factorial of a positive integer value
int factorial(int n){
    if (n>=1) {
        return n*factorial(n-1);
    } else {
        return 1;
    }
}

int main(int argc, char const **argv) {
    
    //if it's not exactly one argument, e.g. no arguments or more than one argument
    if (argc != 2) {
        printf("Huh?\n");
        return 0;
    }
  
    bool isInteger = true;
    
    //check every elements in the first argument
    //if one of them is not an integer, then this argument is not an integer
    for (int i = 0; i < strlen(argv[1]); i++) {
        if (!isdigit(argv[1][i]))
            isInteger = false;
    }
    
    //if the argument is not a positive integer, print "Huh"
    if (!isInteger) {
        printf("Huh?\n");
        return 0;
    }
    
    //convert the argv[1] to actual integer number
    char* remaining;
    int number = strtol(argv[1], &remaining, 10);
    
    //if exceed 12, print "Overflow"
    if(number>12) {
        printf("Overflow\n");
        return 0;
    }
    
    //if it is 0, print "huh"
    if(number==0) {
        printf("Huh?\n");
        return 0;
    }
    
    //if it's between 1 to 12, print its factorial
    printf("%d\n", factorial(number));

    return 0;
}
