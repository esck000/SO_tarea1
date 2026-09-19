#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include "parser.h"

#define MAX_JOBS 64

typedef struct {
    pid_t pid;
    char comando[1024];
} ProcesoJobInfo;

int agregar_job(pid_t pid, char *argv[], int argc);

int agregar_job_pipeline(pid_t pids[], int cantidad_pids, Pipeline *pipeline);

int obtener_procesos_activos(ProcesoJobInfo procesos[], int max_procesos);

void mostrar_jobs(void);
void mostrar_jobs_terminados(void);

void configurar_sigchld(void);

#endif