# Introduzione

L'assignment in questione richiede l'implementazione del passo di ottimizzazione denominato **loop invariant code motion**.   

>Questa ottimizzazione analizza tutte le istruzioni dei loop e verifica quali di queste sono **loop-invariant**, per poi applicarvi lo step di **code motion** in base alla valutazione delle rispettive condizioni. Le istruzioni che risulteranno quindi compatibili verranno trasferite dal loro blocco di partenza al blocco di **preheader** del rispettivo loop.

---
# Condizioni iniziali

![Immagine 1](assets/Pasted%20image%2020260517122251.png)

Si parte con la definizione di due oggetti fondamentali per l'algoritmo:

- Oggetto `LoopInfo`  utilizzato per iterare su tutti i loop esterni del programma
- Oggetto `DominatorTree` impiegato per valutare le condizioni per la code motion

Inoltre, se il programma in analisi non contiene iterazioni, si esce dal passo di ottimizzazione preservando le analisi precedentemente effettuate.

Per la medesima ragione, è necessario tracciare se sia avvenuta almeno una trasformazione ( code motion ) all'interno della gerarchia dei cicli.
Per evitare l'uso di variabili globali libere e al contempo per rendere le funzioni più indipendenti dal contesto, si è scelto di adottare un approccio funzionale basato sulla propagazione del valore di ritorno.

In conclusione si procede a iterare su ogni ciclo esterno del programma richiamandovi la funzione principale di questo passo di ottimizzazione, la funzione `bool visitAllLoopsBottonUp(Loop *loop, DominatorTree &DT)`

---
# Visita di tutti i cicli del programma - `visitAllLoopsBottonUp`

![Immagine 2](assets/Pasted%20image%2020260517122316.png)

Per visitare tutti i loop del programma non basta iterare solo sull'oggetto `LoopInfo` poiché questo ritorna soltanto i loop esterni. Un loop infatti può a sua volta contenere altri loop e così via.

La funzione `visitAllLoopsBottonUp` risolve esattamente questo problema. Per ogni loop si controlla se ne esistano alcuni di annidati altrimenti si procede esaminando il loop attuale.

```c
for (Loop *SubLoop : *loop) {
  changed |= visitAllLoopsBottonUp(SubLoop, DT);
}
```

Se si osserva la struttura del ciclo ricorsivo si può osservare come si analizzino prima i loop più interni, ed infine quello più esterno.

Questa strategia di visita ricorsiva bottom-up è motivata dal fatto che, per applicare correttamente la LICM su una gerarchia di cicli annidati, le **trasformazioni da parte delle code motion devono propagarsi dal basso verso l'alto.**

```c
while (cond_1) {            // Loop Esterno (Livello 1)
    while (cond_1_1) {       // Loop Intermedio (Livello 2)
        while (cond_1_1_1) { // Loop Interno (Livello 3)
            // ... codice ...
        }
    }
}
```

Se un'istruzione si trovasse nel ciclo più interno (`cond_1_1_1`) e fosse *loop-invariant* rispetto a tutti e tre i livelli della gerarchia, il processo di ottimizzazione deve avvenire in modo sequenziale:

1. **Fase 1 (Livello 3):** L'analisi individua l'istruzione come loop-invariant nel ciclo `cond_1_1_1` e la sposta nel suo _preheader_.
    
2. **Fase 2 (Livello 2):** Il preheader del livello 3 si trova fisicamente all'interno del corpo del livello 2 (`cond_1_1`). Quando la visita ricorsiva risale al livello 2, l'istruzione viene analizzata nuovamente. Se risulta invariante anche rispetto a `cond_1_1`, viene sollevata nel _preheader_ del livello 2.
    
3. **Fase 3 (Livello 1):** Infine, la visita raggiunge il loop più esterno. L'istruzione viene esaminata un'ultima volta e, se indipendente da `cond_1`, viene definitivamente spostata fuori dall'intera struttura, nel _preheader_ del Livello 1.

