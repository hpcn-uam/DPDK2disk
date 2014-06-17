#include "realtime.h"
#include <time.h>


/********************* VARIABLES *********************/

//-------------------- Aproximate time

uint64_t aprox_last_time = 0; //ultima medicion del tiempo
unsigned aprox_last_medition = 0; //cuando fue el ultimo ajuste
unsigned aprox_size = 0; //bytes leidos desde la ultima medicion
unsigned aprox_speed = 1250; //velocidad aproximada de transferencia | por defecto 1342B / us = 1342177280 B / s = 10Gb / s
#define PRECCISION     10000000ull // = 100ns
//2^64 = 	18.446.744.073.709.551.616
//       	36.450.791.397.000.000.000
//			xx.xxx.xxx.xxx.x00.000.000
//			18.446.744.073.7 ciclos ~ 76.8s @ 2.4hz
//			 3.647.989.712.700.000.000
#define PRECCISIONu 10ull

#define overflowflag(isOverflow){   \
asm volatile ("pushf ;"             \
     "pop %%rax"                     \
    : "=a" (isOverflow));           \
isOverflow = (isOverflow & 0x800) ;}

static uint64_t eal_tsc_resolution_hz = 0;

/** iDPDK FUNCTIONS **/
static inline uint64_t
rte_rdtsc(void)
{
	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;

	asm volatile("rdtsc" :
		     "=a" (tsc.lo_32),
		     "=d" (tsc.hi_32));
	return tsc.tsc_64;
}
static int
set_tsc_freq_from_clock(void)
{
#ifdef CLOCK_MONOTONIC_RAW
#define NS_PER_SEC 1E9

	struct timespec sleeptime = {.tv_nsec = 5E8 }; /* 1/2 second */

	struct timespec t_start, t_end;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
		uint64_t ns, end, start = rte_rdtsc();
		nanosleep(&sleeptime,NULL);
		clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
		end = rte_rdtsc();
		ns = ((t_end.tv_sec - t_start.tv_sec) * NS_PER_SEC);
		ns += (t_end.tv_nsec - t_start.tv_nsec);

		double secs = (double)ns/NS_PER_SEC;
		eal_tsc_resolution_hz = (uint64_t)((end - start)/secs);
		return 0;
	}
#endif
	return -1;
}

static void
set_tsc_freq_fallback(void)
{
	printf("WARNING: clock_gettime cannot use "
			"CLOCK_MONOTONIC_RAW and HPET is not available"
			" - clock timings may be less accurate.\n");
	/* assume that the sleep(1) will sleep for 1 second */
	uint64_t start = rte_rdtsc();
	sleep(1);
	eal_tsc_resolution_hz = rte_rdtsc() - start;
}
/*
 * This function measures the TSC frequency. It uses a variety of approaches.
 *
 * 1. If kernel provides CLOCK_MONOTONIC_RAW we use that to tune the TSC value
 * 2. If kernel does not provide that, and we have HPET support, tune using HPET
 * 3. Lastly, if neither of the above can be used, just sleep for 1 second and
 * tune off that, printing a warning about inaccuracy of timing
 */
static void
set_tsc_freq(void)
{
	if (set_tsc_freq_from_clock() < 0)
		set_tsc_freq_fallback();

	printf("TSC frequency is ~%lu KHz\n",
			eal_tsc_resolution_hz/1000);
}


/********************* FUNCIONES *********************/

inline void realtime_init(void)
{
	struct timeval tmp;

	set_tsc_freq();
	realtime_hpet_hz 	= eal_tsc_resolution_hz;
	realtime_cicles_old = rte_rdtsc();

	gettimeofday(&tmp, NULL);

	realtime_timeofday_old = tmp.tv_sec * PRECCISION + tmp.tv_usec*PRECCISIONu ;//- realtime_cicles_old;

	aprox_last_time = realtime_get();

	printf(" [REALTIME] iniciado : Hz:%lu cicles:%lu tof:%lu\n", realtime_hpet_hz, realtime_cicles_old, realtime_timeofday_old);
}

inline uint64_t realtime_get_slow(void)
{
	double tmp;	

	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;
	
	asm volatile("rdtsc" :
		 "=a" (tsc.lo_32),
		 "=d" (tsc.hi_32));
	
	tmp=((tsc.tsc_64 - realtime_cicles_old) * (double)PRECCISION);
	return (tmp/ realtime_hpet_hz) + realtime_timeofday_old;
}


inline void realtime_sync_real(void)
{
	struct timeval tmp;

	gettimeofday(&tmp, NULL);
	realtime_cicles_old = rte_rdtsc();
	realtime_timeofday_old = tmp.tv_sec * PRECCISION + tmp.tv_usec*PRECCISIONu ;//- realtime_cicles_old;
	aprox_last_time = realtime_get();

	printf(" [REALTIME] Sync [Real] : cicles:%lu tof:%lu\n", realtime_cicles_old, realtime_timeofday_old);
}

inline void realtime_sync(void)
{
	realtime_timeofday_old = realtime_get_slow();//tmp.tv_sec * PRECCISION + tmp.tv_usec*PRECCISIONu ;//- realtime_cicles_old;
	realtime_cicles_old = rte_rdtsc();
	aprox_last_time = realtime_get();

	printf(" [REALTIME] Sync [Sym]  : cicles:%lu tof:%lu\n", realtime_cicles_old, realtime_timeofday_old);
}

