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

        if (!background) {

            struct sigaction sa;

            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            if (sigaction(SIGINT, &sa, NULL) < 0) {
                perror("sigaction SIGINT hijo");
                _exit(1);
            }
        }
        
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

    if (sigprocmask(SIG_SETMASK, &mascara_anterior, NULL) < 0) {
        perror("sigprocmask");
        return -1;
    }

    int status;

    if (waitpid(pid, &status, 0) < 0 && errno != ECHILD) {        // Si el comando se ejecuta en primer plano, esperamos a que termine y mostramos un mensaje de error si waitpid() falla.
        perror("waitpid");
    }

    return 0;
}

int ejecutar_pipeline(Pipeline *pipeline)
{
    if (pipeline == NULL || pipeline->cantidad == 0) {
        return 0;
    }

    /*
    Si solamente hay un comando, seguimos usando el ejecutor
    que ya tenemos y que sabemos que funciona.
     */
    if (pipeline->cantidad == 1) {

        Comando *comando = &pipeline->comandos[0];

        return ejecutar_comando(
            comando->argv,
            comando->argc,
            pipeline->background,
            &comando->redirecciones
        );
    }

    sigset_t mascara_chld;
    sigset_t mascara_anterior;

    sigemptyset(&mascara_chld);
    sigaddset(&mascara_chld, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mascara_chld, &mascara_anterior) < 0) {
        perror("sigprocmask");      
        return -1;
    }

    int cantidad_pipes = pipeline->cantidad - 1;
    int pipes[MAX_COMANDOS - 1][2];

    for (int i = 0; i < cantidad_pipes; i++) {

        if (pipe(pipes[i]) < 0) {
            perror("pipe");

            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

            return -1;
        }
    }

    pid_t pids[MAX_COMANDOS];

    for (int i = 0; i < pipeline->cantidad; i++) {

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");

            for (int j = 0; j < cantidad_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            for (int j = 0; j < i; j++) {

                if (kill(pids[j], SIGKILL) < 0 && errno != ESRCH) {
                    perror("kill");
                }
            }

            for (int j = 0; j < i; j++) {

                if (waitpid(pids[j], NULL, 0) < 0 && errno != ECHILD) {
                    perror("waitpid");
                }
            }

            sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

            return -1;
        }

        if (pid == 0) {

            sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

            if (!pipeline->background) {

                struct sigaction sa;

                sa.sa_handler = SIG_DFL;
                sigemptyset(&sa.sa_mask);
                sa.sa_flags = 0;

                if (sigaction(SIGINT, &sa, NULL) < 0) {
                    perror("sigaction SIGINT hijo");
                    _exit(1);
                }
            }

            /*
            * Si no somos el primer comando,
            * nuestra entrada viene del pipe anterior.
            */
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }

            /*
            * Si no somos el último comando,
            * nuestra salida va al pipe siguiente.
            */
            if (i < pipeline->cantidad - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }

            /*
            * Después de dup2(), el hijo ya no necesita
            * ninguno de los descriptores originales de los pipes.
            */
            for (int j = 0; j < cantidad_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            Comando *comando = &pipeline->comandos[i];

            if (aplicar_redirecciones(&comando->redirecciones) < 0) {
                _exit(1);
            }

            execvp(comando->argv[0], comando->argv);

            perror("execvp");
            _exit(127);
        }

        pids[i] = pid;
    }

    for (int i = 0; i < cantidad_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (pipeline->background) {

        int numero_job = agregar_job_pipeline(
            pids,
            pipeline->cantidad,
            pipeline
        );

        if (numero_job < 0) {
            fprintf(stderr, "No hay espacio para mas jobs\n");
        } else {
            printf("[%d] %d\n", numero_job, pids[0]);
        }

        sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

        return 0;
    }
    
    if (sigprocmask(SIG_SETMASK, &mascara_anterior, NULL) < 0) {
        perror("sigprocmask");
        return -1;
    }

    for (int i = 0; i < pipeline->cantidad; i++) {

        if (waitpid(pids[i], NULL, 0) < 0 && errno != ECHILD) {
            perror("waitpid");
        }
    }

    return 0;
}