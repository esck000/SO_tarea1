#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "parser.h"      // Redirecciones y Pipeline

// Ejecuta un solo comando con fork() + execvp(). Si background es 1, no espera a que termine.
// Retorna 0 si todo salió bien y -1 en caso de error.
int ejecutar_comando(char *argv[], int argc, int background, Redirecciones *redirecciones);

// Ejecuta un pipeline de 1 a N comandos, un hijo por comando. Retorna 0 si todo salió bien y -1 en caso de error.
int ejecutar_pipeline(Pipeline *Pipeline);

#endif