inline uint64_t realtime_get(void)
{
	//ret.tv_sec  = tmp_time / 1000000;
	//ret.tv_usec = tmp_time % 1000000;
	//printf(" [REALTIME] The time is : %llu\n", ((rte_get_tsc_cycles() - realtime_cicles_old) / realtime_hpet_hz) + realtime_timeofday_old);

	// FORMULA = RETURN = (( NOW_CICLES * 1000000 ) - ( OLD_CICLES * 1000000 )) / HZ_in_s + TimeofDay_in_us
	unsigned long long tmp;
	volatile unsigned long long Oflag;
	

	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;
	
	asm volatile("rdtsc" :
		 "=a" (tsc.lo_32),
		 "=d" (tsc.hi_32));
	
	// 6804177530690793536
	//80591153825529000000
	//return ((unsigned long long)((unsigned long long)(((((double)tsc.tsc_64) * (PRECCISION)) / realtime_hpet_hz)) + (unsigned long long)realtime_timeofday_old));
	tmp=((tsc.tsc_64 - realtime_cicles_old) * PRECCISION);
	overflowflag(Oflag)
	if(Oflag)
	{
		realtime_sync();
		return realtime_get();
	}
	return (tmp/ realtime_hpet_hz) + realtime_timeofday_old;
	
	//return ((tsc.tsc_64 * 1000000ul) / realtime_hpet_hz) + realtime_timeofday_old;
	//return ((rte_get_hpet_cycles() * 1000000ul) / realtime_hpet_hz) + realtime_timeofday_old;

	//struct timeval tmp;
	//gettimeofday(&tmp, NULL);

	//return tmp.tv_sec * 1000000ull + tmp.tv_usec;
}

inline uint64_t realtime_getAprox(unsigned tam)
{
	if((aprox_last_medition == APROXCICLES))
	{
		uint64_t aprox_last_new_time,aprox_result; 
		uint64_t aprox_last_last_time = aprox_last_time;
		aprox_result=aprox_last_time + (aprox_size / aprox_speed);
		aprox_last_medition = 1;
		aprox_last_new_time = realtime_get();		
		
		if(aprox_result < aprox_last_new_time)
		{
//			printf("Tiempo Retrasado (Desviación %lu): ",aprox_last_new_time - aprox_result);
			aprox_last_time = aprox_last_new_time;
			aprox_speed = (uint64_t)((double)aprox_size / (double)(aprox_last_time - aprox_last_last_time));
		}
		else
		{			
//			printf("Tiempo SobreAvanzado (Desviación %lu): ",aprox_result - aprox_last_new_time);//problema, salto hacia atras
			aprox_speed = (uint64_t)((double)aprox_size / (double)(aprox_last_new_time - aprox_last_last_time));
		}
		if((aprox_speed==0))
			aprox_speed = 1;
//		printf("new speed is: %u\n",aprox_speed);
		aprox_size = 0;
	}
	else
		aprox_last_medition++;

	aprox_size += (tam+EXTRALEN);
	//printf("time = %lu\n",aprox_last_time + (aprox_size / aprox_speed));
	return aprox_last_time + (aprox_size / aprox_speed);
}

//#define TESTTIME 15000000
#define TESTREPEAT 60
#ifdef TESTTIME

#define TESTFUNC  realtime_get()
//#define TESTFUNC  realtime_getAprox(60)

#define TESTCALL 0 //3 aprox 10g, 33 aprox 1g

int main(int argc, char**argv)
{
	puts("iniciando...");
	realtime_init();
	volatile uint64_t testv;

	int i=0,j=0,kt;
	uint64_t cost = 0;
	uint64_t tmp = 0;

	uint64_t result = 0;

	printf("iniciando test con %d pruebas y %d repeticiones\n",TESTTIME,TESTREPEAT);

	printf("Test iniciado en %lu\n",realtime_get());

	for (j=0;j<TESTREPEAT;j++)
	{
		printf("paso %d de %d\n",j+1,TESTREPEAT);

		tmp = realtime_get();
		

		for(i=0;i<TESTTIME;i++)
		{
		//if(realtime_get()==realtime_get())
		//puts("ERROR");
			result += TESTFUNC;
		//	for (kt = 0;kt<TESTCALL;kt++) testv = realtime_get();
		}

		cost += realtime_get() - tmp;
	}
	printf("testv = %lu\n",testv);

	printf("Test finalizado en %lu\n",realtime_get());
	printf("[DEBUG] %lu\n",result);
	realtime_init();
	
	printf("\n\tmedia del coste del calculo del tiempo para %d paquetes: %.2lfus (total = %2lfus). Por paquete: %.2lfns - %.2lfps. (%.2lf %% del tiempo consumido)\n\n", TESTTIME , (double)cost / (double)(TESTREPEAT*PRECCISIONu), (double)cost / (double)(PRECCISIONu), ((double)cost / (double)(TESTREPEAT*TESTTIME))* (double)10. , ((double)cost / (double)(TESTREPEAT*TESTTIME)) * (double)10000., (((double)cost / (double)(TESTREPEAT*TESTTIME)))/0.6160);

	return 0;
}

#endif
