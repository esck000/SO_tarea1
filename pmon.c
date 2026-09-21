#define _POSIX_C_SOURCE 200809L     // Habilita sigaction(), alarm() y clock_gettime().

#include <stdio.h>        
#include <stdlib.h>       
#include <string.h>       
#include <signal.h>       
#include <unistd.h>       
#include <time.h>         

#include "pmon.h"         // ejecutar_pmon()
#include "jobs.h"         // obtener_procesos_activos()


static volatile sig_atomic_t refrescar_pmon = 0;    // SIGALRM la pone en 1: hay que redibujar la tabla.
static volatile sig_atomic_t salir_pmon = 0;        // SIGINT la pone en 1: hay que salir de pmon.

// Última lectura de CPU de un proceso, para calcular el % con la diferencia.
typedef struct {
    pid_t pid;                          // Proceso al que pertenece la lectura.
    unsigned long ticks_anteriores;  // utime + stime de la lectura anterior.
    struct timespec tiempo_anterior;   // Momento de la lectura anterior.
    int valido;                         // 1 si esta entrada está en uso.
} CpuAnterior;

// Manejador de SIGALRM: solo levanta la bandera para refrescar.
static void manejar_sigalrm_pmon(int signo)
{
    (void)signo;       // No usamos el número de señal.
    refrescar_pmon = 1;     // Levantamos la bandera.
}

// Manejador de SIGINT (Ctrl+C): solo levanta la bandera para salir.
static void manejar_sigint_pmon(int signo)
{
    (void)signo; // No usamos el número de señal.
    salir_pmon = 1;         // Levantamos la bandera.
}


// Lee de /proc/pid/stat el estado (R, S,etc) y los ticks de CPU: utime (usuario) y stime (kernel).
// Retorna 0 si pudo leerlos y -1 si el proceso ya no existe o el formato no coincide.
static int leer_stat_proceso(
    pid_t pid,                  // Proceso a consultar.
    char *estado,         // Salida es letra del estado.
    unsigned long *utime,  // la salida es ticks en modo usuario.
    unsigned long *stime        // Salida es ticks en modo kernel.
)
{
    char ruta[64];  // Ruta del archivo 

    snprintf(                   // Armamos la ruta con el PID.
        ruta,               //Destino.
        sizeof(ruta),           //Tamaño máximo, para no desbordar.
        "/proc/%d/stat",      //Archivo con el estado y los tiempos de CPU.
        pid             // Reemplaza al %d.
    );

    FILE *archivo = fopen(ruta, "r");   //Abrimos el archivo en modo lectura.

    if (archivo == NULL) {   // El proceso ya no existe.
        return -1;     //Avisamos del error.
    }

    char linea[4096];   //La línea de stat es larga.

    if (fgets(linea, sizeof(linea), archivo) == NULL) {     // Leemos la única línea del archivo.
        fclose(archivo);    //Cerramos antes de salir.
        return -1;         // No se pudo leer.
    }

    fclose(archivo);   //Ya tenemos la línea, cerramos el archivo.

    char *cierre_parentesis = strrchr(linea, ')');  // Leemos después del último ')' .

    if (cierre_parentesis == NULL) {   // Formato inesperado.
        return -1;        // Avisamos del error.
    }

    char *datos = cierre_parentesis + 2;    // Saltamos ") " y quedamos en el campo del estado.

    // Campos de stat que se leen pero no se usan (sscanf necesita dónde guardarlos).
    int ppid;               // PID del padre.
    int pgrp;               // Grupo de procesos.
    int session;            // Sesión.
    int tty_nr;             // Terminal.
    int tpgid;              // Grupo en primer plano de la terminal.

    unsigned int flags;     // Banderas del kernel.

    unsigned long minflt;   // Fallos de página menores.
    unsigned long cminflt;  // Fallos menores de los hijos.
    unsigned long majflt;   // Fallos de página mayores.
    unsigned long cmajflt;  // Fallos mayores de los hijos.

    // Campos 3 a 15 de stat; solo usamos el estado, utime y stime.
    int leidos = sscanf(
        datos,              // Texto desde el campo del estado.
        "%c "                       // Estado.
        "%d %d %d %d %d "    // ppid, pgrp, session, tty_nr, tpgid.
        "%u "                       // flags.
        "%lu %lu %lu %lu "    // Fallos de página.
        "%lu %lu",                  // utime y stime, los que nos interesan.
        estado,                     // Guarda el estado.
        &ppid,           // Guarda el PID del padre.
        &pgrp,               // Guarda el grupo.
        &session,             // Guarda la sesión.
        &tty_nr,             // Guarda la terminal.
        &tpgid,               // Guarda el grupo en primer plano.
        &flags,           // Guarda las banderas.
        &minflt,                    // Guarda los fallos menores.
        &cminflt,        // Guarda los fallos menores de los hijos.
        &majflt,                    // Guarda los fallos mayores.
        &cmajflt,      // Guarda los fallos mayores de los hijos.
        utime,          // Guarda los ticks de usuario.
        stime             // Guarda los ticks de kernel.
    );

    if (leidos != 13) {     // Faltaron campos.
        return -1;          // Avisamos del error.
    }

    return 0;       // Lectura correcta.
}

