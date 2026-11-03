#include <iostream>

#ifdef _WIN32
// ==================== IMPLEMENTARE PENTRU WINDOWS (VISUAL STUDIO) ====================
#include <windows.h>
#include <string>

bool estePrim(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    // Verificam daca executabilul a fost lansat ca PROCES COPIL
    if (argc > 3 && std::string(argv[1]) == "child") {
        HANDLE hRead = (HANDLE)std::stoull(argv[2]);
        HANDLE hWrite = (HANDLE)std::stoull(argv[3]);

        int interval[2];
        DWORD bytesRead;
        if (ReadFile(hRead, interval, sizeof(interval), &bytesRead, NULL) && bytesRead == sizeof(interval)) {
            int start = interval[0];
            int end = interval[1];

            for (int n = start; n <= end; ++n) {
                if (estePrim(n)) {
                    DWORD bytesWritten;
                    WriteFile(hWrite, &n, sizeof(n), &bytesWritten, NULL);
                }
            }
        }

        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 0;
    }

    // PROCES PARINTE
    const int NUM_PROCESE = 10;
    const int DIM_INTERVAL = 1000;

    HANDLE hProcesses[NUM_PROCESE];
    HANDLE hC2P_Read_Arr[NUM_PROCESE];

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE; // Permite copilului sa mosteneasca pipe-urile
    saAttr.lpSecurityDescriptor = NULL;

    for (int i = 0; i < NUM_PROCESE; ++i) {
        HANDLE hP2C_Read, hP2C_Write;
        HANDLE hC2P_Read, hC2P_Write;

        if (!CreatePipe(&hP2C_Read, &hP2C_Write, &saAttr, 0) ||
            !CreatePipe(&hC2P_Read, &hC2P_Write, &saAttr, 0)) {
            std::cerr << "Eroare la crearea pipe-urilor!\n";
            return 1;
        }

        // Prevenim moștenirea capetelor folosite doar de părinte
        SetHandleInformation(hP2C_Write, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hC2P_Read, HANDLE_FLAG_INHERIT, 0);

        // Lansam o noua instanta a programului cu argumentul "child" si handlerele pipe-urilor
        std::string cmd = "\"" + std::string(argv[0]) + "\" child " +
            std::to_string((uintptr_t)hP2C_Read) + " " +
            std::to_string((uintptr_t)hC2P_Write);

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            std::cerr << "Eroare la CreateProcess!\n";
            return 1;
        }

        // Inchidem capetele folosite de copil in procesul parinte
        CloseHandle(hP2C_Read);
        CloseHandle(hC2P_Write);

        // Trimitem intervalul de 1000 de numere catre copil
        int interval[2] = { i * DIM_INTERVAL + 1, (i + 1) * DIM_INTERVAL };
        DWORD bytesWritten;
        WriteFile(hP2C_Write, interval, sizeof(interval), &bytesWritten, NULL);
        CloseHandle(hP2C_Write); // Inchidem scrierea pentru a semnala finalul transmisiei

        hProcesses[i] = pi.hProcess;
        hC2P_Read_Arr[i] = hC2P_Read;
        CloseHandle(pi.hThread);
    }

    // Citim numerele prime trimise de fiecare copil
    for (int i = 0; i < NUM_PROCESE; ++i) {
        std::cout << "\n============================================\n";
        std::cout << "Procesul " << (i + 1) << " (Interval: "
            << (i * DIM_INTERVAL + 1) << " - " << ((i + 1) * DIM_INTERVAL) << "):\n";
        std::cout << "============================================\n";

        int num;
        DWORD bytesRead;
        int count = 0;
        while (ReadFile(hC2P_Read_Arr[i], &num, sizeof(num), &bytesRead, NULL) && bytesRead > 0) {
            std::cout << num << " ";
            count++;
        }
        std::cout << "\n(Total numere prime gasite: " << count << ")\n";
        CloseHandle(hC2P_Read_Arr[i]);
    }

    // Asteptam finalizarea tuturor celor 10 procese
    WaitForMultipleObjects(NUM_PROCESE, hProcesses, TRUE, INFINITE);

    for (int i = 0; i < NUM_PROCESE; ++i) {
        CloseHandle(hProcesses[i]);
    }

    std::cout << "\nToate cele 10 procese s-au incheiat cu succes!\n";
    return 0;
}

#else
// ==================== IMPLEMENTARE PENTRU LINUX / UBUNTU ====================
#include <unistd.h>
#include <sys/wait.h>

bool estePrim(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

int main() {
    const int NUM_PROCESE = 10;
    const int DIM_INTERVAL = 1000;

    int p_to_c[NUM_PROCESE][2];
    int c_to_p[NUM_PROCESE][2];

    for (int i = 0; i < NUM_PROCESE; ++i) {
        if (pipe(p_to_c[i]) == -1 || pipe(c_to_p[i]) == -1) {
            perror("Eroare la crearea pipe-urilor");
            return 1;
        }
    }

    for (int i = 0; i < NUM_PROCESE; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Eroare la fork");
            return 1;
        }

        if (pid == 0) { // Proces Copil
            for (int j = 0; j < NUM_PROCESE; ++j) {
                close(p_to_c[j][1]);
                close(c_to_p[j][0]);
                if (j != i) {
                    close(p_to_c[j][0]);
                    close(c_to_p[j][1]);
                }
            }

            int interval[2];
            read(p_to_c[i][0], interval, sizeof(interval));
            close(p_to_c[i][0]);

            int start = interval[0];
            int end = interval[1];

            for (int n = start; n <= end; ++n) {
                if (estePrim(n)) {
                    write(c_to_p[i][1], &n, sizeof(n));
                }
            }

            close(c_to_p[i][1]);
            _exit(0);
        }
    }

    // Proces Parinte
    for (int i = 0; i < NUM_PROCESE; ++i) {
        close(p_to_c[i][0]);
        close(c_to_p[i][1]);

        int interval[2] = { i * DIM_INTERVAL + 1, (i + 1) * DIM_INTERVAL };
        write(p_to_c[i][1], interval, sizeof(interval));
        close(p_to_c[i][1]);
    }

    for (int i = 0; i < NUM_PROCESE; ++i) {
        std::cout << "\n============================================\n";
        std::cout << "Procesul " << (i + 1) << " (Interval: "
            << (i * DIM_INTERVAL + 1) << " - " << ((i + 1) * DIM_INTERVAL) << "):\n";
        std::cout << "============================================\n";

        int num;
        int count = 0;
        while (read(c_to_p[i][0], &num, sizeof(num)) > 0) {
            std::cout << num << " ";
            count++;
        }
        std::cout << "\n(Total numere prime gasite: " << count << ")\n";
        close(c_to_p[i][0]);
    }

    for (int i = 0; i < NUM_PROCESE; ++i) {
        wait(NULL);
    }

    std::cout << "\nToate cele 10 procese s-au incheiat cu succes!\n";
    return 0;
}
#endif