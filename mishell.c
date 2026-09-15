// Librerias necesarias para el funcionamiento del programa.
#define _POSIX_C_SOURCE 200809L // Definimos la macro _POSIX_C_SOURCE para habilitar funciones POSIX como getcwd() y sigaction().

#include <stdio.h> // Nos permite usar printf() fgets() perror().
#include <stdlib.h> // Funciones generales de C.
#include <unistd.h> // Nos permite usar fork() exec() chdir() getcwd().

#include "parser.h" // Incluimos el archivo de cabecera parser.h que contiene la declaración de la función parsear_linea().
#include "builtins.h" // Incluimos el archivo de cabecera builtins.h que contiene la declaración de la función ejecutar_builtin().
#include "executor.h" // Incluimos el archivo de cabecera executor.h que contiene la declaración de la función ejecutar_comando().
#include "jobs.h" // Incluimos el archivo de cabecera jobs.h que contiene la declaración de las funciones relacionadas con los trabajos en segundo plano.



void mostrar_prompt(void)
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) { //getcwd() obtiene el directorio de trabajo actual y lo almacena en cwd.
        printf("Shelltarea:%s$ ", cwd);
    } else {
        perror("getcwd");
        printf("Shelltarea$ ");
    }

    fflush(stdout);
}


int main(void)
{
    char linea[1024]; // Variable para almacenar la línea de comando ingresada por el usuario.
    char *argv[MAX_ARGS];  //Arreglo de punteros a char para almacenar los argumentos de la línea de comando.
    int background; // Variable para indicar si el comando se debe ejecutar en segundo plano.   
    Redirecciones redirecciones; // Variable para almacenar la información de redirección de entrada/salida.

    configurar_sigchld(); // Configuramos el manejador de la señal SIGCHLD para manejar la terminación de procesos hijos.

    while(1){              // Bucle para mostrar el prompt de mi shell.
        
        mostrar_jobs_terminados();
        mostrar_prompt();

        if(fgets(linea,sizeof(linea), stdin) == NULL){
            printf("\n");
            break; // Salir del bucle si se recibe EOF (Ctrl+D).
        }
        int argc = parsear_linea(linea, argv, &background, &redirecciones); // Llamada a la función para dividir la línea de comando en argumentos.

        if(argc < 0) { // Si argc es negativo, significa que hubo un error en el parseo de la línea de comando.
            continue; // Volvemos al prompt para que el usuario ingrese otro comando.
        }

        if(argc == 0) {
            continue; // Salir si no se ingresaron argumentos (vuelve al while).
        }

        int codigo_salida = 0;
        int resultado_builtin = ejecutar_builtin(argc, argv, &codigo_salida);

        if (resultado_builtin == BUILTIN_OK) { // Si el comando era un built-in y ya se ejecutó, volvemos al prompt.
            continue;
        }

        if (resultado_builtin == BUILTIN_EXIT) { // Si el comando es "exit", salimos del bucle y terminamos la shell.
            return codigo_salida;
        }

        if (ejecutar_comando(argv, argc, background, &redirecciones) < 0) {
            return EXIT_FAILURE;
        }
        
    }

    
    return 0;
}