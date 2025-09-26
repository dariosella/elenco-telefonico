# elenco-telefonico
Progetto universitario per l'esame di Sistemi Operativi.
Il progetto riguarda un servizio di elenco telefonico ed è costituito da un'applicazione server e una client, entrambe scritte in linguaggio C.
Il server è multithread ed accetta fino ad 8 connessioni contemporaneamente, creando un thread per ogni client connesso.
Inoltre mantiene due file di configurazione:
  - users: contiene username e password dei client autorizzati
  - permissions: contiene, per ogni username, i permessi di lettura/scrittura sul file rubrica
Ogni client connesso dovrà loggarsi, e se il login è andato a buon fine potrà decidere se:
  - aggiungere un contatto in rubrica.
  - cercare un contatto in rubrica.
Il server verificherà se il client ha i permessi necessari per effettuare l'operazione scelta, e agirà di conseguenza.
