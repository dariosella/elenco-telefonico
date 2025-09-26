# elenco-telefonico

Progetto universitario per l'esame di Sistemi Operativi.

Il progetto riguarda un servizio di **elenco telefonico** ed è costituito da un'applicazione **client** e una **server**, entrambe scritte in **linguaggio C**.

Il server è **multithread** ed accetta fino ad **8 connessioni** contemporaneamente, creando un thread per ogni client connesso.

Inoltre mantiene **due file di configurazione**:
- *users*: contiene username e password dei client autorizzati.
- *permissions*: contiene, per ogni username, i permessi di lettura/scrittura sul file rubrica.

Ogni client, una volta connesso, deve eseguire il **login**. Se l'autenticazione ha esito positivo, può scegliere tra le seguenti operazioni:
- **Aggiungere** un contatto alla rubrica.
- **Cercare** un contatto nella rubrica.

Il server verificherà se il client possiede i **permessi necessari** per eseguire l'operazione richiesta e agirà di conseguenza.
