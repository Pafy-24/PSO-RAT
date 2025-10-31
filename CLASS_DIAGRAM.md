# PSO-RAT Class Diagram

## Overview
Această diagramă prezintă relațiile între clasele principale din proiectul PSO-RAT.

```mermaid
classDiagram
    %% Core Interface
    class IController {
        <<interface>>
        +start() void
        +stop() void
        +sendJson(json) bool
        +receiveJson(json&) bool
        +handleJson(json) bool
    }

    %% Server Manager - Central orchestrator
    class ServerManager {
        <<singleton>>
        -instance: shared_ptr~ServerManager~
        -clients_: map~string, unique_ptr~TCPSocket~~
        -controllers_: map~string, unique_ptr~IController~~
        -listener_: unique_ptr~TCPSocket~
        -logs_: queue~string~
        -port_: unsigned short
        -running_: bool
        +getInstance() shared_ptr~ServerManager~
        +start(port) bool
        +stop() void
        +run(port) int
        +addClient(name, socket) bool
        +removeClient(name) bool
        +getClient(name) TCPSocket*
        +getController(name) IController*
        +getServerController(name) ServerController*
        +listClients() vector~string~
        +hasClient(name) bool
        +pushLog(msg) void
        +popLog(out) bool
    }

    %% Server Controllers
    class ServerController {
        -socket_: TCPSocket*
        -manager_: ServerManager*
        -deviceName_: string
        -pingThread_: unique_ptr~thread~
        -recvThread_: unique_ptr~thread~
        -running_: atomic~bool~
        +ServerController(socket)
        +ServerController(manager, name, socket)
        +isValid() bool
        +sendJson(obj) bool
        +receiveJson(out) bool
        +start() void
        +stop() void
    }

    class ServerCommandController {
        -manager_: ServerManager*
        -running_: bool
        -stdinThread_: unique_ptr~thread~
        +start() void
        +stop() void
        -processLine(line) string
        -handleList() string
        -handleChoose(name) string
        -handleKill(name) string
        -handleBash(name, cmd) string
        -handleQuit() string
        -handleShowLogs() string
        -spawnTerminal(cmd) pid_t
        -stdinLoop() void
    }

    class ServerLogController {
        -manager_: ServerManager*
        -running_: bool
        -thread_: unique_ptr~thread~
        +start() void
        +stop() void
        +showLogs(path) string
        +handleJson(params) bool
    }

    class ServerPingController {
        +start() void
        +stop() void
    }

    %% Client Side
    class ClientManager {
        -controllers_: map~string, unique_ptr~ClientController~~
        -running_: bool
        +connectTo(address, port) bool
        +disconnect() void
        +run() int
        +getController(name) ClientController*
    }

    class ClientController {
        -socket_: shared_ptr~TCPSocket~
        +ClientController(socket)
        +sendJson(obj) bool
        +receiveJson(out) bool
        +isValid() bool
    }

    class ClientPingController {
        +start() void
        +stop() void
    }

    %% Utils
    class TCPSocket {
        <<wrapper>>
        -socket_: sf::TcpSocket
        -listener_: sf::TcpListener
        +connect(address, port) bool
        +send(message) bool
        +receive(out) bool
        +listen(port) bool
        +accept() unique_ptr~TCPSocket~
        +bind(port) bool
        +close() void
        +setBlocking(blocking) void
    }

    class Socket {
        <<abstract>>
        +bind(port) bool
        +close() void
    }

    %% Relationships
    IController <|-- ServerController : implements
    IController <|-- ServerCommandController : implements
    IController <|-- ServerLogController : implements
    IController <|-- ServerPingController : implements
    
    ServerController <|-- ServerCommandController : extends
    ServerController <|-- ServerLogController : extends
    ServerController <|-- ServerPingController : extends

    ServerManager "1" o-- "0..*" IController : controllers_
    ServerManager "1" o-- "0..*" TCPSocket : clients_
    ServerManager "1" -- "1" TCPSocket : listener_

    ServerController "1" --> "1" TCPSocket : uses
    ServerController "1" --> "1" ServerManager : manager_

    ClientManager "1" o-- "0..*" ClientController : controllers_
    ClientController "1" --> "1" TCPSocket : socket_

    Socket <|-- TCPSocket : extends

    %% Notes
    note for ServerManager "Singleton pattern\nCentral resource owner\nManages client connections\nand controller lifecycle"
    
    note for IController "Base interface for all controllers\nDefines JSON communication\nand lifecycle methods"
    
    note for ServerController "Per-client controller\nHandles ping/recv threads\nManages client communication"
    
    note for ServerCommandController "Admin interface controller\nHandles CLI commands\nNo network socket needed"
    
    note for TCPSocket "SFML wrapper\nProvides simplified API\nfor TCP operations"
```

## Explicația Arhitecturii

### Componente Principale

1. **ServerManager** (Singleton)
   - Orchestratorul central al serverului
   - Gestionează două colecții importante:
     - `clients_`: socket-urile TCP ale clienților conectați
     - `controllers_`: obiectele care implementează logica pentru fiecare client/serviciu

2. **IController** (Interface)
   - Interfața de bază pentru toate controller-ele
   - Definește metodele standard: `start()`, `stop()`, `sendJson()`, `receiveJson()`, `handleJson()`

3. **ServerController** (Per-Client)
   - Gestionează comunicarea cu un client specific
   - Rulează thread-uri pentru ping și recepție mesaje
   - Implementează protocolul JSON cu clientul

4. **ServerCommandController** (Admin)
   - Gestionează comenzile administrative prin stdin
   - Nu folosește socket TCP (doar pentru comenzi locale)
   - Implementează comenzile: list, choose, kill, bash, showlogs, quit

5. **ServerLogController** (Logging)
   - Gestionează log-urile serverului
   - Poate spawna terminal-uri pentru vizualizarea log-urilor

### Fluxul de Date

1. **Conectarea Clientului:**
   ```
   Client → ServerManager.listener_ → ServerManager.run()
   → Creare TCPSocket în clients_[deviceName]
   → Creare ServerController în controllers_[deviceName]
   → ServerController.start()
   ```

2. **Comenzi Administrative:**
   ```
   stdin → ServerCommandController.stdinLoop()
   → processLine() → handle*() methods
   → Interacțiune cu ServerManager pentru list/kill/bash
   ```

3. **Comunicarea Client-Server:**
   ```
   Client JSON → TCPSocket → ServerController.receiveJson()
   → ServerManager.pushLog() → Logs queue
   ```

### Avantajele Acestei Arhitecturi

- **Separarea Responsabilităților:** Fiecare controller are un rol clar
- **Extensibilitate:** Adăugarea unui nou tip de controller este simplă
- **Gestionarea Resurselor:** ServerManager controlează durata de viață a socket-urilor
- **Testabilitate:** Controller-ele pot fi testate independent

### Puncte de Atenție

- **Sincronizarea:** Utilizarea mutex-urilor pentru accesul la `clients_` și `controllers_`
- **Durata de Viață:** Controller-ele trebuie oprite înainte de închiderea socket-urilor
- **Dependency Injection:** Controller-ele primesc referințe la ServerManager și socket-uri