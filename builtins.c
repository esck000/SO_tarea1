#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "jobs.h"
#include "pmon.h"

// Revisa si el comando es un built-in (cd, jobs, pmon o exit) y, si lo es, lo ejecuta directamente en el proceso de la shell, sin fork().
// Retorna BUILTIN_OK si el comando se ejecutó, BUILTIN_EXIT si la shell debe terminar (el código de salida queda en *codigo_salida)
// y BUILTIN_NO si no es un built-in, para que la shell lo ejecute con fork() + execvp().
int ejecutar_builtin(int argc, char *argv[], int *codigo_salida)
{
    if (strcmp(argv[0], "cd") == 0) {   // Si el comando es "cd", cambiamos el directorio de trabajo actual.

        const char *directorio;     // Directorio al que vamos a cambiar.

        if (argc > 1) {             // Si se proporciona un argumento, lo usamos como el directorio al que cambiar.
            directorio = argv[1];
        } else {
            directorio = getenv("HOME");  // Si no se proporciona un argumento, usamos la variable de entorno HOME como el directorio al que cambiar.
        }

        if (directorio == NULL) {       // Si no hay argumento y HOME no está definida, no hay a dónde cambiar.
            fprintf(stderr, "cd: variable HOME no definida\n");
            return BUILTIN_OK;
        }

        if (chdir(directorio) < 0) {        // Si chdir() falla, mostramos un mensaje de error.
            perror("cd");
        }

        return BUILTIN_OK;      // Ya manejamos "cd" (haya funcionado o no), así que la shell no debe ejecutarlo con fork() + exec().
    }

    if (strcmp(argv[0], "jobs") == 0) {     // Si el comando es "jobs", mostramos la lista de trabajos en segundo plano.
        mostrar_jobs();
        return BUILTIN_OK;
    }

    if (strcmp(argv[0], "pmon") == 0) {     // Si el comando es "pmon", entramos al modo de monitoreo de los trabajos en segundo plano.

        int segundos = 2;       // Intervalo de refresco por defecto si no se indica otro.

        if (argc > 2) {         // pmon acepta como máximo un argumento: los segundos.
            fprintf(stderr, "Uso: pmon [segundos]\n");
            return BUILTIN_OK;
        }

        if (argc == 2) {

            segundos = atoi(argv[1]);   // atoi() devuelve 0 si el argumento no es un número.

            if (segundos <= 0) {        // Rechazamos 0, negativos y textos que no son números.
                fprintf(stderr, "pmon: segundos debe ser mayor que 0\n");
                return BUILTIN_OK;
            }
        }

        ejecutar_pmon(segundos);    // Muestra la tabla cada "segundos" segundos hasta que el usuario presione Ctrl+C.

        return BUILTIN_OK;
    }

    if (strcmp(argv[0], "exit") == 0) {    // Si el comando es "exit", terminamos la shell. Si se proporciona un argumento, lo usamos como el código de salida.

        if (argc == 1) {                    // Sin argumento, el código de salida es 0.
            *codigo_salida = 0;
        } else {
            *codigo_salida = atoi(argv[1]);
        }

        return BUILTIN_EXIT;                // Le avisamos a main() que debe terminar la shell con ese código.
    }

    return BUILTIN_NO;      // No es un built-in: la shell lo ejecutará como un programa externo.
}

int es_builtin(const char *comando)
{
    return strcmp(comando, "cd") == 0 ||
           strcmp(comando, "jobs") == 0 ||
           strcmp(comando, "pmon") == 0 ||
           strcmp(comando, "exit") == 0;
}