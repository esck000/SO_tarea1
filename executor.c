#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "executor.h"
#include "jobs.h"

#include <fcntl.h> // Para usar open() y O_CREAT, O_WRONLY, O_TRUNC, O_APPEND
#include <errno.h>

static int aplicar_redirecciones(Redirecciones *redirecciones)
{
    int fd;

    if (redirecciones->entrada != NULL) { // Si hay redirección de entrada, abrimos el archivo y duplicamos el descriptor de archivo en STDIN_FILENO.

        fd = open(redirecciones->entrada, O_RDONLY); // Abrimos el archivo de entrada en modo lectura.

        if (fd < 0) {            // Si open() falla, mostramos un mensaje de error y retornamos -1.
            perror(redirecciones->entrada);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0) {        // Duplicamos el descriptor de archivo en STDIN_FILENO. Si dup2() falla, mostramos un mensaje de error y retornamos -1.
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    if (redirecciones->salida != NULL) { // Si hay redirección de salida, abrimos el archivo y duplicamos el descriptor de archivo en STDOUT_FILENO.

        int flags = O_WRONLY | O_CREAT;  // Abrimos el archivo de salida en modo escritura y creamos el archivo si no existe. Si redirecciones->append es 1, abrimos el archivo en modo append, de lo contrario, truncamos el archivo.

        if (redirecciones->append) { // Si redirecciones->append es 1, abrimos el archivo en modo append, de lo contrario, truncamos el archivo.
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        fd = open(redirecciones->salida, flags, 0644); // Abrimos el archivo de salida con los flags correspondientes y permisos 0644.

        if (fd < 0) {               
            perror(redirecciones->salida);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0) { 
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}



int ejecutar_comando(char *argv[], int argc, int background, Redirecciones *redirecciones)
{
    sigset_t mascara_chld;
    sigset_t mascara_anterior;
    

    sigemptyset(&mascara_chld); // Inicializamos la máscara de señales para bloquear SIGCHLD.
    sigaddset(&mascara_chld, SIGCHLD); // Agregamos SIGCHLD a la máscara de señales.

    if (sigprocmask(SIG_BLOCK, &mascara_chld, &mascara_anterior) < 0) { // Bloqueamos SIGCHLD para evitar que el manejador de señales se ejecute mientras estamos agregando el trabajo a la lista.
        perror("sigprocmask");
        return -1;
    }
    
    pid_t pid = fork();

    if (pid < 0) {                  // Si fork() falla, mostramos un mensaje de error y retornamos -1.
        perror("fork");

        sigprocmask(SIG_SETMASK, &mascara_anterior, NULL); // Restauramos la máscara de señales original antes de retornar.

        return -1;
    }
 
    if (pid == 0) {               // Si estamos en el proceso hijo, ejecutamos el comando usando execvp().
        sigprocmask(SIG_SETMASK, &mascara_anterior, NULL); // Restauramos la máscara de señales original en el proceso hijo antes de ejecutar el comando.

        if (aplicar_redirecciones(redirecciones) < 0) {
            _exit(1);
        }

        execvp(argv[0], argv);

        perror("execvp");
        _exit(127);
    }

    if (background) {             // Si el comando se ejecuta en segundo plano, agregamos el trabajo a la lista de trabajos y mostramos su número y PID.

        int numero_job = agregar_job(pid, argv, argc);

        if (numero_job < 0) {
            fprintf(stderr, "No hay espacio para mas jobs\n");
        } else {
            printf("[%d] %d\n", numero_job, pid);
        }

        sigprocmask(SIG_SETMASK, &mascara_anterior, NULL); // Restauramos la máscara de señales original antes de retornar.

        return 0;
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {        // Si el comando se ejecuta en primer plano, esperamos a que termine y mostramos un mensaje de error si waitpid() falla.
        perror("waitpid");
    }

    sigprocmask(SIG_SETMASK, &mascara_anterior, NULL); // Restauramos la máscara de señales original antes de retornar.

    return 0;
}