#ifndef BUILTINS_H
#define BUILTINS_H

// Valores que retorna ejecutar_builtin().
#define BUILTIN_NO 0    // El comando no es un built-in: la shell debe ejecutarlo con fork() + execvp().
#define BUILTIN_OK 1    // El built-in (cd, jobs o pmon) ya se ejecutó en la shell: se sigue con el siguiente prompt.
#define BUILTIN_EXIT 2  // El comando es "exit": la shell debe terminar con el código guardado en *codigo_salida.

// Si argv[0] es un built-in (cd, jobs, pmon o exit), lo ejecuta en el proceso de la shell, sin fork().

int ejecutar_builtin(int argc, char *argv[], int *codigo_salida);
// argc y argv son los argumentos del comando ya procesado, codigo_salida solo se escribe cuando el comando es "exit"

int es_builtin(const char *comando);

#endif