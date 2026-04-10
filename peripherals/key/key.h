#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx.h"

#define KEY_UP_VAL  4
#define KEY0_VAL    3
#define KEY1_VAL    2
#define KEY2_VAL    1

void    key_init(void);
uint8_t key_scan(uint8_t mode);

#endif /* __KEY_H */
