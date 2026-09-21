#ifndef PARSER_H
#define PARSER_H

#define MAX_LINEA 1024      // Largo máximo de una línea.
#define MAX_ARGS 64         // Máximo de argumentos por comando (incluye el NULL final).
#define MAX_COMANDOS 64     // Máximo de comandos en un pipeline.

// Redirecciones de un comando.
typedef struct {
    char *entrada;      // Archivo de <, o NULL si no hay.
    char *salida;       // Archivo de > o >>, o NULL si no hay.
    int append;         // 1 si es >> (agrega al final), 0 si es > trunca.
} Redirecciones;

// Un comando ya separado en argumentos.
typedef struct {
    char *argv[MAX_ARGS];           // Argumentos, terminados en NULL. argv[0] es el comando.
    int argc;                       // Cantidad de argumentos.
    Redirecciones redirecciones;    // Redirecciones del comando.
} Comando;

// Una línea ya parseada: uno o más comandos unidos por pipes.
typedef struct {
    Comando comandos[MAX_COMANDOS];     // Los comandos, en orden.
    int cantidad;                       // Cuántos comandos hay.
    int background;                     // 1 si la línea termina en &.
} Pipeline;

// Convierte la línea en un Pipeline (modifica el texto de linea).
// Retorna la cantidad de comandos, 0 si la línea está vacía y -1 si hay un error.
int parsear_linea(char *linea, Pipeline *pipeline);

#endif