# PSO-RAT

This repository contains two small C++ projects:

- `RAT-Server` — a simple POSIX TCP server that accepts one connection, sends a welcome message, echoes one message, then closes the connection.
- `RAT-Client` — a simple POSIX TCP client that connects to the server, reads the welcome message, sends a message and prints the echo.

Build (from repository root):

Build with Make (no CMake required):

```bash
make
```

This will produce binaries in `build_make/`:

- `build_make/rat_server`
- `build_make/rat_client`

Run server in one terminal:

```bash
./RAT-Server/rat_server
```

Run client in another terminal:

```bash
./RAT-Client/rat_client
```

Notes:
- Code uses POSIX sockets and should build on Linux/macOS. Windows is not supported without adjustments.
# PSO-RAT
 Remote Access Tool - made for the school project on Operatyng Systems  Development
