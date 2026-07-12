#include <stdio.h>

/*
void not_adjacent_loops(int x) {
    int i;

    // Loop non adiacenti
    for (i = 0; i < 100; ++i) {
    }

    if (x > 19)
        printf("Test 1"); 
    else
        printf("Test 2");

    for (i = 0; i < 100; ++i) {
    }
    
}
*/

/*
void simple_adjacent_loops() {
    int i;
    
    for (i = 0; i < 10; ++i) {
    }
    
    for (i = 0; i < 10; ++i) {
    }
}
*/

/*
void different_trip_count() {
    int A[10], B[20];
    
    // ---------- Loop 1 ----------
    for (int i = 0; i < 10; i++) {  // Trip count = 10
        A[i] = i;                  // Scrittura su A
    }                   
        
    // ---------- Loop 2 ----------
    for (int i = 0; i < 20; i++) {  // Trip count diverso
        B[i] = i;                 // Scrittura su B
    }
}
*/
            
/*
// Loop 1 Dom      Loop 2
// Loop 2 Not PDom Loop1
// Nonostante la CFE sia violata, l'algoritmo termina a causa della non adiancenza 
void not_CFE(int x) {
    int D[10];

    // Loop 1
    for (int i = 0; i < 10; i++) {
        D[i] = i;
    }

    if (x % 2 == 0)
        return;

    // Loop2
    for (int i = 0; i < 10; i++) {
        D[i] += 1;
    }
}
*/


void read_after_write(int* restrict A, int* restrict B) {
    int i;

    for (i = 0; i < 10; i++){
        A[i] = i;
    }

    for (i = 0; i < 10; i++) {
        B[i] = A[i];
    }
}


/*
void read_after_write_sameIVButDifferentSymbol(int* restrict A, int* restrict B) {
    int i,j;

    for (i = 0; i < 10; i++){
        A[i] = i;
    }

    for (j = 0; j < 10; j++) {
        B[j] = A[j];
    }
}
*/

/*
void read_after_write_negative(int* restrict A, int* restrict B) {
    int i;

    for (i = 0; i < 10; i++){
        A[i] = i;
    }

    for (i = 0; i < 10; i++) {
        B[i] = A[i+1];
    }
}
*/

/*
void read_after_write_positive(int* restrict A, int* restrict B) {
    int i;

    for (i = 0; i < 10; i++) {
        A[i + 1] = i * 10; 
    }

    for (i = 0; i < 10; i++) {
        B[i] = A[i]; 
    }
}
*/

/*
void read_after_write_multiple_loop(int* restrict A, int* restrict B) {
    int i;

    for (i = 0; i < 10; i++){
        A[i] = i;
    }

    for (i = 0; i < 10; i++) {
        B[i] = A[i];
    }

    for (i = 0; i < 10; i++) {
        B[i] = A[i];
    }
}
*/

/*
void read_after_write_subloop(int* restrict A, int* restrict B) {
    int i;

    for (i = 0; i < 10; i++) {
        int j;

        for (j = 0; j < 10; j++)
        {
            A[j] = j;
        }

        for (j = 0; j < 10; j++) {
            B[j] = A[j];
        }
    }
}
*/