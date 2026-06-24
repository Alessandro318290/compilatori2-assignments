#include <stdio.h>

int main(int b) {
    int a, c, d, e;

    // Operazioni
    a = b + 1;
    c = a - 1; // Valida per l'ottimizzazione: Operazioni inverse e operandi costanti uguali
    d = a + 1; // Operazione non inversa
    e = a - 2; // Costante diversa
    printf("%d, %d, %d", c, d, e);

    return 0;
} 
