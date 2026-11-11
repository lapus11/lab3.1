#include <iostream>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
// ==================== IMPLEMENTARE WINDOWS (VISUAL STUDIO) ====================
#include <windows.h>
#include <string>

struct SharedData {
    int current_value;
};

void ruleazaProces(int process_id, SharedData* data, HANDLE hSem) {
    srand((unsigned int)time(NULL) ^ (process_id * 54321));

    while (true) {
        // Rezervam semaforul
        WaitForSingleObject(hSem, INFINITE);

        // Verificam daca s-a atins limita de 1000
        if (data->current_value >= 1000) {
            ReleaseSemaphore(hSem, 1, NULL);
            break;
        }

        // 1. Citesc memoria
        int val = data->current_value;

        // 2. Dau cu banul (1 sau 2). Cat timp cade 2, scriu in memorie urmatorul numar
        while (data->current_value < 1000) {
            int ban = (rand() % 2) + 1; // Genereaza 1 sau 2
            if (ban == 2) {
                data->current_value++;
                std::cout << "[Proces " << process_id << "] Banul a fost 2! A scris in memorie: "
                    << data->current_value << "\n";
            }
            else {
                // A cazut 1 -> se opreste din scris pe aceasta tura
                break;
            }
        }

        // 3. Elibereaza semaforul
        ReleaseSemaphore(hSem, 1, NULL);

        // Pauza scurta pentru a da sansa si celuilalt proces sa prinda semaforul
        Sleep(1);
    }
}

int main(int argc, char* argv[]) {
    // Procesul copil lansat de procesul parinte
    if (argc > 1 && std::string(argv[1]) == "child") {
        HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\Lab6SharedMem");
        HANDLE hSem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "Local\\Lab6Sem");

        if (hMap && hSem) {
            SharedData* data = (SharedData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
            if (data) {
                ruleazaProces(2, data, hSem);
                UnmapViewOfFile(data);
            }
            CloseHandle(hMap);
            CloseHandle(hSem);
        }
        return 0;
    }

    // Procesul parinte
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedData), "Local\\Lab6SharedMem");
    HANDLE hSem = CreateSemaphoreA(NULL, 1, 1, "Local\\Lab6Sem");

    if (!hMap || !hSem) {
        std::cerr << "Eroare la crearea resurselor partajate pe Windows!\n";
        return 1;
    }

    SharedData* data = (SharedData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    data->current_value = 0;

    // Lansam al doilea proces
    std::string cmd = "\"" + std::string(argv[0]) + "\" child";
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Eroare la crearea procesului copil!\n";
        return 1;
    }

    // Procesul 1 (Parinte) executa si el numaratul
    ruleazaProces(1, data, hSem);

    // Asteptam finalizarea procesului copil
    WaitForSingleObject(pi.hProcess, INFINITE);

    std::cout << "\n[SUCCESS] Numararea pana la 1000 s-a incheiat cu succes in memorie!\n";

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hSem);

    return 0;
}

#else
// ==================== IMPLEMENTARE LINUX / UBUNTU ====================
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>

struct SharedData {
    int current_value;
    sem_t sem; // Semafor POSIX partajat intre procese
};

void ruleazaProces(int process_id, SharedData* data) {
    srand((unsigned int)time(NULL) ^ (process_id * 54321));

    while (true) {
        // Rezervam semaforul
        sem_wait(&data->sem);

        // Verificam daca s-a atins limita de 1000
        if (data->current_value >= 1000) {
            sem_post(&data->sem);
            break;
        }

        // 1. Citesc memoria
        int val = data->current_value;

        // 2. Dau cu banul (1 sau 2). Cat timp cade 2, scriu in memorie urmatorul numar
        while (data->current_value < 1000) {
            int ban = (rand() % 2) + 1; // Genereaza 1 sau 2
            if (ban == 2) {
                data->current_value++;
                std::cout << "[Proces " << process_id << "] Banul a fost 2! A scris in memorie: "
                    << data->current_value << "\n";
            }
            else {
                // A cazut 1 -> se opreste din scris pe aceasta tura
                break;
            }
        }

        // 3. Elibereaza semaforul
        sem_post(&data->sem);

        // Pauza scurta
        usleep(1000);
    }
}

int main() {
    // Alocam memorie partajata (mmap)
    SharedData* data = (SharedData*)mmap(NULL, sizeof(SharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (data == MAP_FAILED) {
        perror("Eroare mmap");
        return 1;
    }

    // Initializam semaforul (pshared = 1 pentru sincronizare intre procese)
    sem_init(&data->sem, 1, 1);
    data->current_value = 0;

    pid_t pid = fork();

    if (pid < 0) {
        perror("Eroare la fork");
        return 1;
    }

    if (pid == 0) {
        // === PROCESUL 2 (COPIL) ===
        ruleazaProces(2, data);
        _exit(0);
    }
    else {
        // === PROCESUL 1 (PARINTE) ===
        ruleazaProces(1, data);

        // Asteptam copilul
        wait(NULL);

        std::cout << "\n[SUCCESS] Numararea pana la 1000 s-a incheiat cu succes in memorie!\n";

        sem_destroy(&data->sem);
        munmap(data, sizeof(SharedData));
    }

    return 0;
}
#endif