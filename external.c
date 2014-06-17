#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_spinlock.h>
#include <rte_launch.h>

#include <getopt.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "external.h"
#include "realtime.h"
#include "pcap_jose.h"
#include "main.h"

#ifndef APP_STATS
#define APP_STATS                    1000000
#endif

#define FILEOPTIONS   (O_CREAT | O_WRONLY /*| O_NONBLOCK | O_NDELAY*/ /*| O_TRUNC*/ | O_DIRECT | O_SYNC)

/*Common*/
struct app_lcore_params * tmp_lp;
uint32_t tmp_bsz_wr;
uint32_t tmp_bsz_rd;

/*Multicore*/
#define NBUFS (4)
rte_spinlock_t locks_R [NBUFS];
rte_spinlock_t locks_W [NBUFS];
int WSlaveid = -1;
unsigned curBuf=0;

/*Disco*/
char outputFolfer [150]    ={0};
uint64_t maxbytes     = 1;
uint64_t fileWrote    = 0; 

/*PCAP*/
pcap_hdr_tJZ defaultPcapHeader;

/*Buffers*/
#define BUFFERKIND uint8_t
BUFFERKIND* diskBuffer [NBUFS];
char changeFile [NBUFS];
unsigned bytesLeft [NBUFS];
uint64_t FileStamp [NBUFS];
BUFFERKIND* diskBufferPTR = NULL;
//BUFFERKIND* diskBufferPTRdw = NULL;
//BUFFERKIND* diskBufferPTRLimit = NULL;
uint64_t diskBufferWrote=0; //32M
uint64_t diskBufferSize=1024*1024*1024; //1G
uint64_t diskBurst=(1024*1024*1024);//(1024*1024*1024); //4K

/**
 * Obtiene los argumentos (incluidos los de la base) para configurarse
 * NOTA: ya están usados: rx, tx, w, rsz, bsz, pos-lb
 * RET < 0 significa error
 **/