Se si provasse ad utilizzare una strategia di tipo *top-down*, ovvero esaminando prima il ciclo più esterno e poi quelli più interni, l'algoritmo fallirebbe a causa di una violazione della semantica del flusso di controllo.

Poiché il metodo `loop->getBlocks()` del ciclo esterno restituisce la lista di tutti i basic block interni (compresi quelli dei sotto-cicli), un approccio top-down intercetterebbe l'istruzione del livello 3 quando si sta ancora analizzando il *livello 1*. Di conseguenza si tenterebbe di spostare l'istruzione nel preheader del loop più esterno.

Questa casistica rappresenta un'importante **violazione semantica** poiché significherebbe **far eseguire un'istruzione ancora prima di verificare se le condizioni dei cicli intermedi siano valide, eseguendo codice potenzialmente mai raggiungibile nel programma.**

---
# Progettazione dell'algoritmo per la LICM

Proseguendo con l'esecuzione del passo di ottimizzazione si entra nel vivo dell'algoritmo. 
L'obiettivo in questione, come citato all'inizio del documento, è quello di individuare le istruzioni di ogni ciclo compatibili con la LICM.

Per progettare l'algoritmo suddividiamo il problema originale i 3 principali step:

1. Ricerca delle istruzioni **loop-invariant**
2. Per quali di queste istruzioni loop-invariant è applicabile il processo di **code motion**
3. Applicazione della code motion sulle istruzioni contrassegnate

##  1. Ricerca delle istruzioni loop-invariant

La prima fase dell'algoritmo è determinare **l'insieme di istruzioni invarianti** dell'attuale loop, per essere poi utilizzato nello step successivo.

Per ottenere questo risultato si chiama la funzione `search_loop_invariant_instructions,`  che restituisce l'eventuale lista delle istruzioni invarianti.

```c
SetVector<Instruction *> loop_invariant_instructions = search_loop_invariant_instructions(loop);
```

![Immagine 3](assets/Pasted%20image%2020260517122327.png)

La motivazione dietro l'uso di un oggetto della classe `SetVector` anziché di un usuale `std::vector` è giustificata dal fatto che la struttura dati `SetVector` si comporta come `std::vector` , ma garantisce l'assenza di duplicati e preserva l'ordine logico di inserimento. In mancanza di istruzioni loop-invariant verrà restituito un oggetto vuoto, cioè `<setVector>.empty() = true`.

### Funzione search_loop_invariant_instructions

>Questa funzione ha il compito di identificare **l'insieme delle istruzioni loop-invariant all'interno di un ciclo** attraverso l'implementazione di un algoritmo iterativo.

#### Data Flow Analysis

Studiando l'attuale problema dal punto di vista della DFA, quindi senza considerare le istruzioni in formato SSA, la teoria stabilisce che per trovare questo insieme di istruzioni si debba prima di tutto calcolare le varie **reaching definitions**, e poi individuare le istruzioni in base a queste condizioni:

- Tutte le definizioni degli operandi che raggiungono l'istruzione si trovano al di fuori del loop
	- In caso in cui gli operandi siano delle costanti la verifica è valutata positivamente
- Oppure c'è esattamente una *reaching definition* per l'operando, e si tratta di un'istruzione del loop che è stata marcata a sua volta come loop-invariant.

#### SSA

Quando si passa dal modello della DFA al formato SSA l'algoritmo si semplifica ulteriormente. In formato SSA, ogni variabile viene definita **una e una sola volta** nel programma, e ogni **uso di una variabile fa riferimento a quell'unica definizione.**

Questo cambia completamente il modo di verificare le condizioni della DFA:

**Una sola definizione per registro:** Nella DFA, come citato precedentemente, un operando può essere raggiunto da diverse definizioni provenienti da differenti rami del programma. In SSA, questo è matematicamente impossibile per i *registri virtuali*, ovvero c'è sempre una sola istruzione che definisce quel registro.