// Lee VmRSS (memoria residente, en KB) de /proc/pid/status.
// Retorna 0 si la encontró y -1 si el proceso ya no existe o no tiene VmRSS.
static int leer_rss_proceso(pid_t pid, unsigned long *rss_kb)   // rss_kb es la salida.
{
    char ruta[64];      // Ruta del archivo en /proc.

    snprintf(                   // Armamos la ruta con el PID.
        ruta,                   // Destino.
        sizeof(ruta),           // Tamaño máximo, para no desbordar.
        "/proc/%d/status",      // Archivo con la memoria (VmRSS).
        pid                     // Reemplaza al %d.
    );

    FILE *archivo = fopen(ruta, "r");   // Abrimos el archivo en modo lectura.

    if (archivo == NULL) {      // El proceso ya no existe.
        return -1;              // Avisamos del error.
    }

    char linea[256];        // Una línea de status.
    int encontrado = 0;     // 1 si ya leímos VmRSS.

    while (fgets(linea, sizeof(linea), archivo) != NULL) {      // Recorremos el archivo línea por línea.

        if (strncmp(linea, "VmRSS:", 6) == 0) {     // Es la línea que buscamos.

            if (sscanf(linea + 6, "%lu", rss_kb) == 1) {    // Leemos el número que va después de "VmRSS:".
                encontrado = 1;                             // Lo conseguimos.
            }

            break;      // No hace falta seguir leyendo.
        }
    }

    fclose(archivo);        // Cerramos el archivo.

    if (!encontrado) {      // Por ejemplo, los procesos zombie no tienen VmRSS.
        return -1;          // Avisamos del error.
    }

    return 0;       // Lectura correcta.
}

