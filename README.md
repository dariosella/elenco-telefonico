# Elenco Telefonico

Progetto universitario per l'esame di Sistemi Operativi

Autore: Dario Sella

Università: Università degli Studi di Roma Tor Vergata

## Descrizione

Questo progetto implementa un servizio di **elenco telefonico** con architettura **client-server**.  
Il server è multithreaded e permette fino a 8 connessioni contemporanee, gestendo autenticazione e permessi utente.

## Funzionalità

- **Login utente** con autenticazione tramite file di configurazione
- **Gestione permessi** di lettura/scrittura sulla rubrica
- **Aggiunta di contatti** alla rubrica telefonica
- **Ricerca contatti** per nome
- **Gestione concorrente** con mutex per accesso sicuro alla rubrica

## Struttura del progetto

- `server.c` — codice del server
- `client.c` — codice del client
- `contact.c`/`contact.h` — gestione dei dati dei contatti
- `helper.c`/`helper.h` — funzioni di utilità, costanti e macro
- `Makefile` — compilazione automatica

## File di configurazione

- `utenti` — elenca gli utenti autorizzati
- `permessi` — associa a ogni utente i permessi

## Compilazione

Esegui:
```bash
make
```
per generare gli eseguibili `server` e `client`.

## Esecuzione

**Server:**
```bash
./server -p <porta>
```

**Client:**
```bash
./client -a <indirizzo_server> -p <porta>
```

## Esempio d’uso

1. Avvia il server: `./server -p 5000`
2. Avvia il client: `./client -a 127.0.0.1 -p 5000`
3. Effettua il login con le credenziali di un utente presente nel file `utenti`
4. Scegli se aggiungere o cercare un contatto

## Autore

Dario Sella  
Settembre 2025