```c
int a = 1;
int b = 1;
for (int i = 0; i < 10; i++) {
	...
	int tmp = a + b;
	a = b;
	b = tmp;
	...
}

/*
Se si esamina l'istruzione tmp, sia a che b hanno due diverse definizioni: una proveniente dall'esterno e l'altra all'interno del ciclo
*/
```

**Il ruolo delle istruzioni PHI:** Se un certo valore cambia tra un'iterazione e l'altra, come fa l'istruzione ad avere una sola definizione? Il dubbio è risolto tramite i **nodi phi SSA**. Questi sono istruzioni speciali presenti all'inizio del blocco header che *unisce* i diversi flussi di dati.

Riprendendo l'esempio precedente il compilatore produrrebbe una versione simile a questa:

```c
...
%a = phi i32 [1, %entry], [%next_a_value, %loop.body]
%b = phi i32 [1, %entry], [%next_b_value, %loop.body]
...

/*
La variabile assume il valore 1 se si proviene dall'esterno del loop (%entry), altrimenti viene inizializzata con il rispettivo valore post esecuzione dell'iterazione corrente (%loop.body),
*/
```

Grazie a questa proprietà dell'SSA, se si incontra un'istruzione `phi` si sa a priori che quel valore è il risultato di un bivio nel flusso logico. Di conseguenza, quel valore ha quindi differenti *reaching definitions*, pertanto in automatico la si scarta come istruzione invariante.

Ritornando alla definizione del modello classico, bisogna ora determinare se un'istruzione è effettivamente **loop-invariant**.

LLVM nativamente mette a disposizione un metodo pubblico che verifica se un'istruzione è *loop-invariant*.

```c
bool Loop::isLoopInvariant(const Value *V) const {

	if (const Instruction *I = dyn_cast<Instruction>(V))
		return !contains(I);

	return true; // All non-instructions are loop invariant
}
```

Il problema è che questa soluzione permette di verificare soltanto il primo caso. Infatti, se applicata ad entrambi gli operandi di un'istruzione, essa ritorna `true` soltanto quando sono costanti o la loro definizione è all'esterno del ciclo.

Per soddisfare anche il secondo punto della definizione entra in gioco il precedente vettore `SetVector`. 
Dato che un'istruzione può essere loop-invariant anche quando il suo operando è definito all'interno del ciclo ed anch'esso loop-invariant, durante l'analisi degli operandi sarà quindi necessario anche confrontare se esso è contenuto nel vettore, allora l'esito sarà positivo.

In ambo i casi, quando un'istruzione viene riconosciuta come tale, è importante inserirla nel `SetVector`, in modo da utilizzarla nelle ricerche successive.

![Immagine 4](assets/Pasted%20image%2020260517122341.png)

Nonostante il formato SSA semplifichi il controllo delle definizioni, l'ordine in cui i `basicBlock` di un loop sono memorizzati all'interno dell'IR non riflette necessariamente l'ordine temporale di esecuzione, specialmente in cicli complessi con diversi blocchi.

Il metodo `loop->getBlocks()` restituisce una lista di blocchi. Se l'ordine testuale nel file IR prevede che **l'uso** di una variabile appaia in un **blocco posizionato prima del blocco contenente la definizione**, un unico passaggio sequenziale fallirebbe.

Generalmente queste soluzioni possono incombere quando vengono effettuate precedentemente delle altre ottimizzazioni al codice o a scelte architetturale dettate dal front-end nella generazione del file IR.

Si consideri il seguente esempio di IR perfettamente valido in SSA, in cui il blocco condizionale è stampato per primo.

```c
cond:
	%f = add i32 %a, 10    // Uso di %b prima della definizione
	br i1 %cond, label %body, label %exit
	
body:
	%a = add i32 %b, %c // Definizione di %a
	br label %cond
```

Si supponga che `%b e %c` siano definite esternamente al loop. Di seguito si analizza il comportamento dell'algoritmo nei vari cicli del `while`:

