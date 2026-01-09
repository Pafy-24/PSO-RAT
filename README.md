# PSO-RAT — Remote Administration Tool

**Proiect de semestru pentru materia Proiectarea Sistemelor de Operare (PSO)**

## Descriere

Acest proiect implementează un Remote Administration Tool (RAT) educational scris în C++17, demonstrând conceptele fundamentale învățate în cadrul cursului PSO:

- **Gestionarea proceselor și thread-uri**: Arhitectură multi-threaded cu server concurrent
- **Sincronizare (mutex)**: Protejarea resurselor partajate între thread-uri
- **Socket-uri TCP/UDP**: Comunicare client-server prin SFML Network
- **Comunicare inter-proces**: Utilizarea pipe-urilor pentru executarea comenzilor shell
- **Semnale de sistem**: Gestionarea închiderii gracioase și cleanup-ul resurselor
- **Gestionarea memoriei**: Smart pointers (`unique_ptr`, `shared_ptr`) pentru management RAII

## Concepte PSO Demonstrate

### 1. **Socket-uri și Comunicare Rețea**
- Implementare wrapper TCP/UDP peste SFML (`Utils/TCPSocket.cpp`, `Utils/UDPSocket.cpp`)
- Server TCP non-blocking cu acceptare asincronă de clienți
- Protocol JSON pentru comunicare structurată client-server
- Keep-alive UDP pentru menținerea conexiunii

### 2. **Arhitectură Multi-Threading**
- Thread dedicat pentru input administrativ (stdin)
- Thread-uri pentru ping/keep-alive
- Gestionare concurentă a mai multor clienți conectați simultan
- Sincronizare thread-safe cu `std::mutex` și `std::lock_guard`

### 3. **Pipe-uri și Execuție Comenzi Shell**
- Folosirea `popen()` pentru executarea comenzilor bash
- Capturarea stdout/stderr prin pipe-uri
- Returnarea output-ului și exit code către server

### 4. **Gestionarea Proceselor**
- `fork()` și `exec()` pentru spawn-area terminalelor externe
- `waitpid()` pentru gestionarea proceselor copil
- Detașare proces cu `setsid()` pentru daemonizare

### 5. **Pattern-uri de Design**
- **Singleton**: `ServerManager` — instanță unică pentru gestionarea serverului
- **Controller Pattern**: Separarea logicii în controller-e specializate
- **RAII**: Smart pointers pentru gestionarea automată a memoriei

## Structura Proiectului

```
PSO-RAT/
├── RAT-Server/          # Server-ul central
│   ├── src/
│   │   ├── ServerManager.cpp           # Manager principal (singleton)
│   │   ├── ClientManagement.cpp        # Gestionare clienți (thread-safe)
│   │   ├── ServerCommandController.cpp # CLI administrativ
│   │   ├── ServerFileController.cpp    # Transfer fișiere
│   │   ├── ServerPingController.cpp    # Keep-alive UDP
│   │   └── ServerLogController.cpp     # Logging centralizat
│   └── include/
├── RAT-Client/          # Client-ul
│   ├── src/
│   │   ├── ClientManager.cpp           # Orchestrator client
│   │   ├── ClientBashController.cpp    # Execuție comenzi (pipe-uri)
│   │   ├── ClientFileController.cpp    # Upload/download
│   │   ├── ClientScreenshotController.cpp
│   │   └── ClientPingController.cpp
│   └── include/
├── Utils/               # Librării comune
│   ├── src/
│   │   ├── TCPSocket.cpp              # Wrapper TCP (SFML)
│   │   └── UDPSocket.cpp              # Wrapper UDP (SFML)
│   └── include/Utils/
│       └── Base64.hpp                  # Header-only encoding
├── build_make/          # Binare compilate
│   ├── rat_server
│   ├── rat_client
│   └── libutils.so
└── CLASS_DIAGRAM.md     # Documentație arhitectură
```

## Funcționalități Implementate

### Server
- **Acceptare clienți**: Listener TCP non-blocking, înregistrare automată
- **Management centralizat**: Clasa `ClientManagement` gestionează harta clienților (thread-safe)
- **Comenzi administrative**: 
  - `list` — listează clienții conectați
  - `choose <client>` — selectează client implicit
  - `bash <client> <cmd>` — execută comandă shell pe client
  - `download <client> <remote> <local>` — descarcă fișier
  - `upload <client> <local> <remote>` — încarcă fișier
  - `screenshot <client>` — capturează ecran
  - `kill <client>` / `killall` — deconectează clienți
  - `showlogs` — vizualizare log-uri în terminal
  - `quit` — oprește serverul
- **Logging centralizat**: `/tmp/rat_server.log` cu thread-safe append