inline int external_config(int argc, char **argv)
{
	int opt;
	char **argvopt;
	int option_index;
	static struct option lgopts[] = {
		{"folder", 1, 0, 0},
		{"maxgiga", 1, 0, 0},
		{"n2dW", 1, 0, 0}, //worker de n2disk
		/*no tocar...*/
		{"rx", 1, 0, 0},
		{"tx", 1, 0, 0},
		{"w", 1, 0, 0},
		{"rsz", 1, 0, 0},
		{"bsz", 1, 0, 0},
		{"pos-lb", 1, 0, 0},
		{NULL, 0, 0, 0}
	};
	argvopt = argv;
	while ((opt = getopt_long(argc, argvopt, "",
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* long options */
		case 0:
			//printf("%s - %s\n",lgopts[option_index].name,optarg);
			if (!strcmp(lgopts[option_index].name, "folder")) {
				strncpy(outputFolfer,optarg,150);
				printf("[EXTERNAL] Carpeta establecida a %s.\n",outputFolfer);
			}
			if (!strcmp(lgopts[option_index].name, "maxgiga")) {
				maxbytes = atoi(optarg);
				maxbytes = maxbytes << 30;
				//maxbytes -= 2049;
				printf("[EXTERNAL] Tamaño maximo de fichero establecido a %ld bytes\n",maxbytes);
			}
			if (!strcmp(lgopts[option_index].name, "n2dW")) {
				WSlaveid = atoi(optarg);
				if(WSlaveid >= APP_MAX_LCORES || rte_lcore_is_enabled(WSlaveid) == 0)
				{
					puts("[EXTERNAL] WSlave no válido");
					exit(-1);
				}
				if(app.lcore_params[WSlaveid].type != e_APP_LCORE_DISABLED)
				{
					puts("[EXTERNAL] WSlave no válido - Ya en uso");
					exit(-1);
				}
				app.lcore_params[WSlaveid].type = e_APP_LCORE_WORKER_SLAVE;
				printf("[EXTERNAL] Se define un WSlave en el Core %d\n", WSlaveid);
			}
			break;
		default:
			puts(lgopts[option_index].name);
			break;
		}
	}
	
	if (outputFolfer[0]==0) {//esta vacio
		printf("[EXTERNAL] No se introdujo ningun fichero...\n");
		return -1;
	}
	if (WSlaveid<0) {//no hay WSlaveid
		printf("[EXTERNAL] No se introdujo ningun WSlaveid...\n");
		return -1;
	}
	
	return 0;
}

/**
 * Realiza las reservas de memoria y demás requerimientos necesarios para el uso de la aplicación externa
 **/
inline void external_init(void)
{
	int i,j;
	/*Abrimos el primer fichero*/
	//sprintf(outputFile,"%s/%d.bin",outputFolfer,counter);
	//printf("[EXTERNAL] Abriendo el primer fichero %s...",outputFile);
	//output = open( outputFile, FILEOPTIONS , S_IWUSR );
	//if (output==-1) {
	//	puts("ERROR!");
	//	perror("open");
	//	exit(-1);
	//}
	//puts("OK!");
	
	printf("[EXTERNAL] Reservando recursos...");
	for(i=0;i<NBUFS;i++)
	{
		diskBuffer[i] = rte_malloc ("DiskBuff", sizeof(BUFFERKIND)*diskBufferSize, sizeof(BUFFERKIND)*diskBurst );
		
		if (!diskBuffer[i]) {
			puts("Malloc - ERROR!");
			exit(-1);
		}
		/*SpinLocks*/
		rte_spinlock_init (&(locks_R[i]));
		rte_spinlock_init (&(locks_W[i]));
		rte_spinlock_lock (&(locks_W[i]));
	}
	rte_spinlock_lock (&(locks_R[0]));
	
	diskBufferPTR   = diskBuffer[0];
	//diskBufferPTRdw = diskBuffer[0];
	//diskBufferPTRLimit = diskBuffer[0] + diskBufferSize * sizeof(BUFFERKIND) - 2 * diskBurst * sizeof(BUFFERKIND);
	diskBufferWrote = 0;
	fileWrote=0;
	
	changeFile[0]=1; //crea un fichero inicial
	
	puts("OK!");
	
	printf("[EXTERNAL] Hotting memories...");fflush(stdout);
	
	srand(time(NULL));
	for(i=0;i<NBUFS;i++)
	{
		for (j=0;j<10;j++)
		{
			memset(diskBuffer[i], rand(), sizeof(BUFFERKIND)*diskBufferSize);
		}
	}
	
	/*PCAP - CommonlyVersion*/
	defaultPcapHeader.magic_number  = 0xA1B2C3D4;
    defaultPcapHeader.version_major = 0x0002;
    defaultPcapHeader.version_minor = 0x0004;
    defaultPcapHeader.thiszone      = 0x00000000;
    defaultPcapHeader.sigfigs       = 0x00000000;
    defaultPcapHeader.snaplen       = 0xFFFF;
    defaultPcapHeader.network       = 0x00000001;
			
	rte_memcpy(diskBufferPTR,&defaultPcapHeader,sizeof(defaultPcapHeader));
	diskBufferPTR	+= sizeof(defaultPcapHeader);
	diskBufferWrote	+= sizeof(defaultPcapHeader);
	fileWrote		+= sizeof(defaultPcapHeader);
	
	puts("OK!");
	
	realtime_init();
}

//#define _TIMECORE_
#ifdef  _TIMECORE_
struct timeval start_ewr, end_ewr;
#endif
inline void external_first_exec(struct app_lcore_params *lp, uint32_t bsz_rd, uint32_t bsz_wr)
{
		tmp_lp 		= lp ;
		tmp_bsz_wr 	= bsz_wr;
		tmp_bsz_rd 	= bsz_rd;
		
		realtime_sync_real();
		
		#ifdef _TIMECORE_
		  gettimeofday(&start_ewr, NULL);
		#endif
}

/**
 * recoge un paquete y lo procesa
 **/
inline void external_work(struct rte_mbuf * pkt)
{
			unsigned pktLen,tmp,pcapHeaderLen;
			uint8_t* pktData;
			uint64_t timestamp;
			pcaprec_hdr_tJZ pcapPktHeader;
						
			pktLen = pkt->pkt.pkt_len;
			pktData= pkt->pkt.data;
			
			//timestamp = realtime_get();
			timestamp = realtime_getAprox(pktLen);
			
			pcapPktHeader.ts_sec	= timestamp/PRECCISION;
			pcapPktHeader.ts_usec	= (timestamp/PRECCISIONu)%1000000;
			pcapPktHeader.incl_len	= pktLen;
			pcapPktHeader.orig_len	= pktLen;
			pcapHeaderLen			= sizeof(pcaprec_hdr_tJZ);

			if((diskBufferWrote+pktLen+sizeof(pcaprec_hdr_tJZ)) >= diskBufferSize) // comprobamos si el buffer se va a llenar
			{
				if((fileWrote+pktLen+sizeof(pcaprec_hdr_tJZ)) >= maxbytes) //Final de fichero alcanzado
				{
					changeFile[curBuf]	= 1;
					bytesLeft [curBuf]	= maxbytes-fileWrote;
					FileStamp [curBuf]	= timestamp/(PRECCISION/10ull);
					fileWrote=0;
				}
				else
				{	//copiamos hasta llenar
					tmp=diskBufferSize-diskBufferWrote; // espacio disponible
						
					//PCAP PACKET HEADER
					if(tmp>=sizeof(pcaprec_hdr_tJZ))
					{
						rte_mov16 (diskBufferPTR, (uint8_t *) (&pcapPktHeader));
						fileWrote		+= sizeof(pcaprec_hdr_tJZ);
						diskBufferPTR	+= sizeof(pcaprec_hdr_tJZ);
						pcapHeaderLen	 = 0;
						tmp				-= sizeof(pcaprec_hdr_tJZ);
					}
					else
					{
						rte_memcpy(diskBufferPTR,&pcapPktHeader,tmp);
						fileWrote		+= tmp;
						diskBufferPTR   += tmp;
						pcapHeaderLen	-= tmp;
						tmp				 = 0;
					}
					
					//PACKET
					rte_memcpy(diskBufferPTR, pktData, tmp);
					pktLen    -= tmp;
					pktData   += tmp;
					fileWrote += tmp;
					
					changeFile[curBuf]=0;
					bytesLeft [curBuf]=0;
				}
				
				//Cambiamos de buffer
				
				#ifdef _TIMECORE_
				gettimeofday(&end_ewr, NULL);
				printf("Buffer Wroted at %lf Gb/s\n", ((maxbytes)/(((end_ewr.tv_sec * 1000000. + end_ewr.tv_usec)
			  - (start_ewr.tv_sec * 1000000. + start_ewr.tv_usec))/1000000.))/(1024*1024*1024./8.));
				#endif
				rte_spinlock_unlock (&(locks_W[curBuf]));						

				curBuf=(curBuf+1)%NBUFS;
				
				rte_spinlock_lock   (&(locks_R[curBuf]));
				#ifdef _TIMECORE_
				  gettimeofday(&start_ewr, NULL);
				#endif
				
				diskBufferPTR=diskBuffer[curBuf];
				diskBufferWrote=0;
				
				if(fileWrote==0) //Si el fichero está vacio, escribimos cabecera PCAP
				{
					rte_memcpy(diskBufferPTR,&defaultPcapHeader,sizeof(defaultPcapHeader));
					diskBufferPTR	+= sizeof(defaultPcapHeader);
					diskBufferWrote	+= sizeof(defaultPcapHeader);
					fileWrote		+= sizeof(defaultPcapHeader);
				}
			}
			
			rte_memcpy(diskBufferPTR, ((uint8_t*)(&pcapPktHeader))+(sizeof(pcaprec_hdr_tJZ)-pcapHeaderLen), pcapHeaderLen);
			
			rte_memcpy(diskBufferPTR+pcapHeaderLen,pktData,pktLen);
			
			diskBufferPTR	+= (pktLen+pcapHeaderLen);
			diskBufferWrote	+= (pktLen+pcapHeaderLen);
			fileWrote		+= (pktLen+pcapHeaderLen);
				
			
			#ifndef TESTMAIN
			#ifndef EXTHDD
			rte_pktmbuf_free(pkt); //liberamos el pkt
			#endif
			#endif
			
			return;
			
			//port = (!pkt->pkt.in_port) & 1; /*Forwarding...*/
			//external_send(lp, bsz_wr, pkt, port);
}


/**********************************************************************/
/**
 * Encola un paquete y lo envia
 **/
inline void external_send(struct app_lcore_params_worker *lp, uint32_t bsz_wr, struct rte_mbuf * pkt, uint8_t port)
{
			uint32_t pos;
			int ret;
	
			pos = lp->mbuf_out[port].n_mbufs;

			lp->mbuf_out[port].array[pos ++] = pkt;
			if (likely(pos < bsz_wr)) {
				lp->mbuf_out[port].n_mbufs = pos;
				return;
			}

			/*Se incia el proceso de encolado al nucleo de envio...*/
			ret = rte_ring_sp_enqueue_bulk(
				lp->rings_out[port],
				(void **) lp->mbuf_out[port].array,
				bsz_wr);

		#if APP_STATS
			lp->rings_out_iters[port] ++;
			if (ret == 0) {
				lp->rings_out_count[port] += 1;
			}
			if (lp->rings_out_iters[port] == APP_STATS){
				printf("\t\tWorker %u out (NIC port %u): enq success rate = %.2f (%u/%u)\n",
					lp->worker_id,
					(uint32_t) port,
					((double) lp->rings_out_count[port]) / ((double) lp->rings_out_iters[port]),
					(uint32_t)lp->rings_out_count[port],
					(uint32_t)lp->rings_out_iters[port]);
				lp->rings_out_iters[port] = 0;
				lp->rings_out_count[port] = 0;
			}
		#endif

			if (unlikely(ret == -ENOBUFS)) {
				uint32_t k;
				for (k = 0; k < bsz_wr; k ++) {
					struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
					rte_pktmbuf_free(pkt_to_free);
				}
			}

			lp->mbuf_out[port].n_mbufs = 0;
			lp->mbuf_out_flush[port] = 0;
}


/**
 * Thread(s) secundario(s)
 **/
//#define _TIMESLAVE_
void external_slave(void)
{
	int i=0, succes=-1, output=-1;
	unsigned long long j;
	char outputFile   [150+50] ={0};
	
	#ifdef _TIMESLAVE_
	struct timeval start, end;
	#endif
	
	sprintf(outputFile,"%s/%llu.pcap",outputFolfer,(realtime_get()/(PRECCISION/10)));
	output = open( outputFile,  FILEOPTIONS, S_IWUSR );
	if (output==-1) { perror("open"); exit(-1);	}
	
	#ifdef _TIMESLAVE_
	  gettimeofday(&start, NULL);
	#endif
			
	for ( ; ; ) 
	{
		//Start Writing
		//printf("W:   locking W%d\n",i); fflush(stdout);
		rte_spinlock_lock   (&(locks_W[i]));
		
		/***********************************/
				
		for(j=0; (j/*+diskBurst*/) </*=*/ (diskBufferSize/*-bytesLeft[i]*/); j+=diskBurst)
		{
			succes = write( output,  diskBuffer[i]+j,  diskBurst);
			
			if(succes==-1)
				puts("[EXTERNAL] Error en escribir en fichero...");
		}
		/*if(j < (diskBufferSize-bytesLeft[i])) //NotAllignedWrite:
		{
			//printf("Unalligned Write of %llu\n", (diskBufferSize-bytesLeft[i])-j );
			succes = write( output,  diskBuffer[i]+j,  (diskBufferSize-bytesLeft[i])-j);
			
			if(succes==-1)
				puts("[EXTERNAL] Error en escribir en fichero (unalligned)...");
		}*/
		if(bytesLeft[i])
		{
			ftruncate(output, maxbytes-bytesLeft[i]);
		}
		
		if(changeFile[i])
		{			
			close(output); 

			#ifdef _TIMESLAVE_
			gettimeofday(&end, NULL);
			printf("File Wroted at %lf Gb/s\n", ((maxbytes)/(((end.tv_sec * 1000000. + end.tv_usec)
		  - (start.tv_sec * 1000000. + start.tv_usec))/1000000.))/(1024*1024*1024./8.));
			#endif
			
			sprintf(outputFile,"%s/%lu.pcap",outputFolfer,FileStamp[i]);
			if(FileStamp[i]==0)
			{
				puts("memory 0 not expected");
				exit(-1);
			}
			//printf("[EXTERNAL] Cambiando fichero a %s...",outputFile);
			output = open( outputFile,  FILEOPTIONS, S_IWUSR );	
			#ifdef _TIMESLAVE_
			  gettimeofday(&start, NULL);
			#endif
			if (output==-1) {
				puts("ERROR!");
				perror("open");
				exit(-1);
			}//puts("OK");
		}
		
		/***********************************/
		
		//End Writing
		//printf("W: unlocking R%d\n",i);fflush(stdout);
		rte_spinlock_unlock (&(locks_R[i]));
		
		//Actualización de contadores
		 i=(i+1)%NBUFS;
	}	
	return;
}

#ifdef TESTMAIN
#ifdef EXTHDD

#define SIZEPKTS 60.//1518.//60.
#define NUMPKTS (1024*1024*1024ull/(SIZEPKTS/60))

#include <time.h>
#include <sys/time.h>

int main(int argc, char **argv)
{
	struct rte_mbuf test;
	unsigned long long i;
	uint32_t lcore;
	int ret;
	struct timeval start, end;
	
	test.pkt.pkt_len=SIZEPKTS;
	
	test.pkt.data=strdup("012345678901234567890123456789012345678901234567890123456789XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	
	puts ("starting");
	
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;
	argc -= ret;
	argv += ret;
	
	ret = app_parse_args(argc, argv);
	if (ret < 0) {
		app_print_usage();
		return -1;
	}
	
	
	external_config(argc, argv);
	external_init();
	
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
	if(argc>1)
		printf("%s\n",argv[0]);
		
	printf("Simulando %.0lf paquetes de tamaño %.0f Bytes\n",NUMPKTS,SIZEPKTS);
	
  gettimeofday(&start, NULL);
	for (i=0;i<NUMPKTS;i++)
		external_work(&test);
	for (i=0;i<(NBUFS-1);i++)
	{
		curBuf=(curBuf+1)%NBUFS;
		rte_spinlock_lock   (&(locks_R[curBuf]));
	}
  gettimeofday(&end, NULL);
				  
  printf("%ld\n", ((end.tv_sec * 1000000 + end.tv_usec)
		  - (start.tv_sec * 1000000 + start.tv_usec)));
		  
		  
  printf("%lf Gb/s\n", ((NUMPKTS*SIZEPKTS)/(((end.tv_sec * 1000000. + end.tv_usec)
		  - (start.tv_sec * 1000000. + start.tv_usec))/1000000.))/(1024*1024*1024./8.));
		  
		  
  printf("%lf Gb/s Ethernet\n", ((NUMPKTS*(SIZEPKTS+4+8+12))/(((end.tv_sec * 1000000. + end.tv_usec)
		  - (start.tv_sec * 1000000. + start.tv_usec))/1000000.))/(1024*1024*1024./8.));
		  
	puts ("done");
	
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0) {
			return -1;
		}
	}
	//close(output);
	return 0;
}

#endif
#endif
