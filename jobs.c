#define _POSIX_C_SOURCE 200809L     // Habilita sigaction() y otras funciones POSIX.

#include <stdio.h>
#include <string.h>       // memset()
#include <signal.h>       // sigaction(), sigprocmask()
#include <sys/wait.h>     // waitpid()
#include "jobs.h"

#define MAX_PROCESOS_JOB 64     // Máximo de procesos por job (pipelines).


typedef struct { // Estructura para almacenar información de los trabajos en segundo plano.
    int numero;                     // Número de job que ve el usuario: [1], [2] y asi 

    pid_t pids[MAX_PROCESOS_JOB];   // PIDs del job, 0 = ese proceso ya terminó.
    int cantidad_pids;           // Cuántos procesos tiene el job.
    int procesos_restantes;         // Cuántos siguen vivos.

    char comando[1024];    // Texto del comando, para jobs y Done.

    int activo;        // 1 si el job sigue corriendo.
    int terminado;       // 1 si terminó y falta avisar con "Done".
} Job;

static Job jobs[MAX_JOBS];    //Tabla de jobs.
static int siguiente_job = 1;   // Próximo número de job.

// Registra un job de un solo proceso. Retorna su número, o -1 si la tabla está llena.
int agregar_job(pid_t pid, char *argv[], int argc)
{
    for (int i = 0; i < MAX_JOBS; i++) { //itera a través de todos los trabajos en segundo plano.

        if (!jobs[i].activo && !jobs[i].terminado) { // Si encontramos un trabajo inactivo y no terminado, lo usamos para almacenar el nuevo trabajo.
            jobs[i].numero = siguiente_job;     // Asignamos un número único al trabajo.
            jobs[i].pids[0] = pid;
            jobs[i].cantidad_pids = 1;
            jobs[i].procesos_restantes = 1;
            jobs[i].activo = 1;                 // Marcamos el trabajo como activo.
            jobs[i].terminado = 0;    // Marcamos el trabajo como no terminado.

            jobs[i].comando[0] = '\0';     // Inicializamos el comando como una cadena vacía.

            size_t usados = 0;

            for (int j = 0; j < argc; j++) {   //Reconstruimos el comando original a partir de los argumentos.

                int escritos = snprintf(
                    jobs[i].comando + usados,
                    sizeof(jobs[i].comando) - usados,
                    "%s%s",
                    (j == 0) ? "" : " ",
                    argv[j]
                );

                if (escritos < 0) {    // Si ocurre un error al escribir en la cadena, salimos del bucle.
                    break;
                }

                if ((size_t)escritos >= sizeof(jobs[i].comando) - usados) { //Si la cadena se ha llenado, salimos del bucle.
                    break;
                }

                usados += (size_t)escritos;      // Actualizamos la cantidad de caracteres usados en la cadena.
            }

            siguiente_job++;

            return jobs[i].numero;
        }
    }

    return -1;
}

