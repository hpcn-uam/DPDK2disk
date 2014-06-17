//#define _GNU_SOURCE
#define MEBIBYTE (1024*1024)
#define KEBIBYTE (1024)
#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


/**
 * Calcula el tamaño optimo de los bloques para escritura en un disco
 * @param benchmark_blocksize tamaño del bloque inicial
 * @param dataToWrite tamaño de la memoria que se escribira en disco
 * @param numberOfRepeats numero de veces que se escribirá dicha memoria ( se hace una media al final )
 * @param isSync abre el fichero en modo sincrono, lo que afectara al rendimiento
 * @param printInfo imprime informacion por pantalla
 * @return retorna, en bytes, el tamaño de bloque optimo encontrado
 */
unsigned hddSpeedTest (unsigned benchmark_blocksize, unsigned dataToWrite, unsigned numberOfRepeats,int isSync,int printInfo)
{
	unsigned optimal_blocksize = benchmark_blocksize ;

	unsigned total_done = 0 ;
	unsigned i,k;
	unsigned done = 0 ;
	GTimer *timer = g_timer_new ();
	double smallest_time = 1000000 ;
	double elapsed_time = 0;
	int succes = 1 ;
	void  *buffer;

	buffer = calloc(1,dataToWrite);
	gulong dumb_API_needs_this_variable;
	
	int f;

	//Benchmark copy times using different block sizes to determine optimal size
	while ( succes > 0 &&
		benchmark_blocksize <= dataToWrite )
	{
		
		if(isSync)
			f = open( "disco/test.txt",  O_CREAT | O_WRONLY | O_SYNC | O_DIRECT     , S_IWUSR );
		else
			f = open( "disco/test.txt",  O_CREAT | O_WRONLY | O_NONBLOCK | O_NDELAY	, S_IWUSR );
	
		/**********/
		g_timer_reset(timer);
		
		for (k=0;k<numberOfRepeats;k++)
			for (i=0;i<dataToWrite/benchmark_blocksize; i++)
				succes = write(  f,  buffer,  benchmark_blocksize );
		
		g_timer_stop(timer);
		/**********/
		
		close(f);
		

		elapsed_time = g_timer_elapsed (timer, &dumb_API_needs_this_variable);

		elapsed_time = elapsed_time / (numberOfRepeats/**2*/);
		
		if(printInfo)
		{
			printf("t: %lf | d : %u\n",elapsed_time,benchmark_blocksize);
			fflush(stdout);
		}
		
		if ( elapsed_time <= smallest_time )
		{
			smallest_time = elapsed_time ;
			optimal_blocksize = benchmark_blocksize ; 
		}
		benchmark_blocksize *= 2 ;

	}
	
    g_timer_destroy (timer);
    
	if(printInfo)
		printf("Optimal blocksize is: %u\n",optimal_blocksize);
				
	return optimal_blocksize;
}

int main()
{
	hddSpeedTest (16*KEBIBYTE, 1024*MEBIBYTE, 16, 0, 1);
}

