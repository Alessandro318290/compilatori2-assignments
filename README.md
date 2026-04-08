# compilatori2-assignments
Repo per gli assignments di compilatori2

## Struttura delle directory

```text
root/
├── CMakeLists.txt             
├── common/                # Funzioni comuni che i test chiamano
│   ├── common.cpp           
│   └── common.hpp 
├── Assignment1/
│   ├── OptzA/             # Sorgenti ottimizzazione A
│   │   ├── Pass.cpp
│   │   ├── CMakeLists.txt
│   │   └── tests/         # TEST SPECIFICI per questo passo
│   │       ├── optaA.c  
│   │       └── optaB.ll
│   └── OptzB/             # Sorgenti ottimizzazione B
│       ├── Pass.cpp
│       └── CMakeLists.txt
├── tests/                
│   ├── source/            # .c file
│   └── IR/                # .ll o .bc                                     
└── build/                 # Unica cartella di build
    └── LLVMAssignments.so # Un'unica libreria contenente TUTTI i pass
```
