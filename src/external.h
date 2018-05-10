#ifndef __EXTERNAL__H__
#define __EXTERNAL__H__

#include <stdint.h>
#ifndef __EXTERNAL__H__NOTINCLUDEMAIN
#include "main.h"
#endif

/**
 * Obtiene los argumentos (incluidos los de la base) para configurarse
 * NOTA: ya están usados: rx, tx, w, rsz, bsz, pos-lb
 * RET != 0 significa error
 **/
int external_config(int argc, char **argv);

/**
 * Realiza las reservas de memoria y demás requerimientos necesarios para el uso de la aplicación externa
 **/
void external_init(void);

/**
 * Se ejecuta unicamente al iniciarse el worker
 **/
void external_first_exec(struct app_lcore_params *lp, uint32_t bsz_rd, uint32_t bsz_wr);

/**
 * recoge un paquete y lo procesa
 **/
void external_work(struct rte_mbuf * pkt);

/**
 * Encola un paquete y lo envia
 **/
void external_send(struct app_lcore_params_worker *lp, uint32_t bsz_wr, struct rte_mbuf * pkt, uint8_t port);

/**
 * Thread(s) secundario(s)
 **/
void external_slave(void);

#endif
