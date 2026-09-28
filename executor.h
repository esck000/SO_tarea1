#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "parser.h"      // Redirecciones y Pipeline

//se usa los hijos antes de execvp(), y mishell.c para los built-ins.
//Retorna 0 si todo salio bien y -1 si falla open() o dup2().
int aplicar_redirecciones(Redirecciones *redirecciones);

// Ejecuta un solo comando con fork() + execvp(). Si background es 1, no espera a que termine.
// Retorna 0 si todo salió bien y -1 en caso de error.
int ejecutar_comando(char *argv[], int argc, int background, Redirecciones *redirecciones);

// Ejecuta un pipeline de 1 a N comandos, un hijo por comando. Retorna 0 si todo salió bien y -1 en caso de error.
int ejecutar_pipeline(Pipeline *Pipeline);

#endif