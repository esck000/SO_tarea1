#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "pmon.h"
#include "jobs.h"


static volatile sig_atomic_t refrescar_pmon = 0;
static volatile sig_atomic_t salir_pmon = 0;

typedef struct {
    pid_t pid;
    unsigned long ticks_anteriores;
    struct timespec tiempo_anterior;
    int valido;
} CpuAnterior;

static void manejar_sigalrm_pmon(int signo)
{
    (void)signo;
    refrescar_pmon = 1;
}

static void manejar_sigint_pmon(int signo)
{
    (void)signo;
    salir_pmon = 1;
}


static int leer_stat_proceso(
    pid_t pid,
    char *estado,
    unsigned long *utime,
    unsigned long *stime
)
{
    char ruta[64];

    snprintf(
        ruta,
        sizeof(ruta),
        "/proc/%d/stat",
        pid
    );

    FILE *archivo = fopen(ruta, "r");

    if (archivo == NULL) {
        return -1;
    }

    char linea[4096];

    if (fgets(linea, sizeof(linea), archivo) == NULL) {
        fclose(archivo);
        return -1;
    }

    fclose(archivo);

    char *cierre_parentesis = strrchr(linea, ')');

    if (cierre_parentesis == NULL) {
        return -1;
    }

    char *datos = cierre_parentesis + 2;

    int ppid;
    int pgrp;
    int session;
    int tty_nr;
    int tpgid;

    unsigned int flags;

    unsigned long minflt;
    unsigned long cminflt;
    unsigned long majflt;
    unsigned long cmajflt;

    int leidos = sscanf(
        datos,
        "%c "
        "%d %d %d %d %d "
        "%u "
        "%lu %lu %lu %lu "
        "%lu %lu",
        estado,
        &ppid,
        &pgrp,
        &session,
        &tty_nr,
        &tpgid,
        &flags,
        &minflt,
        &cminflt,
        &majflt,
        &cmajflt,
        utime,
        stime
    );

    if (leidos != 13) {
        return -1;
    }

    return 0;
}

static int leer_rss_proceso(pid_t pid, unsigned long *rss_kb)
{
    char ruta[64];

    snprintf(
        ruta,
        sizeof(ruta),
        "/proc/%d/status",
        pid
    );

    FILE *archivo = fopen(ruta, "r");

    if (archivo == NULL) {
        return -1;
    }

    char linea[256];
    int encontrado = 0;

    while (fgets(linea, sizeof(linea), archivo) != NULL) {

        if (strncmp(linea, "VmRSS:", 6) == 0) {

            if (sscanf(linea + 6, "%lu", rss_kb) == 1) {
                encontrado = 1;
            }

            break;
        }
    }

    fclose(archivo);

    if (!encontrado) {
        return -1;
    }

    return 0;
}

