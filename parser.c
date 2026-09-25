#include <string.h>
#include "parser.h"
#include <stdio.h>

//Las comillas simples o dobles agrupan texto en un solo token y hacen
//que |, <, > queden como simples caracteres dentro del string
// Osea todo lo que este dentro de las comillas, no importa el caracter, se imprimrá como string
// Revisa la línea antes de partirla: sin comandos vacíos entre pipes y sin comillas sin cerrar.
// Retorna 0 si es válida y -1 si no.
static int validar_pipeline(const char *linea)
{
    int hay_contenido = 0;      // 1 si el comando actual tiene algo escrito.
    char comilla = '\0';        // Comilla abierta '  " o '\0' si no hay ninguna.

    for (int i = 0; linea[i] != '\0'; i++) {

        if (comilla != '\0') {

            if (linea[i] == comilla) {      // Cierra la comilla abierta.
                comilla = '\0';
            }
        }
        else if (linea[i] == '"' || linea[i] == '\'') {     // Abre una comilla.

            comilla = linea[i];
            hay_contenido = 1;
        }
        else if (linea[i] == '|') {     // Un | necesita un comando antes.

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
        fprintf(stderr, "Error: comilla sin cerrar\n");     // La comilla nunca se cerró.
        return -1;
    }

    if (!hay_contenido) {
        fprintf(stderr, "Error: comando vacio en pipeline\n");     // La línea termina en un | sin comando.
        return -1;
    }

    return 0;
}

//busca el | que no este entre comillas, y devueve las llamadas que esten entre estos
static char *siguiente_segmento(char **cursor)
{
    char *inicio = *cursor;     // Empieza donde terminó el tramo anterior.

    if (inicio == NULL) {       // Ya no quedan tramos.
        return NULL;
    }

    char comilla = '\0';

    for (char *p = inicio; *p != '\0'; p++) {

        if (comilla != '\0') {

            if (*p == comilla) {
                comilla = '\0';
            }
        }
        else if (*p == '"' || *p == '\'') {     // Abre una comilla.
            comilla = *p;
        }
        else if (*p == '|') {       // Un | fuera de comillas separa dos comandos.

            *p = '\0';              // Cortamos el texto en el |.
            *cursor = p + 1;        // La próxima búsqueda sigue después del |.

            return inicio;
        }
    }

    *cursor = NULL;     // Era el último tramo.

    return inicio;
}

// Devuelve el siguiente token separado por espacios, tabs o saltos de
// linea, quitando las comillas en el mismo buffer. *tuvo_comillas indica
// si el token traia comillas: en ese caso un ">" es texto y no operador.
 
static char *siguiente_token(char **cursor, int *tuvo_comillas)
{
    char *lectura = *cursor;

    *tuvo_comillas = 0;     // Por ahora, el token no tiene comillas.

    while (*lectura == ' ' || *lectura == '\t' || *lectura == '\n') {   // Saltamos los espacios iniciales.
        lectura++;
    }

    if (*lectura == '\0') {     // No hay más tokens.
        *cursor = lectura;
        return NULL;
    }

    char *inicio = lectura;
    char *escritura = lectura;      // Donde copiamos el token, sin las comillas.
    char comilla = '\0';

    while (*lectura != '\0') {

        if (comilla != '\0') {

            if (*lectura == comilla) {      // Cierra la comilla.
                comilla = '\0';
                lectura++;
                continue;
            }
        }
        else if (*lectura == '"' || *lectura == '\'') {     // Abre una comilla.

            comilla = *lectura;
            *tuvo_comillas = 1;
            lectura++;
            continue;
        }
        else if (*lectura == ' ' ||
                 *lectura == '\t' ||
                 *lectura == '\n') {
            break;      // Un espacio fuera de comillas termina el token.
        }

        *escritura++ = *lectura++;      // Copiamos el carácter.
    }

    if (*lectura != '\0') {     // Saltamos el separador.
        lectura++;
    }

    *escritura = '\0';      // Terminamos el token.
    *cursor = lectura;

    return inicio;
}


// Separa un comando en argumentos y redirecciones (<, > y >>).
// Retorna la cantidad de argumentos, o -1 si hay un error.
static int parsear_comando(char *texto, Comando *comando)
{
    int argc = 0;

    comando->redirecciones.entrada = NULL;
    comando->redirecciones.salida = NULL;
    comando->redirecciones.append = 0;

    char *cursor = texto;       // Posición actual dentro del texto.
    int comillas = 0;           // 1 si el token traía comillas: entonces es texto, no operador.
    char *token = siguiente_token(&cursor, &comillas);

    while (token != NULL && argc < MAX_ARGS - 1) {      // Dejamos un lugar para el NULL final.

        if (!comillas && strcmp(token, "<") == 0) {     // < archivo: entrada.

            token = siguiente_token(&cursor, &comillas);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de <\n");
                return -1;
            }

            comando->redirecciones.entrada = token;
        }

        else if (!comillas && strcmp(token, ">") == 0) {    // > archivo: salida, la trunca.

            token = siguiente_token(&cursor, &comillas);

            if (token == NULL) {
                fprintf(stderr,
                        "Error: falta archivo despues de >\n");
                return -1;
            }

            comando->redirecciones.salida = token;
            comando->redirecciones.append = 0;
        }

        else if (!comillas && strcmp(token, ">>") == 0) {   // >> archivo: salida, agrega al final.

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
            comando->argv[argc] = token;    // Es un argumento normal.
            argc++;
        }

        token = siguiente_token(&cursor, &comillas);
    }

    comando->argv[argc] = NULL;     // execvp() necesita el NULL final.
    comando->argc = argc;

    return argc;
}

// Convierte la línea en un Pipeline, detecta el & final, separa por | y parsea cada comando.
// Retorna la cantidad de comandos, 0 si la línea está vacía y -1 si hay un error.
int parsear_linea(char *linea, Pipeline *pipeline)
{
    pipeline->cantidad = 0;
    pipeline->background = 0;

    size_t largo = strlen(linea);   // Primero quitamos los espacios y el salto de línea del final.

    while (largo > 0 &&
           (linea[largo - 1] == ' ' ||
            linea[largo - 1] == '\t' ||
            linea[largo - 1] == '\n')) {

        linea[largo - 1] = '\0';
        largo--;
    }
    
    if (largo == 0) {       // Línea vacía.
        return 0;
    }

    if (largo > 0 && linea[largo - 1] == '&') {     // Un & al final pide ejecutar en background.
        pipeline->background = 1;
        linea[largo - 1] = '\0';
    }

    if (validar_pipeline(linea) < 0) {      // Comandos vacíos o comillas sin cerrar.
        return -1;
    }

    char *cursor = linea;
    char *segmento = siguiente_segmento(&cursor);   // Un tramo por cada comando del pipeline.

    while (segmento != NULL) {

        if (pipeline->cantidad >= MAX_COMANDOS) {   // Demasiados comandos.
            fprintf(stderr,
                    "Error: demasiados comandos en el pipeline\n");
            return -1;
        }

        Comando *comando =
            &pipeline->comandos[pipeline->cantidad];

        if (parsear_comando(segmento, comando) < 0) {
            return -1;
        }

        if (comando->argc == 0) {       // Tramo sin comando (por ejemplo, solo una redirección).
            fprintf(stderr,
                    "Error: comando vacio en pipeline\n");
            return -1;
        }

        pipeline->cantidad++;

        segmento = siguiente_segmento(&cursor);
    }

    return pipeline->cantidad;
}