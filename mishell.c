// Librerias necesarias para el funcionamiento del programa.
#define _POSIX_C_SOURCE 200809L // Definimos la macro _POSIX_C_SOURCE para habilitar funciones POSIX como getcwd() y sigaction().

#include <stdio.h> // Nos permite usar printf() fgets() perror().
#include <stdlib.h> // Funciones generales de C.
#include <unistd.h> // Nos permite usar fork() exec() chdir() getcwd().

#include "parser.h" // Incluimos el archivo de cabecera parser.h que contiene la declaración de la función parsear_linea().
#include "builtins.h" // Incluimos el archivo de cabecera builtins.h que contiene la declaración de la función ejecutar_builtin().
#include "executor.h" // Incluimos el archivo de cabecera executor.h que contiene la declaración de la función ejecutar_comando().
#include "jobs.h" // Incluimos el archivo de cabecera jobs.h que contiene la declaración de las funciones relacionadas con los trabajos en segundo plano.

#include <signal.h>


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

static void configurar_senales_shell(void)
{
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction SIGINT");
    }

    if (sigaction(SIGQUIT, &sa, NULL) < 0) {
        perror("sigaction SIGQUIT");
    }
}

int main(void)
{
    char linea[MAX_LINEA];
    Pipeline pipeline;

    configurar_senales_shell();
    configurar_sigchld(); // Configuramos el manejador de la señal SIGCHLD para manejar la terminación de procesos hijos.

    while(1){              // Bucle para mostrar el prompt de mi shell.
        
        mostrar_jobs_terminados();
        mostrar_prompt();

        if(fgets(linea,sizeof(linea), stdin) == NULL){
            printf("\n");
            break; // Salir del bucle si se recibe EOF (Ctrl+D).
        }
        int cantidad = parsear_linea(linea, &pipeline);

        if (cantidad < 0) {
            continue;
        }

        if (cantidad == 0) {
            continue;
        }

        if (pipeline.cantidad == 1) {

            Comando *comando = &pipeline.comandos[0];

            int codigo_salida = 0;

            int resultado_builtin = ejecutar_builtin(
                comando->argc,
                comando->argv,
                &codigo_salida
            );

            if (resultado_builtin == BUILTIN_OK) {
                continue;
            }

            if (resultado_builtin == BUILTIN_EXIT) {
                return codigo_salida;
            }
        }

        if (ejecutar_pipeline(&pipeline) < 0) {
            return EXIT_FAILURE;
        }
    }

    
    return 0;
}