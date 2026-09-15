#ifndef PARSER_H
#define PARSER_H

#define MAX_LINEA 1024
#define MAX_ARGS 64

typedef struct {
    char *entrada;
    char *salida;
    int append;
} Redirecciones;

int parsear_linea(char *linea, char *argv[], int *background, Redirecciones *redirecciones);

#endif