# PSO-RAT — Remote Administration Tool (simplificat)

Acest repository conține un prototip de RAT (Remote Administration Tool) scris în C++17. Scopul proiectului este educațional: demonstrăm un server care acceptă clienți, o arhitectură bazată pe "managers" și "controllers", comunicare JSON și un canal administrativ pentru comenzi și vizualizare log-uri.

Structură importantă
- `RAT-Server/` — codul serverului; gestionează conexiunile, controlează clienții și oferă controale administrative.
- `RAT-Client/` — codul clientului care se conectează la server și execută comenzi trimise (executeshell → return stdout + exitcode).
- `Utils/` — utilitare comune (wrapper TCP, socket helpers).
- `build_make/` — rezultate de build (binare: `rat_server`, `rat_client`, `log_script`, `libutils.so`).

Principalele caracteristici
- Comunicare JSON între server și client (folosind nlohmann::json).
- Manager/Controller pattern: `ServerManager` deține socket-urile (clients_) și obiectele `IController` (`controllers_`).
- Canale administrative:
	- interfață CLI (stdin) pe server pentru comenzi precum `list`, `choose`, `kill`, `bash`, `showlogs`, `quit`;
	- afișare log-urilor printr-un mic utilitar `log_script` care poate rula într-un terminal/PTY.
- Mecanism PTY pentru vizualizarea log-urilor în medii headless (fallback la `tmux` sau PTY attach).

Prerechizite
- Linux (testat pe distribuții Debian/Ubuntu)
- g++ (C++17), make
- SFML (doar modulul `sfml-network`) instalat în sistem
- librării de bază (libstdc++, etc.)

Compilare
1. Din directorul rădăcină al repo:

```bash
make
```

Aceasta construiește binarele în `build_make/`:
- `build_make/rat_server` — serverul
- `build_make/rat_client` — clientul
- `build_make/log_script` — utilitar pentru "tail" pe log (folosit de showlogs)

Rulare (exemplu)
1. Pornește serverul (foreground) pe portul 5555 implicit:

```bash
./build_make/rat_server
```

2. Pornește un client într-un alt terminal:

```bash
./build_make/rat_client
```

Comenzi administrative (din terminalul serverului)
- `list` — listează clienții conectați
- `choose <nume_client>` — marchează clientul (comandele ulterioare pot folosi acest client explicit)
- `kill <nume_client>` — deconectează și elimină clientul
- `bash <nume_client> <comandă>` — trimite comanda către client; clientul execută comanda și trimite înapoi JSON cu `out` și `ret` (stdout și codul de ieșire)
- `showlogs` — pornește `log_script` (într-un terminal/PTY) care face `tail -F` pe fișierul de log al serverului (implicit `/tmp/rat_server.log`)
- `quit` — oprește serverul

Notă: în această implementare comenzile administrative sunt citite din stdin (CLI server). Există și un mecanism pentru a porni `log_script` într-un terminal GUI (gnome-terminal/konsole/xterm) sau într-o sesiune `tmux` când nu există GUI.

Locația și formatele de log
- Mesajele serverului sunt scrise în `ServerManager::logPath()` (implicit `/tmp/rat_server.log`).
- `log_script` poate fi rulat direct pentru a vizualiza fișierul în timp real:

```bash
./build_make/log_script /tmp/rat_server.log
```

Considerații de securitate
- Acest proiect este un prototip educațional. Nu folosi acest cod pe rețele sau mașini de producție fără a implementa autentificare, criptare și controale de acces adecvate.

Sugestii de dezvoltare
- Convertirea socket-urilor din `unique_ptr` în `shared_ptr` pentru a simplifica gestiunea duratei de viață între manager și controller.
- Îmbunătățirea protocoalelor admin (folosirea JSON pentru cereri/răspunsuri) pentru interoperabilitate cu UI-uri externe.
- Înlocuirea buclelor ocupate de log cu `condition_variable` pentru a reduce consumul CPU.

Cum contribui
- Creează un branch, fă modificări mici și deschide un pull request. Adaugă descriere clară și un mic test/pași de reproducere.

Licență
- Proiect experimental — folosește-l responsabil. (Adaugă o licență în repo dacă dorești claritate juridică.)

