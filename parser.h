#ifndef PARSER_H
#define PARSER_H

#define MAX_LINEA 1024
#define MAX_ARGS 64
#define MAX_COMANDOS 64

typedef struct {
    char *entrada;
    char *salida;
    int append;
} Redirecciones;

typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    Redirecciones redirecciones;
} Comando;

typedef struct {
    Comando comandos[MAX_COMANDOS];
    int cantidad;
    int background;
} Pipeline;

int parsear_linea(char *linea, Pipeline *pipeline);

#endif