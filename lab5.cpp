#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

// Functie pentru verificarea daca un numar este prim
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

    int p_to_c[NUM_PROCESE][2]; // Pipe-uri Parinte -> Copil (trimitere interval)
    int c_to_p[NUM_PROCESE][2]; // Pipe-uri Copil -> Parinte (trimitere numere prime)

    // 1. Crearea tuturor pipe-urilor
    for (int i = 0; i < NUM_PROCESE; ++i) {
        if (pipe(p_to_c[i]) == -1 || pipe(c_to_p[i]) == -1) {
            perror("Eroare la crearea pipe-urilor");
            return 1;
        }
    }

    // 2. Crearea celor 10 procese copii
    for (int i = 0; i < NUM_PROCESE; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Eroare la fork");
            return 1;
        }

        if (pid == 0) { // PROCES COPIL
            // Inchidem toate pipe-urile nefolosite de acest copil pentru a evita blocurile de citire
            for (int j = 0; j < NUM_PROCESE; ++j) {
                close(p_to_c[j][1]); // Inchide scrierea P->C
                close(c_to_p[j][0]); // Inchide citirea C->P
                if (j != i) {
                    close(p_to_c[j][0]);
                    close(c_to_p[j][1]);
                }
            }

            // Citim intervalul primit de la parinte [start, end]
            int interval[2];
            read(p_to_c[i][0], interval, sizeof(interval));
            close(p_to_c[i][0]); // Am terminat citirea intervalului

            int start = interval[0];
            int end = interval[1];

            // Cautam numerele prime si le trimitem parintelui prin pipe
            for (int n = start; n <= end; ++n) {
                if (estePrim(n)) {
                    write(c_to_p[i][1], &n, sizeof(n));
                }
            }

            close(c_to_p[i][1]); // Inchidem scrierea (semnaleaza EOF catre parinte)
            _exit(0);            // Incheiem procesul copil
        }
    }

    // 3. PROCES PARINTE: Trimitem intervalele catre fiecare copil
    for (int i = 0; i < NUM_PROCESE; ++i) {
        close(p_to_c[i][0]); // Parintele nu citeste din p_to_c
        close(c_to_p[i][1]); // Parintele nu scrie in c_to_p

        int interval[2] = { i * DIM_INTERVAL + 1, (i + 1) * DIM_INTERVAL };
        write(p_to_c[i][1], interval, sizeof(interval));
        close(p_to_c[i][1]); // Am terminat de scris intervalul
    }

    // 4. PROCES PARINTE: Citim si afisam rezultatele de la fiecare copil
    for (int i = 0; i < NUM_PROCESE; ++i) {
        std::cout << "\n============================================\n";
        std::cout << "Procesul " << (i + 1) << " (Interval: "
            << (i * DIM_INTERVAL + 1) << " - " << ((i + 1) * DIM_INTERVAL) << "):\n";
        std::cout << "============================================\n";

        int num;
        int count = 0;
        // read() returneaza 0 (EOF) cand copilul i inchide pipe-ul de scriere
        while (read(c_to_p[i][0], &num, sizeof(num)) > 0) {
            std::cout << num << " ";
            count++;
        }
        std::cout << "\n(Total numere prime gasite: " << count << ")\n";
        close(c_to_p[i][0]);
    }

    // 5. Asteptam ca toate procesele sa se termine curat
    for (int i = 0; i < NUM_PROCESE; ++i) {
        wait(NULL);
    }

    std::cout << "\nToate cele 10 procese s-au incheiat cu succes!\n";
    return 0;