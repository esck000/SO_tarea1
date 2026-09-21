# SO_tarea1 — mishell

Shell de texto simplificada escrita en C (POSIX, Linux) para la Tarea 1 de Sistemas Operativos 2026.
Soporta pipes de largo arbitrario, redirecciones, ejecución en background, manejo de señales y un
comando propio de monitoreo de procesos (`pmon`).

## Requisitos

- Linux, o Windows con **WSL** (Windows Subsystem for Linux).
- `gcc` y `make`.

La shell usa llamadas de sistema de Linux (`fork`, `execvp`, `sigaction`, `/proc`), por lo que **no**
compila ni funciona en Windows nativo: hay que usar WSL o una máquina Linux.

## Compilación y ejecución en WSL

1. Abrir la terminal de WSL (por ejemplo, escribir `wsl` en PowerShell).

2. Instalar las herramientas de compilación (solo la primera vez):

   ```bash
   sudo apt update
   sudo apt install build-essential
   ```

3. Entrar a la carpeta del proyecto. Las carpetas de Windows se acceden desde WSL a través de `/mnt`.
   Por ejemplo, para `D:\ARCHIVOS\Desktop\SO_tarea1`:

   ```bash
   cd /mnt/d/ARCHIVOS/Desktop/SO_tarea1
   ```

4. Compilar:

   ```bash
   make
   ```

   Esto ejecuta `gcc -Wall -Wextra -std=gnu11` sobre todos los `.c` y genera el ejecutable `mishell`.

5. Ejecutar la shell:

   ```bash
   ./mishell
   ```

6. Para salir, escribir `exit` o presionar Ctrl+D.

7. Para borrar los archivos generados (`*.o` y `mishell`):

   ```bash
   make clean
   ```

## Compilación y ejecución con Docker (alternativa)

Si no se tiene WSL ni Linux, se puede usar Docker con la imagen oficial `gcc`, que ya trae `gcc` y
`make`. Requiere Docker instalado y en ejecución (por ejemplo, Docker Desktop).

1. Abrir una terminal en la carpeta del proyecto (donde está el `Makefile`).

2. Compilar y ejecutar la shell dentro de un contenedor, montando la carpeta actual:

   En PowerShell (Windows):

   ```powershell
   docker run --rm -it -v "${PWD}:/work" -w /work gcc:latest bash -c "make && ./mishell"
   ```

   En Linux, macOS o Git Bash:

   ```bash
   docker run --rm -it -v "$(pwd):/work" -w /work gcc:latest bash -c "make && ./mishell"
   ```

   La primera vez Docker descarga la imagen `gcc`, que pesa más de 1 GB. Las opciones `-it` son
   necesarias para tener una terminal interactiva; sin ellas no se puede usar Ctrl+C ni salir de
   `pmon`.

3. Para salir de la shell, escribir `exit` o presionar Ctrl+D. Como se usa `--rm`, el contenedor se
   elimina al salir.

4. `make` deja los archivos generados (`*.o` y `mishell`) en la carpeta del proyecto. Para borrarlos:

   ```bash
   docker run --rm -v "$(pwd):/work" -w /work gcc:latest make clean
   ```

   (en PowerShell, reemplazar `$(pwd)` por `${PWD}`).

Los procesos que se lancen dentro de `mishell` (por ejemplo `sleep 30 &`) viven solo dentro del
contenedor. `pmon` y `jobs` los ven sin problema, porque `/proc` es el del contenedor.

## Uso

El prompt muestra el directorio actual: `Shelltarea:/ruta/actual$`.

### Comandos internos

| Comando | Descripción |
|---|---|
| `cd [dir]` | Cambia de directorio. Sin argumentos va a `$HOME`. |
| `exit [n]` | Termina la shell con código de salida `n` (0 por defecto). |
| `jobs` | Lista los procesos en background: número de job, PID, estado y comando. |
| `pmon [segundos]` | Monitor de los jobs en background (ver más abajo). Por defecto refresca cada 2 s. |

Ejemplo de `cd` y `exit`:

```
Shelltarea:/home/estudiante$ cd /tmp
Shelltarea:/tmp$ cd
Shelltarea:/home/estudiante$ exit 3
```