### Client
- **Auto-conectare**: Se conectează la server cu hostname, OS, kernel
- **Execuție comenzi**: Folosește `popen()` pentru comenzi bash
- **Transfer fișiere**: Upload/download cu encoding Base64
- **Screenshot**: Capturare ecran cu `scrot`/`import`/`gnome-screenshot`
- **Keep-alive**: Răspunde la ping-uri UDP

## Prerechizite

- **OS**: Linux (testat pe Arch, Ubuntu, Debian)
- **Compilator**: g++ cu suport C++17
- **Build system**: GNU Make
- **Librării**:
  - SFML Network, System, Window, Graphics (`libsfml-dev`)
  - nlohmann-json (header-only, inclus în sistem sau `/usr/include`)

```bash
# Ubuntu/Debian
sudo apt install build-essential libsfml-dev nlohmann-json3-dev

# Arch Linux
sudo pacman -S base-devel sfml nlohmann-json
```

## Compilare și Rulare

### 1. Compilare
```bash
make
```

Binarele sunt generate în `build_make/`:
- `rat_server` — serverul
- `rat_client` — clientul  
- `libutils.so` — librărie partajată

### 2. Rulare Server
```bash
./build_make/rat_server
```
Server-ul pornește pe portul 5555 implicit.

### 3. Rulare Client
```bash
./build_make/rat_client
```
Clientul se conectează la `127.0.0.1:5555`.

## Exemple de Utilizare

```bash
# Pe server
=== RAT Server Control ===
Type 'help' for available commands

> list
Clients:
  - arch_0 (127.0.0.1:45678)

> bash arch_0 uname -a
Linux hostname 6.1.0-arch1-1 #1 SMP PREEMPT_DYNAMIC x86_64 GNU/Linux

> download arch_0 /etc/hostname ./hostname.txt
Downloaded 9 bytes to ./hostname.txt

> screenshot arch_0
Screenshot saved to /tmp/ss_arch_0_1704672345.png

> kill arch_0
Killed arch_0
```

## Arhitectură și Sincronizare

### Thread-Safety
- **ClientManagement**: `std::mutex` protejează harta `clients_`
- **Logging**: `std::mutex` pentru accesul la queue-ul de log-uri
- **Socket I/O**: Operații blocking cu timeout pentru evitarea deadlock-urilor

### Pattern de Comunicare
1. **Request-Response sincron**:
   ```
   Server → sendRequest(client, JSON)
   Server → receiveResponse(client, JSON&, timeout)
   ```

2. **Protocolul JSON**:
   ```json
   Request: {"controller": "bash", "cmd": "ls -la"}
   Response: {"output": "...", "exitcode": 0}
   ```

### Gestionarea Memoriei
- **Smart pointers**: `unique_ptr` pentru ownership exclusiv (sockets, controllers)
- **RAII**: Destructorii asigură cleanup-ul automat (close socket, join thread)
- **No raw pointers**: Eliminarea memory leaks prin ownership clar

## Probleme Cunoscute și Limitări

- **Securitate**: Fără autentificare sau criptare (proiect educational!)
- **Timeout-uri**: Socket-urile pot bloca dacă clientul nu răspunde
- **Threading**: Nu există pool de thread-uri (un thread per funcționalitate)
- **Protocol**: JSON text-based (overhead mai mare decât binar)

## Concepte PSO Aplicate

| Concept | Implementare | Fișier |
|---------|-------------|---------|
| **Socket TCP** | Wrapper SFML, listener non-blocking | `Utils/TCPSocket.cpp` |
| **Socket UDP** | Keep-alive ping/pong | `Utils/UDPSocket.cpp` |
| **Thread-uri** | stdin thread, ping threads | `ServerCommandController.cpp` |
| **Mutex** | Protecție `clients_`, `logs_` | `ServerManager.cpp` |
| **Pipe-uri** | `popen()` pentru comenzi shell | `ClientBashController.cpp` |
| **fork/exec** | Spawn terminal pentru logs | `ServerCommandController.cpp` |
| **Semnale** | Graceful shutdown | `main.cpp` |
| **Smart pointers** | `unique_ptr`, `shared_ptr` | Throughout |

## Diagrame

Vezi [CLASS_DIAGRAM.md](CLASS_DIAGRAM.md) pentru:
- Diagrama de clase (Mermaid)
- Flow-uri de comunicare
- Relații între componente

## Referințe

- Curs PSO: Proiectarea Sistemelor de Operare
- SFML Network: https://www.sfml-dev.org/documentation/2.6.0/group__network.php
- nlohmann/json: https://json.nlohmann.me/
- POSIX Threads: `man pthreads`, `man mutex`
- Socket Programming: `man 2 socket`, `man 2 bind`, `man 2 listen`

## Autor

Proiect realizat pentru materia PSO, semestrul 2, 2025-2026.

