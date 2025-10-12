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

- **Writer** (aggiungi contatto):
  1) `sem_wait(turnstile)`  → blocca nuovi lettori futuri
  2) `sem_wait(roomEmpty)`  → attende stanza vuota
  3) scrive su `rubrica`
  4) `sem_post(roomEmpty)`; `sem_post(turnstile)`

- **Reader** (cerca contatto):
  1) `sem_wait(turnstile)`; `sem_post(turnstile)`  → passa se non ci sono writer in attesa
  2) `sem_wait(mutex)`; `readers++`; se primo: `sem_wait(roomEmpty)`; `sem_post(mutex)`
  3) legge da `rubrica`
  4) `sem_wait(mutex)`; `readers--`; se ultimo: `sem_post(roomEmpty)`; `sem_post(mutex)`

> Le attese sono effettuate tramite `safeWait(sem_t*)`: wrapper su `sem_wait` che **ripete su `EINTR`** e restituisce `-1` su errore reale.

> Le uscite dal thread usano **cleanup handler** (`pthread_cleanup_push/pop`) per:
> - sbloccare `u_mutex` e i semafori RW in caso di terminazione/cancel
> - `close(c_sock)` e `free(...)` sempre e comunque alla fine

---

## Gestione I/O robusta

Wrapper in `helper.c`:

- `ssize_t safeRecv(int sfd, void *buf, size_t size, int flags)`
  - accumula fino a `size`
  - `-2` su timeout (`EAGAIN|EWOULDBLOCK`)
  - `-3` su chiusura connessione (r==0)
  - `-1` su errore

- `ssize_t safeSend(int sfd, const void *buf, size_t size, int flags)`
  - usa `MSG_NOSIGNAL` per non generare `SIGPIPE`
  - `-2` timeout, `-3` peer chiuso, `-1` errore

- `ssize_t readLine(int fd, char *line, size_t size)`
  - legge fino a `\n`/EOF
  - `-2` se overflow della linea, `-1` errore

- `ssize_t safeWrite(...)`
  - robusta a `EINTR`, scrive esattamente `size` o errore

Le funzioni `handleSendReturn/handleRecvReturn` (client e server) **centralizzano la gestione degli esiti**: in caso di `-1/-2/-3` stampano, chiudono dove serve e **terminano** il thread/processo.

---

## Gestione segnali

### Server (`server.c`)
- **Main thread**: installa handler con `sigaction`.
  - `SIGINT`, `SIGTERM` → `interruptHandler`: chiude `l_sock`, distrugge i semafori RW e termina il processo.
  - **Ignora** `SIGPIPE`, `SIGHUP`.
- Solo il **main** gestisce i segnali; i worker thread non ridefiniscono handler.
- Timeout socket connessioni con `SO_RCVTIMEO` e `SO_SNDTIMEO` (30s).

### Client (`client.c`)
- `SIGALRM` → per input con timeout (funzione `safeInputAlarm`).
- `SIGINT` → chiusura `c_sock` e `_exit`.
- **Ignora** `SIGPIPE`.
- Timeout socket con `SO_RCVTIMEO/SO_SNDTIMEO` (30s).
- Funzioni di supporto:
  - `safeInputAlarm(0, buf, size)` + `handleNewline(buf)`  
    (il flush extra è delegato a `handleNewline`, niente doppio flush).

---

## Concorrenza e risorse

- Un thread **per client** (`pthread_create` + `pthread_detach`).
- **Cleanup handlers** in tutti i punti critici (mutex, semafori, socket, heap).
- **`u_mutex`**: serializza **solo** l’accesso ai file `utenti/permessi`.  
  Le letture della rubrica sono concorrenti tra loro (sezione reader), mentre gli scrittori sono esclusivi (sezione writer).
- **SO_REUSEADDR** sul listening socket per restart rapidi.

---

## Formato contatti

Input utente (client):
```
"Alice Bianchi 3401234567"
```
Parsing lato server (`createContact`):
- token numerico → `number`
- token non numerico → concatenato in `name`
- la funzione valida che **entrambi** `name` e `number` siano presenti

`addContact` evita duplicati confrontando la rappresentazione normalizzata (stessa linea che `searchContact` restituirebbe).

---

## Codici di ritorno (riassunto)

- **Login/Registrazione** (int32):
  - `0`   successo
  - `-2`  password errata (login)
  - `-3`  utente inesistente (login)
  - `-1`  errore generico (I/O, allocazioni, ecc.)
- **Autorizzazioni** (uint8_t):
  - `1` autorizzato
  - `0` negato
- **Wrapper I/O**:
  - `>=0` bytes trasferiti
  - `-2` timeout
  - `-3` connessione chiusa
  - `-1` errore

---

## Esempi d’uso (rapidi)

1) Avvia il server:
```bash
./server -p 12345
```

2) Avvia il client:
```bash
./client -a 127.0.0.1 -p 12345
```

3) Dal client:
- **Registrazione** → inserisci utente/password e permesso (`r`, `w` o `rw`)
- **Login**
- **Aggiungi** o **Cerca** contatti secondo menù

---
