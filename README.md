# Elenco Telefonico — Client/Server TCP

Applicazione client–server in C per una semplice rubrica telefonica con autenticazione, permessi (lettura/scrittura) e sincronizzazione **readers–writers** lato server. Comunicazione via TCP, gestione dei timeout, segnali, thread e semafori POSIX.

---

## Struttura del progetto

```
Makefile
helper.h / helper.c      // wrapper I/O robusti, costanti, utility (readLine, safeSend/Recv, safeWait, ...)
user.h / user.c          // login/registrazione e controllo permessi (file "utenti" e "permessi")
contact.h / contact.c    // parsing/creazione contatto, add/search (file "rubrica")
server.c                 // server multithread, segnale/i, RW-semafori, protocollo
client.c                 // client interattivo con menù iniziale e menù operazioni
```

### Costanti principali (da `helper.h`)
- `TIMEOUT = 30` secondi (socket e input con allarme)
- `LISTENQ = 8` backlog listen
- Taglie buffer:
  - `BUF_SIZE = 97` (righe generiche / contatto)
  - `NAME_SIZE = 64`, `NUMBER_SIZE = 32`
  - `USR_SIZE = 32`, `PWD_SIZE = 32`, `PERM_SIZE = 16`
  - `CHOICE_SIZE = 16`

---

## Build

Richiede un ambiente POSIX (Linux/macOS) e GCC con pthreads e semafori POSIX.

```bash
make         # compila server e client
make clean   # pulizia artefatti
```

---

## Esecuzione

### Server
```bash
./server -p <porta>
# es.: ./server -p 12345
```

### Client
```bash
./client -a <indirizzo_server> -p <porta>
# es.: ./client -a 127.0.0.1 -p 12345
```

---

## File dati (creati se mancanti)

- `utenti`   — ogni riga: `username password\n`
- `permessi` — ogni riga: `username permesso\n` con `permesso ∈ { "r", "w", "rw" }`
- `rubrica`  — ogni riga: `"Nome [Nomi secondari] Cognome Numero\n"`

Permessi file: `0600`.

---

## Protocollo applicativo (alto livello)

### 1) Fase iniziale: Registrazione/Login
Il client mostra:
```
1. Registrarti
2. Loggarti
```
Invia al server **uint32_t** in **network byte order** (`htonl`):
- `1` → **Registrazione**: il client invia `USR_SIZE` bytes (username), `PWD_SIZE` (password), `PERM_SIZE` (permesso).  
  Il server **serializza** l’accesso a `utenti/permessi` con `pthread_mutex_t u_mutex`, chiama `usrRegister` e risponde con **int32 (htonl)**:
  - `0` → ok
  - `<0` → errore (es. utente già esistente/errore I/O)
- `2` → **Login**: il client invia username (`USR_SIZE`) e password (`PWD_SIZE`).  
  Il server chiama `usrLogin` e risponde con **int32 (htonl)**:
  - `0` → ok
  - `-2` → password errata
  - `-3` → utente inesistente
  - `-1` → errore

Se la risposta è `0`, si passa al menù operativo.

### 2) Menù operativo (post-login)
Il client mostra:
```
1. Aggiungere contatto
2. Cercare contatto
3. Uscire
```
Invia la scelta come **uint32_t (htonl)**.

- **1. Aggiungere contatto**
  1) Server verifica `checkPermission(username, "w")` e invia **uint8_t**: `1` autorizzato, `0` negato.  
  2) Se autorizzato, il client invia una riga (`BUF_SIZE`) del tipo:  
     `Nome [Nomi secondari] Cognome Numero`  
     Il server effettua la **sezione critica writer** sulla rubrica con RW-semafori (vedi sotto), esegue `addContact` e risponde una stringa (`BUF_SIZE`) con l’esito (OK/errore/già esiste).

- **2. Cercare contatto**
  1) Server verifica `checkPermission(username, "r")` e invia **uint8_t**: `1` autorizzato, `0` negato.  
  2) Se autorizzato, il client invia il **nome completo** (`NAME_SIZE`).  
     Il server entra come **lettore** (RW-semafori) ed esegue `searchContact`, inviando una stringa (`BUF_SIZE`) con il risultato (contatto/non trovato/errore).

- **3. Uscire**  
  Il client chiude la socket e termina.

> **Formattazione**: tutti i payload di testo sono **fixed-size** (riempiti/terminati a `\0`); i numeri (scelte/codici) sono scalari in network byte order.

---

## Sincronizzazione (server)

Implementazione **readers–writers** con fairness per gli scrittori:

```c
typedef struct {
    sem_t turnstile; // chiuso se c'è uno writer in attesa
    sem_t roomEmpty; // stanza libera per writer (o nessun reader inside)
    sem_t mutex;     // protegge il contatore readers
    int readers;
} rwsem_t;
```

---

## Gestione segnali

### Server
- `SIGINT`, `SIGTERM` → chiusura socket e semafori RW.
- Ignora `SIGPIPE`, `SIGHUP`.
- Solo il main thread gestisce segnali.

### Client
- `SIGALRM` → timeout input (`safeInputAlarm`).
- `SIGINT` → chiusura socket e exit.
- Ignora `SIGPIPE`.

---