`cd` sin argumentos vuelve a `$HOME`, y `exit 3` termina la shell con código de salida 3.

### Redirecciones

```
Shelltarea:/tmp$ sort < datos.txt > datos_ordenados.txt
Shelltarea:/tmp$ echo hola > salida.txt
Shelltarea:/tmp$ echo otra linea >> salida.txt
Shelltarea:/tmp$ cat salida.txt
hola
otra linea
```

Los operadores `<`, `>` y `>>` deben ir separados por espacios.

### Pipes

Se pueden encadenar tantos comandos como se quiera (hasta 64):

```
Shelltarea:/tmp$ ls -l | grep ".c" | wc -l
27
Shelltarea:/tmp$ cat datos.txt | sort | uniq | wc -l
3
```

La shell espera a que termine todo el pipeline antes de mostrar el siguiente prompt.

### Background

Si la línea termina en `&`, el comando (o el pipeline completo) se ejecuta sin bloquear la shell:

```
Shelltarea:/tmp$ sleep 30 &
[1] 4821
Shelltarea:/tmp$ yes > /dev/null &
[2] 4830
Shelltarea:/tmp$ jobs
[1] 4821 Ejecutando sleep 30
[2] 4830 Ejecutando yes
```

`yes > /dev/null &` lanza un proceso que imprime `y` sin parar, pero con la salida descartada y en
background. Sirve para ver un proceso con uso alto de CPU en `pmon`.

Cuando el job termina, la shell avisa antes de mostrar el siguiente prompt:

```
[1]+ Done sleep 30
```

Los procesos terminados se recogen con un manejador de `SIGCHLD` (`waitpid(-1, &status, WNOHANG)` en
ciclo), por lo que no quedan procesos zombie.

### Señales

- Ctrl+C y Ctrl+\ **no cierran la shell**: ella los ignora.
- Ctrl+C termina el proceso (o el pipeline) que está en primer plano.
- Los procesos en background no se ven afectados por Ctrl+C.

Ejemplo, con `yes` corriendo en background:

```
Shelltarea:/tmp$ sleep 100
^C
Shelltarea:/tmp$ jobs
[2] 4830 Ejecutando yes
```

Ctrl+C terminó `sleep 100` (que estaba en primer plano) y volvió al prompt. La shell y el proceso en
background (`yes`) siguen vivos.

### pmon

`pmon [segundos]` muestra una tabla con los jobs en background activos, y la vuelve a imprimir cada
`segundos` segundos. Termina con Ctrl+C y devuelve el control a la shell.

```
Shelltarea:/tmp$ pmon 2

PID     COMANDO    ESTADO  CPU(%)  RSS(KB)
4821    sleep 30   S       0.0     412
4830    yes        R       97.3    360
^C
Shelltarea:/tmp$
```

- **ESTADO**: `R` ejecutando, `S` durmiendo, `Z` zombie, `T` detenido (campo `state` de
  `/proc/[pid]/stat`).
- **CPU(%)**: `(delta de utime+stime) / (segundos reales transcurridos)` entre dos lecturas
  sucesivas. La primera lectura de un proceso muestra 0.0 porque aún no hay una lectura anterior.
- **RSS(KB)**: campo `VmRSS` de `/proc/[pid]/status`.

Si un proceso monitoreado termina, deja de aparecer en la siguiente actualización.

## Estructura del proyecto

| Archivo | Contenido |
|---|---|
| `mishell.c` | Ciclo principal: prompt, lectura, señales de la shell. |
| `parser.c` / `parser.h` | Parseo de la línea: tokens, comillas, pipes, redirecciones y `&`. |
| `builtins.c` / `builtins.h` | Comandos internos `cd`, `exit`, `jobs` y `pmon`. |
| `executor.c` / `executor.h` | `fork`/`execvp`, redirecciones, pipes de N comandos. |
| `jobs.c` / `jobs.h` | Tabla de jobs en background y manejador de `SIGCHLD`. |
| `pmon.c` / `pmon.h` | Monitor de procesos con `/proc`, `alarm` y `SIGALRM`. |
| `Makefile` | Genera el ejecutable `mishell`. |


