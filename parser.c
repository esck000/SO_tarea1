#include <string.h>
#include "parser.h"
#include <stdio.h>

static int validar_pipeline(const char *linea)
{
    int hay_contenido = 0;

    for (int i = 0; linea[i] != '\0'; i++) {

        if (linea[i] == '|') {

            if (!hay_contenido) {
                return -1;
            }

            hay_contenido = 0;
        }
        else if (linea[i] != ' ' &&
                 linea[i] != '\t' &&
                 linea[i] != '\n') {

            hay_contenido = 1;
        }
    }

    if (!hay_contenido) {
        return -1;
    }

    return 0;
}


static int parsear_comando(char *texto, Comando *comando)
{
    int argc = 0;

    comando->redirecciones.entrada = NULL;
    comando->redirecciones.salida = NULL;
    comando->redirecciones.append = 0;

    char *saveptr = NULL;
    char *token = strtok_r(texto, " \t\n", &saveptr);  

    while (token != NULL && argc < MAX_ARGS - 1) {

        if (strcmp(token, "<") == 0) {

            token = strtok_r(NULL, " \t\n", &saveptr);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de <\n");
                return -1;
            }

            comando->redirecciones.entrada = token;
        }

        else if (strcmp(token, ">") == 0) {

            token = strtok_r(NULL, " \t\n", &saveptr);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de >\n");
                return -1;
            }

            comando->redirecciones.salida = token;
            comando->redirecciones.append = 0;
        }

        else if (strcmp(token, ">>") == 0) {

            token = strtok_r(NULL, " \t\n", &saveptr);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de >>\n");
                return -1;
            }

            comando->redirecciones.salida = token;
            comando->redirecciones.append = 1;
        }

        else {
            comando->argv[argc] = token;
            argc++;
        }

        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    comando->argv[argc] = NULL;
    comando->argc = argc;

    return argc;
}

int parsear_linea(char *linea, Pipeline *pipeline)
{
    pipeline->cantidad = 0;
    pipeline->background = 0;

    size_t largo = strlen(linea);

    while (largo > 0 &&
           (linea[largo - 1] == ' ' ||
            linea[largo - 1] == '\t' ||
            linea[largo - 1] == '\n')) {

        linea[largo - 1] = '\0';
        largo--;
    }
    
    if (largo == 0) {
        return 0;
    }

    if (largo > 0 && linea[largo - 1] == '&') {
        pipeline->background = 1;
        linea[largo - 1] = '\0';
    }

    if (validar_pipeline(linea) < 0) {
        fprintf(stderr, "Error: comando vacio en pipeline\n");
        return -1;
    }

    char *saveptr = NULL;
    char *segmento = strtok_r(linea, "|", &saveptr);

    while (segmento != NULL) {

        if (pipeline->cantidad >= MAX_COMANDOS) {
            fprintf(stderr,
                    "Error: demasiados comandos en el pipeline\n");
            return -1;
        }

        Comando *comando =
            &pipeline->comandos[pipeline->cantidad];

        if (parsear_comando(segmento, comando) < 0) {
            return -1;
        }

        if (comando->argc == 0) {
            fprintf(stderr,
                    "Error: comando vacio en pipeline\n");
            return -1;
        }

        pipeline->cantidad++;

        segmento = strtok_r(NULL, "|", &saveptr);
    }

    return pipeline->cantidad;
}