**Iterazione 1**
1. L'algoritmo esamina `cond` e incontra `%f = add i32 %a, 10`. Interroga la funzione nativa `loop->isLoopInvariant(%a)`, la quale risponde con `false` perché l'istruzione che definisce `%a` si trova all'interno del loop. Non avendo ancora letto il blocco successivo, `%a` non è presente nemmeno nel set `invariants`. **Risultato:** `%f` viene temporaneamente scartata.
2. L'algoritmo passa a `body` ed esamina `%a = add i32 %b, %c`. Poiché `%b` e `%c` sono esterni, `%a` viene catalogata come istruzione *loop-invariant* ed inserita nel set `invariants`. Il flag `isChanged` viene impostato a `true`.

**Iterazione 2**
Avendo rilevato un cambiamento, il `while` riesegue il ciclo sui blocchi partendo dall'inizio.

1. L'algoritmo torna su `cond` e analizza nuovamente `%f = add i32 %a, 10`. La funzione `isLoopInvariant(%a)` restituisce ancora `false`, ma questa volta il controllo custom `invariants.count(%a)` ha successo (grazie all'inserimento avvenuto al termine del primo giro).
2. Avendo validato `%a`, anche `%f` viene finalmente riconosciuta come loop-invariant e inserita nel set. `isChanged` diventa nuovamente `true`.

**Iterazione 3**
L'algoritmo compie un ultimo giro di controllo in cui nessuna nuova istruzione viene marcata. `isChanged` rimane `false`, determinando l'uscita del ciclo.

# Code Motion

Una volta ottenuto l'insieme delle istruzioni loop-invariant, non è detto che tutte possano essere spostate liberamente nel preheader. La **code motion** è l'operazione di spostamento fisico di un'istruzione dal suo blocco originale al preheader del loop, ma affinché tale operazione sia **semanticamente corretta**, è necessario verificare una serie di condizioni preliminari.

> L'obiettivo della fase di code motion è individare, tra tutte le istruzioni, quelle per cui lo spostamento nel preheader **non altera il comportamento osservabile del programma**.

---
# Condizioni per la Code Motion

Dato un'istruzione loop-invariant `I`, lo spostamento nel preheader è lecito se valgono **tutte** le seguenti condizioni:

1. **Assenza di side effects** - L'istruzione non deve produrre effetti collaterali osservabili (scritture in memoria, chiamate a funzione, eccezioni, ecc...).
2. **Dominanza delle uscite del loop** - Il blocco che contiene `I` deve dominare tutti i blocchi di uscita del loop, oppure `I` deve essere un'operazione intrinscamente sicura (come un `BinaryOperator`).
3. **Definizione unica nel loop** - In SSA questa proprietà è sempre soddisfatta per i registri virtuali, poiché ogni valore è definito esattamente una volta.
4. **Dominanza di tutti gli usi interni al loop** - Il blocco di `I` deve dominare ogni blocco che contiene un uso di `I` all'interno del loop.

Per la valutazione di queste condizioni viene utilizzata la funzione `filter_safe_to_move_instructions`, che riceve in input l'insieme delle istruzioni loop-invariant e restituisce il sottoinsieme effettivamente candidato alla code motion.

```c
std::vector<Instruction *>filter_safe_to_move_instructions(
    const SetVector<Instruction *> &invariant_instructions,
    Loop *loop,
    DominatorTree &DT
  )
```

---
# Funzione `filter_safe_to_move_instructions()`

> Questa funzione ha il compito di **filtrare l'insieme delle istruzioni loop-invariant**, conservando solo quelle per cui la code motion è semanticamente sicura.

## Controllo di integrità di base

Il primo filtraggio è immediato e scarta direttamente le istruzioni che, per natura, non possono mai essere hoistate:

```c
if (I->isTerminator() || isa<PHINode>(I) || I->mayHaveSideEffects()) continue;
```

