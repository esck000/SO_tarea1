#include <string.h>
#include "parser.h"
#include <stdio.h>

//Las comillas simples o dobles agrupan texto en un solo token y hacen
//que |, <, > queden como simples caracteres dentro del string
 // Osea todo lo que etse dentro de las comillas, no importa el caracter, se imprimrá como string
static int validar_pipeline(const char *linea)
{
    int hay_contenido = 0;
    char comilla = '\0';

    for (int i = 0; linea[i] != '\0'; i++) {

        if (comilla != '\0') {

            if (linea[i] == comilla) {
                comilla = '\0';
            }
        }
        else if (linea[i] == '"' || linea[i] == '\'') {

            comilla = linea[i];
            hay_contenido = 1;
        }
        else if (linea[i] == '|') {

            if (!hay_contenido) {
                fprintf(stderr,
                        "Error: comando vacio en pipeline\n");
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

    if (comilla != '\0') {
        fprintf(stderr, "Error: comilla sin cerrar\n");
        return -1;
    }

    if (!hay_contenido) {
        fprintf(stderr, "Error: comando vacio en pipeline\n");
        return -1;
    }

    return 0;
}

//busca el | que no este entre comillas, y devueve las llamadas que esten entre estos
static char *siguiente_segmento(char **cursor)
{
    char *inicio = *cursor;

    if (inicio == NULL) {
        return NULL;
    }

    char comilla = '\0';

    for (char *p = inicio; *p != '\0'; p++) {

        if (comilla != '\0') {

            if (*p == comilla) {
                comilla = '\0';
            }
        }
        else if (*p == '"' || *p == '\'') {
            comilla = *p;
        }
        else if (*p == '|') {

            *p = '\0';
            *cursor = p + 1;

            return inicio;
        }
    }

    *cursor = NULL;

    return inicio;
}

/*
 * Devuelve el siguiente token separado por espacios, tabs o saltos de
 * linea, quitando las comillas en el mismo buffer. *tuvo_comillas indica
 * si el token traia comillas: en ese caso un ">" es texto y no operador.
 */
static char *siguiente_token(char **cursor, int *tuvo_comillas)
{
    char *lectura = *cursor;

    *tuvo_comillas = 0;

    while (*lectura == ' ' || *lectura == '\t' || *lectura == '\n') {
        lectura++;
    }

    if (*lectura == '\0') {
        *cursor = lectura;
        return NULL;
    }

    char *inicio = lectura;
    char *escritura = lectura;
    char comilla = '\0';

    while (*lectura != '\0') {

        if (comilla != '\0') {

            if (*lectura == comilla) {
                comilla = '\0';
                lectura++;
                continue;
            }
        }
        else if (*lectura == '"' || *lectura == '\'') {

            comilla = *lectura;
            *tuvo_comillas = 1;
            lectura++;
            continue;
        }
        else if (*lectura == ' ' ||
                 *lectura == '\t' ||
                 *lectura == '\n') {
            break;
        }

        *escritura++ = *lectura++;
    }

    if (*lectura != '\0') {
        lectura++;
    }

    *escritura = '\0';
    *cursor = lectura;

    return inicio;
}


static int parsear_comando(char *texto, Comando *comando)
{
    int argc = 0;

    comando->redirecciones.entrada = NULL;
    comando->redirecciones.salida = NULL;
    comando->redirecciones.append = 0;

    char *cursor = texto;
    int comillas = 0;
    char *token = siguiente_token(&cursor, &comillas);

    while (token != NULL && argc < MAX_ARGS - 1) {

        if (!comillas && strcmp(token, "<") == 0) {

            token = siguiente_token(&cursor, &comillas);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de <\n");
                return -1;
            }

            comando->redirecciones.entrada = token;
        }

        else if (!comillas && strcmp(token, ">") == 0) {

            token = siguiente_token(&cursor, &comillas);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de >\n");
                return -1;
            }

            comando->redirecciones.salida = token;
            comando->redirecciones.append = 0;
        }

        else if (!comillas && strcmp(token, ">>") == 0) {

            token = siguiente_token(&cursor, &comillas);

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

        token = siguiente_token(&cursor, &comillas);
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
        return -1;
    }

    char *cursor = linea;
    char *segmento = siguiente_segmento(&cursor);

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

        segmento = siguiente_segmento(&cursor);
    }

    return pipeline->cantidad;
}