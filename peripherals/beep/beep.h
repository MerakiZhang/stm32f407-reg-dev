#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f4xx.h"

void beep_init(void);
void beep_on(void);
void beep_off(void);
void beep_toggle(void);

#endif /* __BEEP_H */
