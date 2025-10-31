#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>

static std::atomic<bool> running(true);

static void handle_sigint(int) { running = false; }

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: log_script <path-to-log-file>\n";
        return 2;
    }
    const char *path = argv[1];
    
    std::string cmd = std::string("tail -F ") + path;
    std::signal(SIGINT, handle_sigint);

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "log_script: failed to start tail on " << path << "\n";
        return 3;
    }

    char buf[1024];
    while (running) {
        if (fgets(buf, sizeof(buf), pipe) != nullptr) {
            std::fputs(buf, stdout);
            std::fflush(stdout);
        } else {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    pclose(pipe);
    return 0;
}
