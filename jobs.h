#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>   // pid_t
#include "parser.h"       // Pipeline

#define MAX_JOBS 64       // Máximo de jobs en background a la vez.

// Un proceso en background, tal como lo lee pmon.
typedef struct {
    pid_t pid;              // PID del proceso.
    char comando[1024];     // Comando del job al que pertenece.
} ProcesoJobInfo;

//Registra un job de un solo proceso. Retorna su número, o -1 si la tabla está llena.
int agregar_job(pid_t pid, char *argv[], int argc);

//registra un pipeline en background como un solo job. Retorna su número, o -1 si no hay espacio.
int agregar_job_pipeline(pid_t pids[], int cantidad_pids, Pipeline *pipeline);

//Llena procesos[] con los procesos vivos (lo usa pmon). Retorna cuántos guardó, o -1 si falla.
int obtener_procesos_activos(ProcesoJobInfo procesos[], int max_procesos);

// Comando "jobs": muestra los jobs que siguen corriendo.
void mostrar_jobs(void);

// Avisa "[n]+ Done comando" de los jobs que terminaron. Se llama antes de cada prompt.
void mostrar_jobs_terminados(void);

// Instala el manejador de SIGCHLD que recoge los hijos terminados.
void configurar_sigchld(void);

#endif