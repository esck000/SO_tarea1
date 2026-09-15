#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "parser.h"

int ejecutar_comando(char *argv[], int argc, int background, Redirecciones *redirecciones);

#endif