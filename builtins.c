#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "jobs.h"

int ejecutar_builtin(int argc, char *argv[], int *codigo_salida)
{
    if (strcmp(argv[0], "cd") == 0) {   // Si el comando es "cd", cambiamos el directorio de trabajo actual.

        const char *directorio;

        if (argc > 1) {             // Si se proporciona un argumento, lo usamos como el directorio al que cambiar.
            directorio = argv[1];
        } else {
            directorio = getenv("HOME");  // Si no se proporciona un argumento, usamos la variable de entorno HOME como el directorio al que cambiar.
        }

        if (directorio == NULL) {
            fprintf(stderr, "cd: variable HOME no definida\n");
            return BUILTIN_OK;
        }

        if (chdir(directorio) < 0) {        // Si chdir() falla, mostramos un mensaje de error.
            perror("cd");
        }

        return BUILTIN_OK;
    }

    if (strcmp(argv[0], "jobs") == 0) {     // Si el comando es "jobs", mostramos la lista de trabajos en segundo plano.
        mostrar_jobs();
        return BUILTIN_OK;
    }

    if (strcmp(argv[0], "exit") == 0) {    // Si el comando es "exit", terminamos la shell. Si se proporciona un argumento, lo usamos como el código de salida.

        if (argc == 1) {
            *codigo_salida = 0;
        } else {
            *codigo_salida = atoi(argv[1]);
        }

        return BUILTIN_EXIT;
    }

    return BUILTIN_NO;
}
