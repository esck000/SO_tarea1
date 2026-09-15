#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "jobs.h"


typedef struct { // Estructura para almacenar información de los trabajos en segundo plano.
    int numero;
    pid_t pid;
    char comando[1024];
    int activo;
    int terminado;
} Job;

static Job jobs[MAX_JOBS];
static int siguiente_job = 1;

int agregar_job(pid_t pid, char *argv[], int argc)
{
    for (int i = 0; i < MAX_JOBS; i++) { // Itera a través de todos los trabajos en segundo plano.

        if (!jobs[i].activo && !jobs[i].terminado) { // Si encontramos un trabajo inactivo y no terminado, lo usamos para almacenar el nuevo trabajo.
            jobs[i].numero = siguiente_job;     // Asignamos un número único al trabajo.
            jobs[i].pid = pid;                  // Guardamos el PID del proceso hijo.
            jobs[i].activo = 1;                 // Marcamos el trabajo como activo.
            jobs[i].terminado = 0;              // Marcamos el trabajo como no terminado.

            jobs[i].comando[0] = '\0';          // Inicializamos el comando como una cadena vacía.

            size_t usados = 0;

            for (int j = 0; j < argc; j++) {   // Reconstruimos el comando original a partir de los argumentos.

                int escritos = snprintf(
                    jobs[i].comando + usados,
                    sizeof(jobs[i].comando) - usados,
                    "%s%s",
                    (j == 0) ? "" : " ",
                    argv[j]
                );

                if (escritos < 0) {           // Si ocurre un error al escribir en la cadena, salimos del bucle.
                    break;
                }

                if ((size_t)escritos >= sizeof(jobs[i].comando) - usados) {  // Si la cadena se ha llenado, salimos del bucle.
                    break;
                }

                usados += (size_t)escritos;                  // Actualizamos la cantidad de caracteres usados en la cadena.
            }

            siguiente_job++;

            return jobs[i].numero;
        }
    }

    return -1;
}

void mostrar_jobs(void)
{
    for (int i = 0; i < MAX_JOBS; i++) { // Itera a través de todos los trabajos en segundo plano.

        if (jobs[i].activo) {          // Si el trabajo está activo, lo mostramos.
            printf("[%d] Ejecutando %s\n",
                   jobs[i].numero,
                   jobs[i].comando);
        }
    }
}


static void marcar_job_terminado(pid_t pid)
{
    for (int i = 0; i < MAX_JOBS; i++) {    
        if (jobs[i].activo && jobs[i].pid == pid) { // Si encontramos el trabajo con el PID especificado, lo marcamos como inactivo.
            jobs[i].activo = 0;
            jobs[i].terminado = 1;
            break;
        }
    }
}

void mostrar_jobs_terminados(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].terminado) {

            printf("[%d]+ Done %s\n",
                   jobs[i].numero,
                   jobs[i].comando);

            jobs[i].terminado = 0;
        }
    }
}

static void manejar_sigchld(int signo)
{
    (void)signo;

    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {  // Mientras haya procesos hijos que hayan terminado, los marcamos como terminados en la lista de trabajos.
        marcar_job_terminado(pid);
    }
}

void configurar_sigchld(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = manejar_sigchld;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction SIGCHLD");
    }
}