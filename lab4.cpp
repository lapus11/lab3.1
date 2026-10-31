#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <string>

int main(int argc, char* argv[]) {
    // Dacă este un proces copil pe Windows (a primit secunde ca argument)
    if (argc > 1) {
        int secunde = std::stoi(argv[1]);
        std::cout << "[Copil " << secunde << "] A inceput si asteapta " << secunde << " secunda/secunde...\n";
        Sleep(secunde * 1000); // Milisecunde
        std::cout << "[Copil " << secunde << "] S-a terminat.\n";
        return 0;
    }

    // Procesul parinte pe Windows
    std::cout << "[Parinte] Procesul principal a inceput.\n";
    HANDLE hProcesses[3];
    STARTUPINFOA si[3];
    PROCESS_INFORMATION pi[3];

    for (int i = 0; i < 3; ++i) {
        ZeroMemory(&si[i], sizeof(si[i]));
        si[i].cb = sizeof(si[i]);
        ZeroMemory(&pi[i], sizeof(pi[i]));

        std::string cmd = std::string(argv[0]) + " " + std::to_string(i + 1);
        if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si[i], &pi[i])) {
            std::cerr << "Eroare la crearea procesului copil!\n";
            return 1;
        }
        hProcesses[i] = pi[i].hProcess;
    }

    // Asteapta terminarea tuturor proceselor copii
    WaitForMultipleObjects(3, hProcesses, TRUE, INFINITE);

    for (int i = 0; i < 3; ++i) {
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }

    std::cout << "[Parinte] Toate cele 3 procese s-au terminat!\n";
    return 0;
}

#else
#include <unistd.h>
#include <sys/wait.h>

int main() {
    std::cout << "[Parinte] Procesul principal a inceput.\n";

    for (int i = 1; i <= 3; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "Eroare la crearea procesului!\n";
            return 1;
        }

        if (pid == 0) {
            // Cod executat doar de procesul copil
            std::cout << "[Copil " << i << "] A inceput si asteapta " << i << " secunda/secunde...\n";
            sleep(i);
            std::cout << "[Copil " << i << "] S-a terminat.\n";
            _exit(0);
        }
    }

    // Procesul parinte asteapta sa se termine toate cele 3 procese copii
    for (int i = 0; i < 3; ++i) {
        wait(NULL);
    }

    std::cout << "[Parinte] Toate cele 3 procese s-au terminat!\n";
    return 0;
}
#endif