- **Terminatori** - Le istruzioni come `br` o `ret` gestiscono il flusso di controllo del blocco e non possono essere spostate.
- **Nodi PHI** - Come discusso nella fase di ricerca delle istruzioni loop-invariant, un nodo `phi` rappresenta per definizione la confluenza di più flussi di dati. Spostarli nel preheader violerebbe la semantica SSA.
- **Istruzioni con side effects** - Scritture di memorie (`store`) , chiamate a funzione (`call`) o istruzioni che possono generare eccezioni possono alterare il comportamento osservabile del programma. Il metodo `mayHaveSideEffects()` del framework LLVM verifica automaticamente questa proprietà.

## Condizione 1 - Dominanza delle uscite

La motivazione di questa condizione è la seguente: se `I` si trova in un blocco che non domina tutte le uscite, allora esiste un percorso di esecuzione del loop che termina **senza mai attraversare il blocco di `I`**. In un tale percorso, l'istruzione non verrebbe mai eseguita. Spostarla nel preheader la farebbe eseguire **incondizionatamente**, modificando il comportamento del programma.

```c
SmallVector<BasicBlock *, 8> exit_blocks;
loop->getExitBlocks(exit_blocks);
// itero sulle istruzioni identificate come invarianti 
for (Instruction *I : invariant_instructions) {
	if (I->isTerminator() || isa<PHINode>(I) || I->mayHaveSideEffects()) continue; // controllo di integrità base: non sposto terminatori, nodi PHI o istruzioni con side effect
	
	// condizione di dominanza delle usite del loop
	bool dominates_all_exits = true;
	BasicBlock *inst_bb = I->getParent(); // identifico il blocco in cui si trova l'istruzione
	// verifico se il blocco dell'istruzione domina tutti i blocchi di uscita
	for (BasicBlock *exit_bb : exit_blocks) {
		if (!DT.dominates(inst_bb, exit_bb)) {
			dominates_all_exits = false;
			break; // basta che non ne domini uno per invalidare la condizione
		}
}
```

Il metodo `loop->getExitBlocks()` restituisce tutti i blocchi che sono al di fuori del loop ma che hanno almeno un predecessore interno ad esso. Per ogni istruzione, si verifica che il suo blocco padre `inst_bb` domini ciascuno di questi blocchi di uscita tramite il `DominatorTree`.

### Eccezione per i `BinaryOperators`

Nel caso in cui la condizione di dominanza non sia soddisfatta, l'istruzione non viene scartata immediatamente se si stratta di un `BinaryOperator` (operazioni aritmetiche come `add`, `mul`, `sub`, ecc.)

```c
if (!dominates_all_exits) {
// Se è un BinaryOperator (add, mul, sub...), è un'istruzione intrinsecamente "safe"
// che non genera eccezioni. Quindi posso sollevarla anche se il blocco non domina le uscite.
	if (!isa<BinaryOperator>(I)) {
		continue; // Se non è un BinOp e non domina le uscite, la scarto definitivamente
	}
}
```

