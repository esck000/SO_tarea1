#include <string.h>
#include "parser.h"
#include <stdio.h>


int parsear_linea(char *linea, char *argv[], int *background, Redirecciones *redirecciones)
{
    int argc = 0;

    *background = 0;
    redirecciones->entrada = NULL;    // Inicializamos las redirecciones de entrada y salida a NULL y el flag de append a 0.
    redirecciones->salida = NULL;     // redirecciones->salida es equivalente a (*redirecciones).salida
    redirecciones->append = 0;

    size_t largo = strlen(linea);

    while (largo > 0 &&
           (linea[largo - 1] == ' ' ||
            linea[largo - 1] == '\t' ||
            linea[largo - 1] == '\n')) {

        linea[largo - 1] = '\0';
        largo--;
    }

    if (largo > 0 && linea[largo - 1] == '&') {
        *background = 1;
        linea[largo - 1] = '\0';
    }

    char *token = strtok(linea, " \t\n");

    while (token != NULL && argc < MAX_ARGS - 1) {

        if (strcmp(token, "<") == 0) {

            token = strtok(NULL, " \t\n");

            if (token == NULL) {
                fprintf(stderr, "Error: falta archivo despues de <\n");
                return -1;
            }

            redirecciones->entrada = token;
        }

        else if (strcmp(token, ">") == 0) {

            token = strtok(NULL, " \t\n");

            if (token == NULL) {
                fprintf(stderr, "Error: falta archivo despues de >\n");
                return -1;
            }

            redirecciones->salida = token;
            redirecciones->append = 0;
        }

        else if (strcmp(token, ">>") == 0) {

            token = strtok(NULL, " \t\n");

            if (token == NULL) {
                fprintf(stderr, "Error: falta archivo despues de >>\n");
                return -1;
            }

            redirecciones->salida = token;
            redirecciones->append = 1;
        }

        else {
            argv[argc] = token;
            argc++;
        }

        token = strtok(NULL, " \t\n");
    }

    argv[argc] = NULL;

    return argc;
}