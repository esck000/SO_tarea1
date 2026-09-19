#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "jobs.h"

#define MAX_PROCESOS_JOB 64


typedef struct { // Estructura para almacenar información de los trabajos en segundo plano.
    int numero;

    pid_t pids[MAX_PROCESOS_JOB];
    int cantidad_pids;
    int procesos_restantes;

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
            jobs[i].pids[0] = pid;
            jobs[i].cantidad_pids = 1;
            jobs[i].procesos_restantes = 1;
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

int agregar_job_pipeline(pid_t pids[], int cantidad_pids, Pipeline *pipeline)
{
    if (cantidad_pids <= 0 || cantidad_pids > MAX_PROCESOS_JOB) {
        return -1;
    }

    for (int i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].activo && !jobs[i].terminado) {

            jobs[i].numero = siguiente_job;
            jobs[i].activo = 1;
            jobs[i].terminado = 0;

            jobs[i].cantidad_pids = cantidad_pids;
            jobs[i].procesos_restantes = cantidad_pids;

            for (int j = 0; j < cantidad_pids; j++) {
                jobs[i].pids[j] = pids[j];
            }

            jobs[i].comando[0] = '\0';
            size_t usados = 0;

            for (int c = 0; c < pipeline->cantidad; c++) {

                if (c > 0) {
                    int escritos = snprintf(
                        jobs[i].comando + usados,
                        sizeof(jobs[i].comando) - usados,
                        " | "
                    );

                    if (escritos < 0 ||
                        (size_t)escritos >= sizeof(jobs[i].comando) - usados) {
                        break;
                    }

                    usados += (size_t)escritos;
                }

                Comando *comando = &pipeline->comandos[c];

                for (int a = 0; a < comando->argc; a++) {

                    int escritos = snprintf(
                        jobs[i].comando + usados,
                        sizeof(jobs[i].comando) - usados,
                        "%s%s",
                        (a == 0) ? "" : " ",
                        comando->argv[a]
                    );

                    if (escritos < 0 ||
                        (size_t)escritos >= sizeof(jobs[i].comando) - usados) {
                        break;
                    }

                    usados += (size_t)escritos;
                }
            }

            siguiente_job++;

            return jobs[i].numero;
        }
    }

    return -1;
}

int obtener_procesos_activos(ProcesoJobInfo procesos[], int max_procesos)
{
    if (procesos == NULL || max_procesos <= 0) {
        return 0;
    }

    sigset_t mascara_chld;
    sigset_t mascara_anterior;

    sigemptyset(&mascara_chld);
    sigaddset(&mascara_chld, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mascara_chld, &mascara_anterior) < 0) {
        perror("sigprocmask");
        return -1;
    }

    int cantidad = 0;

    for (int i = 0; i < MAX_JOBS && cantidad < max_procesos; i++) {

        if (!jobs[i].activo) {
            continue;
        }

        for (int j = 0;
             j < jobs[i].cantidad_pids && cantidad < max_procesos;
             j++) {

            if (jobs[i].pids[j] == 0) {
                continue;
            }

            procesos[cantidad].pid = jobs[i].pids[j];

            snprintf(
                procesos[cantidad].comando,
                sizeof(procesos[cantidad].comando),
                "%s",
                jobs[i].comando
            );

            cantidad++;
        }
    }

    sigprocmask(SIG_SETMASK, &mascara_anterior, NULL);

    return cantidad;
}

void mostrar_jobs(void)
{
    for (int i = 0; i < MAX_JOBS; i++) { // Itera a través de todos los trabajos en segundo plano.

        if (jobs[i].activo) {          // Si el trabajo está activo, lo mostramos.
        printf("[%d] %d Ejecutando %s\n",
            jobs[i].numero,
            jobs[i].pids[0],
            jobs[i].comando);
        }
    }
}


static void marcar_job_terminado(pid_t pid)
{
    for (int i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].activo) {
            continue;
        }

        for (int j = 0; j < jobs[i].cantidad_pids; j++) {

            if (jobs[i].pids[j] == pid) {

                jobs[i].pids[j] = 0;
                jobs[i].procesos_restantes--;

                if (jobs[i].procesos_restantes == 0) {
                    jobs[i].activo = 0;
                    jobs[i].terminado = 1;
                }

                return;
            }
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