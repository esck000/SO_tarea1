// Librerias necesarias para el funcionamiento del programa.
#define _POSIX_C_SOURCE 200809L // Definimos la macro _POSIX_C_SOURCE para habilitar funciones POSIX como getcwd() y sigaction().

#include <stdio.h> // Nos permite usar printf() fgets() perror().
#include <stdlib.h> // Funciones generales de C.
#include <unistd.h> // Nos permite usar fork() exec() chdir() getcwd().

#include "parser.h" // Incluimos el archivo de cabecera parser.h que contiene la declaración de la función parsear_linea().
#include "builtins.h" // Incluimos el archivo de cabecera builtins.h que contiene la declaración de la función ejecutar_builtin().
#include "executor.h" // Incluimos el archivo de cabecera executor.h que contiene la declaración de la función ejecutar_comando().
#include "jobs.h" // Incluimos el archivo de cabecera jobs.h que contiene la declaración de las funciones relacionadas con los trabajos en segundo plano.

#include <signal.h>     // sigaction(), SIG_IGN


// Muestra el prompt con el directorio actual: Shelltarea:/ruta$
void mostrar_prompt(void)
{
    char cwd[1024];     // Aquí se guarda el directorio actual.

    if (getcwd(cwd, sizeof(cwd)) != NULL) { // obtiene el directorio de trabajo actual y lo almacena en cwd.
        printf("Shelltarea:%s$ ", cwd);     // Prompt con la ruta.
    } else {
        perror("getcwd");                   // No se pudo obtener la ruta.
        printf("Shelltarea$ ");             // Prompt sin ruta.
    }

    fflush(stdout);     // Forzamos que se vea el prompt (no termina en salto de línea).
}

// Hace que la shell ignore Ctrl+C (SIGINT) y Ctrl+\ (SIGQUIT), para que no se cierre.
static void configurar_senales_shell(void)
{
    struct sigaction sa;        // Configuración de la señal.

    sa.sa_handler = SIG_IGN;    // Ignorar la señal.
    sigemptyset(&sa.sa_mask);   // No bloqueamos otras señales.
    sa.sa_flags = SA_RESTART;   // Reinicia las llamadas al sistema interrumpidas (como fgets).

    if (sigaction(SIGINT, &sa, NULL) < 0) {     // Ctrl+C.
        perror("sigaction SIGINT");
    }

    if (sigaction(SIGQUIT, &sa, NULL) < 0) {    // Ctrl+\.
        perror("sigaction SIGQUIT");
    }
}

// Ciclo principal de la shell: leer, parsear y ejecutar, hasta exit o Ctrl+D.
int main(void)
{
    char linea[MAX_LINEA];      // Línea que escribe el usuario.
    Pipeline pipeline;          // La línea ya parseada.

    configurar_senales_shell(); // La shell ignora Ctrl+C y Ctrl+\.
    configurar_sigchld(); // Configuramos el manejador de la señal SIGCHLD para manejar la terminación de procesos hijos.

    while(1){              // Bucle para mostrar el prompt de mi shell.

        mostrar_jobs_terminados();  // Avisamos de los jobs que terminaron: [n]+ Done.
        mostrar_prompt();           // Mostramos el prompt.

        if(fgets(linea,sizeof(linea), stdin) == NULL){  // Leemos una línea completa.
            printf("\n");
            break; // Salir del bucle si se recibe EOF (Ctrl+D).
        }
        int cantidad = parsear_linea(linea, &pipeline);     // Separamos la línea en comandos.

        if (cantidad < 0) {     //error
            continue;           // Pedimos otra línea.
        }

        if (cantidad == 0) {    // Línea vacía.
            continue;           // Pedimos otra línea.
        }

        if (pipeline.cantidad == 1) {   // Los built-ins solo se ejecutan si el comando va solo.

            Comando *comando = &pipeline.comandos[0];   // El único comando.

            int codigo_salida = 0;      // Lo llena ejecutar_builtin() si el comando es exit.

            // Probamos si es un built-in: cd, jobs, pmon o exit.
            int resultado_builtin = ejecutar_builtin(
                comando->argc,      // Cantidad de argumentos.
                comando->argv,      // Argumentos.
                &codigo_salida      // Recibe el código de salida de exit.
            );

            if (resultado_builtin == BUILTIN_OK) {  // Ya se ejecutó en la shell.
                continue;                           // Pedimos otra línea.
            }

            if (resultado_builtin == BUILTIN_EXIT) {    // El usuario escribió exit.
                return codigo_salida;                   // Terminamos la shell con ese código.
            }
        }

        // No era un built-in: lo ejecutamos con fork() + execvp().
        if (ejecutar_pipeline(&pipeline) < 0) {
            return EXIT_FAILURE;    // Error fatal (por ejemplo, falló fork()).
        }
    }


    return 0;   // Salimos por Ctrl+D.
}