int ejecutar_pmon(int segundos)
{

    struct sigaction sa_alrm;
    struct sigaction sa_int;
    struct sigaction anterior_alrm;
    struct sigaction anterior_int;

    sigset_t bloqueo_pmon;
    sigset_t mascara_original;  

    sigemptyset(&bloqueo_pmon);
    sigaddset(&bloqueo_pmon, SIGALRM);
    sigaddset(&bloqueo_pmon, SIGINT);

    if (sigprocmask(
            SIG_BLOCK,
            &bloqueo_pmon,
            &mascara_original
        ) < 0) {

        perror("sigprocmask");
        return -1;
    }

    refrescar_pmon = 0;
    salir_pmon = 0;

    sa_alrm.sa_handler = manejar_sigalrm_pmon;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0;

    if (sigaction(SIGALRM, &sa_alrm, &anterior_alrm) < 0) {
        perror("sigaction SIGALRM");

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);

        return -1;
    }

    sa_int.sa_handler = manejar_sigint_pmon;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, &anterior_int) < 0) {
        perror("sigaction SIGINT");

        sigaction(SIGALRM, &anterior_alrm, NULL);

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);

        return -1;
    }
    (void)segundos;

    int capacidad = MAX_JOBS * MAX_COMANDOS;

    ProcesoJobInfo *procesos = calloc(
        capacidad,
        sizeof(ProcesoJobInfo)
    );

    if (procesos == NULL) {
        perror("calloc");

        sigaction(SIGALRM, &anterior_alrm, NULL);
        sigaction(SIGINT, &anterior_int, NULL);

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);

        return -1;
    }

    CpuAnterior *cpu_anteriores = calloc(
    capacidad,
    sizeof(CpuAnterior)
    );

    if (cpu_anteriores == NULL) {
        perror("calloc");

        free(procesos);

        sigaction(SIGALRM, &anterior_alrm, NULL);
        sigaction(SIGINT, &anterior_int, NULL);

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);

        return -1;
    }

    long ticks_por_segundo = sysconf(_SC_CLK_TCK);

    if (ticks_por_segundo <= 0) {
        fprintf(stderr, "pmon: no se pudo obtener _SC_CLK_TCK\n");

        free(cpu_anteriores);
        free(procesos);

        sigaction(SIGALRM, &anterior_alrm, NULL);
        sigaction(SIGINT, &anterior_int, NULL);

        sigprocmask(SIG_SETMASK, &mascara_original, NULL);

        return -1;
    }

    refrescar_pmon = 1;

    while (!salir_pmon) {

        if (refrescar_pmon) {

            refrescar_pmon = 0;

            int cantidad = obtener_procesos_activos(
                procesos,
                capacidad
            );

            if (cantidad < 0) {
                break;
            }

            printf("\nPID\tCOMANDO\tESTADO\tCPU(%%)\tRSS(KB)\n");

            for (int i = 0; i < cantidad; i++) {

                char estado;
                unsigned long utime;
                unsigned long stime;
                unsigned long rss_kb;

                if (leer_stat_proceso(
                        procesos[i].pid,
                        &estado,
                        &utime,
                        &stime
                    ) < 0) {

                    continue;
                }

                double cpu = 0.0;
                unsigned long ticks_actuales = utime + stime;

                struct timespec tiempo_actual;

                if (clock_gettime(CLOCK_MONOTONIC, &tiempo_actual) < 0) {
                    perror("clock_gettime");
                    continue;
                }

                int indice_cpu = -1;

                for (int j = 0; j < capacidad; j++) {

                    if (cpu_anteriores[j].valido &&
                        cpu_anteriores[j].pid == procesos[i].pid) {

                        indice_cpu = j;
                        break;
                    }
                }

                if (indice_cpu >= 0) {
                    unsigned long diferencia_ticks =
                        ticks_actuales - cpu_anteriores[indice_cpu].ticks_anteriores;

                    double intervalo_real =
                        (double)(tiempo_actual.tv_sec -
                                cpu_anteriores[indice_cpu].tiempo_anterior.tv_sec)
                        +
                        (double)(tiempo_actual.tv_nsec -
                                cpu_anteriores[indice_cpu].tiempo_anterior.tv_nsec)
                        / 1000000000.0;

                    if (intervalo_real > 0.0) {

                        cpu =
                            ((double)diferencia_ticks /
                            (double)ticks_por_segundo /
                            intervalo_real) * 100.0;
                    }

                    cpu_anteriores[indice_cpu].ticks_anteriores =
                        ticks_actuales;

                    cpu_anteriores[indice_cpu].tiempo_anterior =
                        tiempo_actual;
                }
                else {

                    for (int j = 0; j < capacidad; j++) {

                        if (!cpu_anteriores[j].valido) {

                            cpu_anteriores[j].pid = procesos[i].pid;
                            cpu_anteriores[j].ticks_anteriores = ticks_actuales;
                            cpu_anteriores[j].tiempo_anterior = tiempo_actual;
                            cpu_anteriores[j].valido = 1;

                            break;
                        }
                    }
                }

                if (leer_rss_proceso(
                        procesos[i].pid,
                        &rss_kb
                    ) < 0) {

                    continue;
                }

                printf(
                    "%d\t%s\t%c\t%.1f\t%lu\n",
                    procesos[i].pid,
                    procesos[i].comando,
                    estado,
                    cpu,
                    rss_kb
                );
            }

            /*
            * Pedimos al kernel que envíe SIGALRM
            * dentro de "segundos" segundos.
            */
            alarm(segundos);
        }

        if (!salir_pmon && !refrescar_pmon) {
            sigsuspend(&mascara_original);
        }
    }
    /*
    * Cancelamos cualquier alarma futura.
    */
    alarm(0);

    /*
    * Ponemos SIGALRM temporalmente en SIG_IGN.
    * Esto también descarta un SIGALRM que pudiera
    * haber quedado pendiente.
    */
    struct sigaction ignorar_alrm;

    ignorar_alrm.sa_handler = SIG_IGN;
    sigemptyset(&ignorar_alrm.sa_mask);
    ignorar_alrm.sa_flags = 0;

    sigaction(SIGALRM, &ignorar_alrm, NULL);

    /*
    * Restauramos las configuraciones que tenía
    * la shell antes de entrar a pmon.
    */
    sigaction(SIGALRM, &anterior_alrm, NULL);
    sigaction(SIGINT, &anterior_int, NULL);

    /*
    * Ahora es seguro restaurar la máscara.
    */
    sigprocmask(SIG_SETMASK, &mascara_original, NULL);

    free(cpu_anteriores);
    free(procesos);

    return 0;
}