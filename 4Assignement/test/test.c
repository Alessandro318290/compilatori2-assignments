#include <stdio.h>



void test_non_fusible() {
    int A[10], B[10];

    // ---------- Loop 1 ----------
    for (int i = 0; i < 10; i++) {  // Trip count = 10
        A[i] = i;                  // Scrittura su A
    }

    // Istruzione intermedia che rompe adiacenza
    int x = 42;                    

    // ---------- Loop 2 ----------
    for (int i = 0; i < 20; i++) {  // Trip count diverso
        B[i] = i*2;                 // Scrittura su B
    }
}

void test_non_fusible_dependency() {
    int C[10];

    // Loop 1
    for (int i = 0; i < 10; i++) {
        C[i] = i;                 // store
    }

    // Loop 2
    for (int i = 0; i < 10; i++) {
        C[i] += 1;                // load + store -> dipendenza
    }
}

void test_non_fusible_conditional() {
    int D[10];

    // Loop 1
    for (int i = 0; i < 10; i++) {
        D[i] = i;
    }

    if (D[0] > 0) {               // ramo condizionale rompe dominanza/post-dominanza
        for (int i = 0; i < 10; i++) {
            D[i] += 1;
        }
    }
}

void four_adjacent_loops() {
    int i;

        for (i = 0; i < 100; ++i) {
        }

        i = 0; /*due loop non adiacenti*/

        for (i = 0; i < 100; ++i) {
        }

    /* Loop 3 (unguarded) */
    for (i = 0; i < 10; ++i) {
    }

    /* Loop 4 (unguarded) */
    for (i = 0; i < 10; ++i) {
    }

    i = 0;
    int i2 = 0;
    do{
      i++;
    } while(i<10);
    
    do{
      i2++;
    }while(i2<10);
}
