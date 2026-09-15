#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64

int agregar_job(pid_t pid, char *argv[], int argc);

void mostrar_jobs(void);
void mostrar_jobs_terminados(void);

void configurar_sigchld(void);

#endif