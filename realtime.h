#ifndef __REALTIME__H__
#define __REALTIME__H__

#include <sys/time.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define APROXCICLES (1024*16)
#define EXTRALEN 2.625

uint64_t realtime_timeofday_old;
uint64_t realtime_cicles_old;
uint64_t realtime_hpet_hz;

#define PRECCISION  10000000ull // = 100ns
//2^64 = 	18.446.744.073.709.551.616
//       	36.450.791.397.000.000.000
//			xx.xxx.xxx.xxx.x00.000.000
//			18.446.744.073.7 ciclos ~ 76.8s @ 2.4hz
//			 3.647.989.712.700.000.000
#define PRECCISIONu 10ull

/**
 * Inicializa el modulo
 **/
inline void realtime_init(void);

/**
 * Sincroniza el reloj virtual con el reloj del sistema
 **/
inline void realtime_sync_real(void);

/**
 * Sincroniza el reloj virtual de forma simulada
 **/
inline void realtime_sync(void);

/**
 * Optiene el tiempo
 **/
inline uint64_t realtime_get(void); 

/**
 * optiene una aproximacion del tiempo en funcion del tamanio del paquete.
 * cada APROXCICLES se corrijen los posibiles errores.
 **/
inline uint64_t realtime_getAprox(unsigned tam);


/*** PRIVATE ***/
inline uint64_t realtime_get_slow(void);
#endif