// Muestra la tabla de procesos en background y la refresca cada "segundos" segundos, hasta Ctrl+C.
// Retorna 0 si terminó bien y -1 si falló algo al iniciar.
int ejecutar_pmon(int segundos)
{

    struct sigaction sa_alrm;      // Manejador nuevo de SIGALRM.
    struct sigaction sa_int;         // Manejador nuevo de SIGINT.
    struct sigaction anterior_alrm;     // Manejador de SIGALRM que tenía la shell.
    struct sigaction anterior_int;      // Manejador de SIGINT que tenía la shell.

    sigset_t bloqueo_pmon;          // Señales que vamos a bloquear.
    sigset_t mascara_original;      // Máscara que tenía la shell, para restaurarla al salir.

    // Bloqueamos SIGALRM y SIGINT; se entregan solo en sigsuspend().
    sigemptyset(&bloqueo_pmon);                 // Partimos de un conjunto vacío.
    sigaddset(&bloqueo_pmon, SIGALRM);          // Agregamos SIGALRM.
    sigaddset(&bloqueo_pmon, SIGINT);           // Agregamos SIGINT.

    // Aplicamos el bloqueo y guardamos la máscara anterior.
    if (sigprocmask(
            SIG_BLOCK,              // Agrega estas señales a las ya bloqueadas.
            &bloqueo_pmon,          // Señales a bloquear.
            &mascara_original       // Guarda aquí la máscara anterior.
        ) < 0) {

        perror("sigprocmask");      // Mostramos el error.
        return -1;                  // No pudimos iniciar pmon.
    }

    refrescar_pmon = 0;     // Empezamos sin banderas levantadas.
    salir_pmon = 0;         // Idem.

    sa_alrm.sa_handler = manejar_sigalrm_pmon;      // SIGALRM levanta refrescar_pmon.
    sigemptyset(&sa_alrm.sa_mask);                  // No bloqueamos otras señales en el manejador.
    sa_alrm.sa_flags = 0;                           // Sin opciones especiales.

    // Instalamos el manejador de SIGALRM y guardamos el anterior.
    if (sigaction(SIGALRM, &sa_alrm, &anterior_alrm) < 0) {
        perror("sigaction SIGALRM");                            // Mostramos el error.

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);      // Deshacemos el bloqueo antes de salir.

        return -1;                                              // No pudimos iniciar pmon.
    }

    sa_int.sa_handler = manejar_sigint_pmon;        // Ctrl+C levanta salir_pmon.
    sigemptyset(&sa_int.sa_mask);                   // No bloqueamos otras señales en el manejador.
    sa_int.sa_flags = 0;                            // Sin opciones especiales.

    // Instalamos el manejador de SIGINT y guardamos el anterior.
    if (sigaction(SIGINT, &sa_int, &anterior_int) < 0) {
        perror("sigaction SIGINT");                             // Mostramos el error.

        sigaction(SIGALRM, &anterior_alrm, NULL);               // Restauramos SIGALRM.

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);      // Deshacemos el bloqueo antes de salir.

        return -1;                                              // No pudimos iniciar pmon.
    }
    (void)segundos;     // No hace nada: segundos se usa más abajo, en alarm().

    int capacidad = MAX_JOBS * MAX_COMANDOS;    // Máximo de procesos que puede haber.

    // Procesos vivos, se leen de nuevo en cada refresco.
    ProcesoJobInfo *procesos = calloc(
        capacidad,                  // Cantidad de elementos.
        sizeof(ProcesoJobInfo)      // Tamaño de cada uno.
    );

    if (procesos == NULL) {     // Sin memoria.
        perror("calloc");       // Mostramos el error.

        sigaction(SIGALRM, &anterior_alrm, NULL);       // Restauramos las señales de la shell.
        sigaction(SIGINT, &anterior_int, NULL);         // Idem.

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);      // Deshacemos el bloqueo antes de salir.

        return -1;                                              // No pudimos iniciar pmon.
    }

    // Lecturas anteriores de CPU, una por proceso.
    CpuAnterior *cpu_anteriores = calloc(
    capacidad,                  // Cantidad de elementos.
    sizeof(CpuAnterior)         // Tamaño de cada uno.
    );

    if (cpu_anteriores == NULL) {       // Sin memoria.
        perror("calloc");               // Mostramos el error.

        free(procesos);         // Liberamos lo que ya habíamos pedido.

        sigaction(SIGALRM, &anterior_alrm, NULL);       // Restauramos las señales de la shell.
        sigaction(SIGINT, &anterior_int, NULL);         // Idem.

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);      // Deshacemos el bloqueo antes de salir.

        return -1;                                              // No pudimos iniciar pmon.
    }

    long ticks_por_segundo = sysconf(_SC_CLK_TCK);      // Ticks de CPU por segundo.

    if (ticks_por_segundo <= 0) {       // No se pudo obtener el valor.
        fprintf(stderr, "pmon: no se pudo obtener _SC_CLK_TCK\n");      // Avisamos al usuario.

        free(cpu_anteriores);       // Liberamos la memoria.
        free(procesos);             // Idem.

        sigaction(SIGALRM, &anterior_alrm, NULL);       // Restauramos las señales de la shell.
        sigaction(SIGINT, &anterior_int, NULL);         // Idem.

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);      // Deshacemos el bloqueo antes de salir.

        return -1;                                              // No pudimos iniciar pmon.
    }

    refrescar_pmon = 1;     // La primera tabla se dibuja de inmediato.

    while (!salir_pmon) {       // Se repite hasta Ctrl+C.

        if (refrescar_pmon) {       // Toca redibujar la tabla.

            refrescar_pmon = 0;     // Bajamos la bandera.

            // Procesos de los jobs que siguen vivos.
            int cantidad = obtener_procesos_activos(
                procesos,           // Dónde guardarlos.
                capacidad           // Cuántos caben.
            );

            if (cantidad < 0) {     // Falló la lectura de la tabla de jobs.
                break;              // Salimos del ciclo.
            }

            printf("\nPID\tCOMANDO\tESTADO\tCPU(%%)\tRSS(KB)\n");     // Encabezado de la tabla.

            for (int i = 0; i < cantidad; i++) {    // Una fila por proceso.

                char estado;                // R, S, Z, T...
                unsigned long utime;        // Ticks de CPU en modo usuario.
                unsigned long stime;        // Ticks de CPU en modo kernel.
                unsigned long rss_kb;       // Memoria residente, en KB.

                // Si el proceso ya terminó, saltamos su fila.
                if (leer_stat_proceso(
                        procesos[i].pid,    // Proceso a consultar.
                        &estado,      // Recibe el estado.
                        &utime,       // Recibe los ticks de usuario.
                        &stime         // Recibe los ticks de kernel.
                    ) < 0) {

                    continue;               // Pasamos al siguiente proceso.
                }

                double cpu = 0.0;       // Queda en 0.0 si es la primera lectura de este proceso.
                unsigned long ticks_actuales = utime + stime;   // CPU total usada hasta ahora.

                struct timespec tiempo_actual;      // Momento de esta lectura.

                if (clock_gettime(CLOCK_MONOTONIC, &tiempo_actual) < 0) {   // Reloj que no retrocede.
                    perror("clock_gettime");    // Mostramos el error.
                    continue;                   // Pasamos al siguiente proceso.
                }

                int indice_cpu = -1;    // Lugar del proceso en cpu_anteriores (-1 si no está).

                // Buscamos si ya teníamos una lectura anterior de este proceso.
                for (int j = 0; j < capacidad; j++) {

                    if (cpu_anteriores[j].valido &&                 // La entrada está en uso
                        cpu_anteriores[j].pid == procesos[i].pid) { //mismo proceso.

                        indice_cpu = j;     // Ya lo habíamos leído antes.
                        break;              // No seguimos buscando.
                    }
                }

                if (indice_cpu >= 0) {      // Hay lectura anterior: calculamos el % de CPU.
                    // Ticks usados desde la lectura anterior.
                    unsigned long diferencia_ticks =
                        ticks_actuales - cpu_anteriores[indice_cpu].ticks_anteriores;

                    // Segundos reales transcurridos desde la lectura anterior.
                    double intervalo_real =
                        (double)(tiempo_actual.tv_sec -                             // Diferencia de segundos
                                cpu_anteriores[indice_cpu].tiempo_anterior.tv_sec)
                        +
                        (double)(tiempo_actual.tv_nsec -                            
                                cpu_anteriores[indice_cpu].tiempo_anterior.tv_nsec)
                        / 1000000000.0;                                             // Pasamos los nanosegundos a segundos.

                    if (intervalo_real > 0.0) {     // Evita dividir por cero.

                        // %CPU = (ticks usados / ticks por segundo) / segundos reales * 100.
                        cpu =
                            ((double)diferencia_ticks /     
                            (double)ticks_por_segundo /     
                            intervalo_real) * 100.0;        
                    }

                    // Guardamos esta lectura para la próxima vuelta.
                    cpu_anteriores[indice_cpu].ticks_anteriores =
                        ticks_actuales;                     // Ticks de ahora.

                    cpu_anteriores[indice_cpu].tiempo_anterior =
                        tiempo_actual;                      // Momento de ahora.
                }
                else {      // Primera lectura del proceso: solo la guardamos.

                    // Buscamos un lugar libre.
                    for (int j = 0; j < capacidad; j++) {

                        if (!cpu_anteriores[j].valido) {    // Primer lugar libre.

                            // Guardamos el PID, los ticks y el momento de la lectura.
                            cpu_anteriores[j].pid = procesos[i].pid;            // Proceso.
                            cpu_anteriores[j].ticks_anteriores = ticks_actuales; // Ticks de ahora.
                            cpu_anteriores[j].tiempo_anterior = tiempo_actual;   // Momento de ahora.
                            cpu_anteriores[j].valido = 1;                        // Entrada en uso.

                            break;      // Listo, no seguimos buscando.
                        }
                    }
                }

                // Si el proceso ya terminó, no mostramos su fila.
                if (leer_rss_proceso(
                        procesos[i].pid,    // Proceso a consultar.
                        &rss_kb             // Recibe la memoria en KB.
                    ) < 0) {

                    continue;               // Pasamos al siguiente proceso.
                }

                // Fila: PID, comando, estado, %CPU y RSS.
                printf(
                    "%d\t%s\t%c\t%.1f\t%lu\n",  // Columnas separadas por tabulaciones.
                    procesos[i].pid,            // PID.
                    procesos[i].comando,        // Comando.
                    estado,                     // Estado.
                    cpu,                        // %CPU, con un decimal.
                    rss_kb                      // Memoria en KB.
                );
            }

            
            //Pedimos al kernel que envíe SIGALRM
            //dentro de "segundos" segundos.
            
            alarm(segundos);        // Programa el próximo refresco.
        }

        if (!salir_pmon && !refrescar_pmon) {       // Sin nada pendiente, esperamos.
            sigsuspend(&mascara_original);          // Duerme hasta que llegue una señal.
        }
    }
    
    //Cancelamos cualquier alarma futura.
    
    alarm(0);       // Un 0 cancela la alarma pendiente.

    
    //Ponemos SIGALRM temporalmente en SIG_IGN.
    //Esto también descarta un SIGALRM que pudiera
    //haber quedado pendiente.
    
    struct sigaction ignorar_alrm;          // Configuración temporal.

    ignorar_alrm.sa_handler = SIG_IGN;      // Ignorar SIGALRM.
    sigemptyset(&ignorar_alrm.sa_mask);     // Sin señales bloqueadas.
    ignorar_alrm.sa_flags = 0;              // Sin opciones especiales.

    sigaction(SIGALRM, &ignorar_alrm, NULL);    // La aplicamos.

    
    // Restauramos las configuraciones que tenía
    //la shell antes de entrar a pmon.
    
    sigaction(SIGALRM, &anterior_alrm, NULL);   // SIGALRM como estaba.
    sigaction(SIGINT, &anterior_int, NULL);     // SIGINT como estaba (la shell lo ignora).

    
    // Ahora es seguro restaurar la máscara.
    
    sigprocmask(SIG_SETMASK, &mascara_original, NULL);      // Desbloqueamos las señales.

    free(cpu_anteriores);       // Liberamos la memoria.
    free(procesos);             // Idem.

    return 0;       // pmon terminó bien.
}