La motivazione è che le operazioni aritmetiche su interi sono **totali e prive di eccezioni** (ad esclusione della divisione intera per zero, che tuttavia in LLVM IR è un comportamento non definito e non un'eccezione hardware). Spostarle nel preheader, anche se il loro blocco non domina tutte le uscite, non altera il comportamento osservabile del programma: l'unico effetto è seguire un calcolo il cui risultato potrebbe non essere utilizzato in alcuni percorsi, ma senza alcuna conseguenza sullo stato della memoria o sul flusso di controllo. 

Si consideri il seguente scenario applicato al primo loop del programma di test `Loop.c`

```c
  for (i = 0; i < 10; i++) {
    z = x * y;       // loop-invariant deve essere hoistato
    ret = ret + z;   // NON loop-invariant (dipende da ret)
  }
```

Nell'IR corrispondente `Loop.m2r.ll`, il blocco `%6` contiene l'istruzione `%7 = mul nsw i32 5, 3`. Il blocco di uscite del loop è `%11`. Poiché `%6` è il corpo del loop e non il suo header non domina necessariamente `%11` (si può direttamente dall'header `%4` senza mai entrare in `%6`). Grazie all'eccezione per i `BinaryOperator`, l'istruzione `mul` supera comunque il filtro e viene inserita nel vettore `safe_to_move` come candidata alla code motion.

## Condizione 2 - Dominanza degli usi interni

Anche se un'istruzione è loop-invariant e sicura da eseguire nel preheader, è necessario verificare che **tutti i suoi usi all'interno del loop** si trovino in blocchi che essa domina. In caso contrario, potrebbe esistere un percorso in cui un uso di `I` viene raggiunto prima che `I` stessa sia eseguita, violando la correttezza del programma.

```c
bool dominates_all_uses = true;
// itero su tutti gli utilizzatori dell'istruzione I
for (User *U : I->users()) {
	if (Instruction *user_inst = dyn_cast<Instruction>(U)){
// mi interesso solo su gli utilizzatori che si trovano dentro il loop
		if (loop->contains(user_inst)) {
// controllo se l'istruzione I domina l'utilizzatore U
			if (!DT.dominates(I, user_inst)){
				dominates_all_uses = false;
				break;
			}
		}
	}
}

if (!dominates_all_uses) {
	continue;// non domina tutti i suoi usi, allora l'istruzione viene scartata
}
```

Il metodo `I->users()` restituisce tutti gli utilizzatori del valore prodotto da `I`. Il controllo viene ristretto ai suoi usi **interni al loop** tramite `loop->contains(user_inst)`, poiché gli usi esterni non sono influenzati dallo spostamento nel preheader. Per ciascuno di essi, il `DominatorTree` verifica che `I` domini `user_inst`

---
# Esempio Pratico - `Loop.c`

Si analizza il comportamento dell'intero algoritmo applicato ai due loop di `Loop.c`

## Primo loop

```c
// Caso classico LICM: x*y è loop-invariant
// Caso classico LICM: x*y è loop-invariant
for (i = 0; i < 10; i++) {
	z = x * y;       // loop-invariant deve essere hoistato
	ret = ret + z;   // NON loop-invariant (dipende da ret)
}
```

Nell'IR (`Loop.m2r.ll`), le istruzioni rilevanti del blocco `%6` (corpo del loop) sono:

```c
%7 = mul nsw i32 5, 3   ; z = x * y  →  loop-invariant (operandi costanti)
%8 = add nsw i32 %.0, %7 ; ret = ret + z  →  NON loop-invariant (%.0 è phi)
```

- `%7` viene identificata come loop-invariant nella fase precedente.
- `filter_safe_to_move_instructions` la valida: è un `BinaryOperator`, quindi supera la condizione di dominanza delle uscite anche se il blocco `%6` non domina `%11`. Domina inoltre il proprio unico uso interno (`%8`).
- `%7` supera tutti i filtri e viene inserita nel vettore `safe_to_move`, risultando candidata alla code motion.

## Secondo loop
 
```c
for (i = a; i < b; i++) {
  int w = x + y;   // loop-invariant
  ret += w * i;    // NON loop-invariant
}
```
 
Nell'IR, il blocco `%14` contiene:
 
```c
%15 = add nsw i32 5, 3    ; w = x + y  →  loop-invariant (operandi costanti)
%16 = mul nsw i32 %15, %.12 ; w * i  →  NON loop-invariant (%.12 è phi su i)
%17 = add nsw i32 %.1, %16  ; ret += ...  →  NON loop-invariant
```
 
- `%15` viene identificata come loop-invariant.
- Supera tutti i filtri: è un `BinaryOperator` e domina i propri usi interni.
- `%15` supera tutti i filtri e viene inserita nel vettore `safe_to_move`, risultando candidata alla code motion.
