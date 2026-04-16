# compilatori2-assignments
Repo per gli assignments di compilatori2

## Struttura delle directory

```text
root/        
├── common/                # Funzioni comuni che i test chiamano
│   ├── common.cpp           
│   └── common.hpp 
├── 1Assignment/
│   ├── OptzA/             # Ottimizzazione A
│   │   ├── build/
│   │   ├── src/
│   │   │   ├── Pass.cpp
│   │   │   └── CMakeLists.txt
│   │   └── test/         # TEST SPECIFICI per questo passo
│   │       ├── optaA.c  
│   │       └── optaA.ll
│   └── OptzB/             # Ottimizzazione B
│       ├── ...
├── test/                
│   ├── test.c  
│   └── test.ll
```