// Registra un pipeline en background como un solo job. Retorna su número, o -1 si no hay espacio.
int agregar_job_pipeline(pid_t pids[], int cantidad_pids, Pipeline *pipeline)
{
    if (cantidad_pids <= 0 || cantidad_pids > MAX_PROCESOS_JOB) {   //cantidad de procesos inválida.
        return -1;
    }

    for (int i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].activo && !jobs[i].terminado) {

            jobs[i].numero = siguiente_job;
            jobs[i].activo = 1;
            jobs[i].terminado = 0;

            jobs[i].cantidad_pids = cantidad_pids;
            jobs[i].procesos_restantes = cantidad_pids;

            for (int j = 0; j < cantidad_pids; j++) {       // Guardamos los PIDs.
                jobs[i].pids[j] = pids[j];
            }

            jobs[i].comando[0] = '\0';
            size_t usados = 0;

            for (int c = 0; c < pipeline->cantidad; c++) {  // Reconstruimos el texto del pipeline.

                if (c > 0) {    // Separamos los comandos con |.
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

// Llena procesos[] con el PID y el comando de cada proceso vivo (lo usa pmon).
// Bloquea SIGCHLD mientras lee la tabla. Retorna cuántos guardó, o -1 si falla.
int obtener_procesos_activos(ProcesoJobInfo procesos[], int max_procesos)
{
    if (procesos == NULL || max_procesos <= 0) {    // No hay dónde guardar.
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

    int cantidad = 0;       // Cuántos procesos llevamos guardados.

    for (int i = 0; i < MAX_JOBS && cantidad < max_procesos; i++) {

        if (!jobs[i].activo) {
            continue;
        }

        for (int j = 0;
             j < jobs[i].cantidad_pids && cantidad < max_procesos;
             j++) {

            if (jobs[i].pids[j] == 0) {     // Ese proceso ya terminó.
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

// Comando muestra los jobs que siguen corriendo. Bloquea SIGCHLD mientras lee la tabla.
void mostrar_jobs(void)
{
    sigset_t mascara_chld;
    sigset_t mascara_anterior;

    sigemptyset(&mascara_chld);
    sigaddset(&mascara_chld, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mascara_chld, &mascara_anterior) < 0) {
        perror("sigprocmask");
        return;
    }

    for (int i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].activo) {

            pid_t pid_activo = 0;       // Primer proceso vivo del job.

            for (int j = 0; j < jobs[i].cantidad_pids; j++) {

                if (jobs[i].pids[j] != 0) {
                    pid_activo = jobs[i].pids[j];
                    break;
                }
            }

            printf("[%d] %d Ejecutando %s\n",
                jobs[i].numero,
                pid_activo,
                jobs[i].comando);
        }
    }

    if (sigprocmask(SIG_SETMASK, &mascara_anterior, NULL) < 0) {
        perror("sigprocmask");
    }
}


// Marca el proceso pid como terminado. Cuando terminan todos los de un job, el job pasa a terminado.
// Solo la llama el manejador de SIGCHLD.
static void marcar_job_terminado(pid_t pid)
{
    for (int i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].activo) {
            continue;
        }

        for (int j = 0; j < jobs[i].cantidad_pids; j++) {

            if (jobs[i].pids[j] == pid) {

                jobs[i].pids[j] = 0;                // Este proceso terminó.
                jobs[i].procesos_restantes--;

                if (jobs[i].procesos_restantes == 0) {  // Terminaron todos: el job pasa a terminado.
                    jobs[i].activo = 0;
                    jobs[i].terminado = 1;
                }

                return;
            }
        }
    }
}

// Avisa "[n]+ Done comando" de los jobs que terminaron y libera su lugar en la tabla.
// Se llama antes de cada prompt. Bloquea SIGCHLD mientras lee la tabla.
void mostrar_jobs_terminados(void)
{
    sigset_t mascara_chld;
    sigset_t mascara_anterior;

    sigemptyset(&mascara_chld);
    sigaddset(&mascara_chld, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mascara_chld, &mascara_anterior) < 0) {
        perror("sigprocmask");
        return;
    }

    for (int i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].terminado) {        // Ya terminó y todavía falta avisar.

            printf("[%d]+ Done %s\n",
                   jobs[i].numero,
                   jobs[i].comando);

            jobs[i].terminado = 0;
        }
    }

    if (sigprocmask(SIG_SETMASK, &mascara_anterior, NULL) < 0) {
        perror("sigprocmask");
    }
}

// Manejador de SIGCHLD: recoge con waitpid() todos los hijos terminados, sin bloquear.
static void manejar_sigchld(int signo)
{
    (void)signo;    // No usamos el número de señal.

    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {//Mientras haya procesos hijos que hayan terminado, los marcamos como terminados en la lista de trabajos.
        marcar_job_terminado(pid);
    }
}

// Instala el manejador de SIGCHLD con sigaction().
void configurar_sigchld(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));   // Dejamos la estructura en cero.

    sa.sa_handler = manejar_sigchld;  // Función que se ejecuta al recibir SIGCHLD.

    sigemptyset(&sa.sa_mask);   // No bloqueamos otras señales dentro del manejador.

    sa.sa_flags = SA_RESTART;    // Reinicia las llamadas al sistema interrumpidas (como fgets).

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction SIGCHLD");
    }
}