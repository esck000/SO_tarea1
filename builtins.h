#ifndef BUILTINS_H
#define BUILTINS_H

#define BUILTIN_NO 0
#define BUILTIN_OK 1
#define BUILTIN_EXIT 2

int ejecutar_builtin(int argc, char *argv[], int *codigo_salida);

#endif