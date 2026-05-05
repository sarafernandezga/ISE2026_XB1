#ifndef MEM_H
#define MEM_H

#include "cmsis_os2.h"
#include <stdint.h>

// Definiciones para la memoria
#define AT24C_ADDR 0x50 

// Tipos de operaciones permitidas
typedef enum {
    EEPROM_OP_READ = 0,
    EEPROM_OP_WRITE = 1
} EEPROM_OpType_t;

// Estructura del mensaje para la cola
typedef struct {
    EEPROM_OpType_t op_type; // Operación: Leer o Escribir
    uint16_t mem_addr;       // Dirección de memoria interna (ej. 0x0000 a 0x3FFF)
    uint8_t *data_ptr;       // Puntero al buffer de datos (origen para TX, destino para RX)
    uint32_t length;         // Cantidad de bytes a transferir
} MSGQUEUE_EEPROM_t;

// Tamaño de la cola (cuántas peticiones simultáneas puede encolar)
#define MSGQUEUE_EEPROM_OBJECTS 4

// Funciones públicas
int Init_ThEEPROM(void);

extern osMessageQueueId_t EEPROM_Queue_R;
extern osMessageQueueId_t EEPROM_Queue_S;

#endif /* MEM_H */