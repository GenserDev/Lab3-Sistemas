#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <pthread.h>
#include <omp.h>

int sudoku[9][9];
int es_valido = 1;


void validar_filas() {
    omp_set_nested(1);
    omp_set_num_threads(9);
    
    // schedule(dynamic)
    #pragma omp parallel for 
    for (int i = 0; i < 9; i++) {
        int count[10] = {0}; 
        for (int j = 0; j < 9; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || count[num] == 1) {
                #pragma omp critical
                es_valido = 0;
            }
            count[num] = 1;
        }
    }
}

void* validar_columnas(void *arg) {
    pid_t tid = syscall(SYS_gettid);
    printf("El thread que ejecuta el metodo de revision de columnas es: %d\n", tid);
    
    omp_set_nested(1);
    omp_set_num_threads(9);
    
    // schedule(dynamic) 
    #pragma omp parallel for shared(es_valido, sudoku) 
    for (int i = 0; i < 9; i++) {
        pid_t lwp_id = syscall(SYS_gettid);
        printf("En la revision de columnas el siguiente es un thread en ejecucion: %d\n", lwp_id);
        
        int count[10] = {0};
        for (int j = 0; j < 9; j++) {
            int num = sudoku[j][i];
            if (num < 1 || num > 9 || count[num] == 1) {
                #pragma omp critical
                es_valido = 0;
            }
            count[num] = 1;
        }
    }
    pthread_exit(0);
}

void validar_subarreglos(int fila_inicio, int col_inicio) {
    int count[10] = {0};
    for (int i = fila_inicio; i < fila_inicio + 3; i++) {
        for (int j = col_inicio; j < col_inicio + 3; j++) {
            int num = sudoku[i][j];
            if (num < 1 || num > 9 || count[num] == 1) {
                #pragma omp critical
                es_valido = 0;
            }
            count[num] = 1;
        }
    }
}

// Función Principal

int main(int argc, char *argv[]) {
    // Limitamos el número de hilos del main a 1
    omp_set_num_threads(1);
    
    if (argc != 2) {
        printf("Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error abriendo el archivo");
        return 1;
    }
    
    struct stat sb;
    fstat(fd, &sb);
    char *file_in_memory = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    int index = 0;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            sudoku[i][j] = file_in_memory[index] - '0';
            index++;
        }
    }
    munmap(file_in_memory, sb.st_size);
    close(fd);

    for (int i = 0; i <= 6; i += 3) {
        validar_subarreglos(i, i);
    }

    pid_t pid_padre = getpid();
    char pid_str[15];
    sprintf(pid_str, "%d", pid_padre);

    pid_t hijo1 = fork();

    if (hijo1 == 0) {
        execlp("ps", "ps", "-p", pid_str, "-lLf", NULL);
        perror("Error al ejecutar ps");
        exit(1);
    } else if (hijo1 > 0) {
        pthread_t thread_id;
        
        pthread_create(&thread_id, NULL, validar_columnas, NULL);
        pthread_join(thread_id, NULL);
        
        pid_t tid_main = syscall(SYS_gettid);
        printf("El thread en el que se ejecuta main es: %d\n", tid_main);

        waitpid(hijo1, NULL, 0);

        validar_filas();

        if (es_valido) {
            printf("Sudoku resuelto!\n");
        } else {
            printf("El Sudoku no es valido.\n");
        }

        printf("Antes de terminar el estado de este proceso y sus threads es:\n");
        pid_t hijo2 = fork();

        if (hijo2 == 0) {
            execlp("ps", "ps", "-p", pid_str, "-lLf", NULL);
            perror("Error al ejecutar ps");
            exit(1);
        } else {
            waitpid(hijo2, NULL, 0);
        }
    } else {
        perror("Error en el fork");
        return 1;
    }

    return 0;
}