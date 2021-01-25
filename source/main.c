/*
 * main.c
 */
#define __MAIN_C_dc83edef7fb74d0f88488010fe346ac7	1

#include "main.h"
#include "libraries/API/apipage.h"
#include "libraries/avrlibs-baerwolf/include/extfunc.h"
#include "libraries/avrlibs-baerwolf/include/hwclock.h"
#include "libraries/avrlibs-baerwolf/include/cpucontext.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* http://nongnu.org/avr-libc/user-manual/modules.html */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include <util/delay.h>


// hardware depended - only avrs for tinyusbboard at the moment //
#if (defined (__AVR_ATmega8__) || defined (__AVR_ATmega8A__) || \
     defined (__AVR_ATmega88__) || defined (__AVR_ATmega88P__) || defined (__AVR_ATmega88A__) || defined (__AVR_ATmega88PA__) || \
     defined (__AVR_ATmega168__) || defined (__AVR_ATmega168P__) || defined (__AVR_ATmega168A__) || defined (__AVR_ATmega168PA__) || \
     defined (__AVR_ATmega328__) || defined (__AVR_ATmega328P__) || defined (__AVR_ATmega328A__) || defined (__AVR_ATmega328PA__) || \
     defined (__AVR_ATmega164__) || defined (__AVR_ATmega164P__) || defined (__AVR_ATmega164A__) || defined (__AVR_ATmega164PA__) || \
     defined (__AVR_ATmega324__) || defined (__AVR_ATmega324P__) || defined (__AVR_ATmega324A__) || defined (__AVR_ATmega324PA__) || \
     defined (__AVR_ATmega644__) || defined (__AVR_ATmega644P__) || defined (__AVR_ATmega644A__) || defined (__AVR_ATmega644PA__) || \
     defined (__AVR_ATmega1284__)|| defined (__AVR_ATmega1284P__)||                                 defined (__AVR_ATmega32__)    || \
   0)
void __hwclock_timer_init(void) {
  OCR1A=0xffff;
  OCR1B=0x8000;
  TCNT1=0;
  TCNT0=0;
}

void __hwclock_timer_start(void) {
//PWM TIMER1 (R&G)
  TCCR1A=0b00000000;
  TCCR1B=0b00000100;

//TIMER0
#if (defined (__AVR_ATmega8__) || defined (__AVR_ATmega8A__))
  TCCR0 =0b00000001; /* activate timer0 running */
#else /* not atmega8 */
  TCCR0B=0b00000001; /* activate timer0 running */
#endif

  /* calibrate timer0 to prescaler of timer1                */
  /* in general we need to use 2-cycle memory access:       */
  /* This introduces a problem, since the prescaler can     */
  /* overflow not just inbetween the two tcnthl accesses    */
  /* but also during execution of the second "LDS"...       */
  /* BUT IT IS NOT THE SAME AS: "IN r18" - "NOP" - "IN r19" */
  asm volatile (
      "timer0_calibrate_again%=:       \n\t"
      "lds  r18, %[tcnthl]             \n\t" /* 2 cycles - sampled after 1 cycle */
      "lds  r19, %[tcnthl]             \n\t" /* 2 cycles - sampled after 1 cycle */
      "inc  r18                        \n\t" /* 1 cycle  */
      "nop                             \n\t" /* 1 cycle to gcd()=1 */
      "cp   r18, r19                   \n\t" /* 1 cycle  */
      "brne timer0_calibrate_again%=   \n\t" /* 2 cycles jumping - 1 cycle continue */


      /* we configure here, as overflow would have happend during sampling of "LDS r19" */
      "ldi  r20, 7                     \n\t" /* 1 cycle  */
      "out  %[tcnt], r20               \n\t" /* 1 cycle  */
      "nop                             \n\t" /* 1 cycle - here TCNT0 must be 7 */

      /* if prescaler overflowed starting at "LDS r19" before sampling... */
      /* ...it overflows again in 254 cycles after that.                  */
      /* (That means if LDS is executed in 253 cycles and tcnthl sampled  */
      /*  in 254 cycles to be incremented one more...)                    */
      /* taking the other opcodes into account leaves us with 246 cycles  */
      "ldi  r20, 246                   \n\t" /* 1 cycle  */
      "timer0_calibrate_busyloop%=:    \n\t"
      "subi r20, 3                     \n\t" /* 1 cycle  */
      "brne timer0_calibrate_busyloop%=\n\t" /* 2 cycles jumping - 1 cycle continue */

      /* here we are 1 cycle ahead of overflow? (due to brne) - 245 cycles passed  */
      /* 253 cycles since last "LDS r19" ...                                       */
      "lds  r19, %[tcnthl]             \n\t" /* 2 cycles - sampled after 1 cycle   */
      "ldi  r20, 3                     \n\t" /* 1 cycle - in case of equal, overflow happens here */
      "cpse r18, r19                   \n\t" /* 1 cycle - equal means overflow when sampled */
      "out  %[tcnt], r20               \n\t" /* 1 cycle - overflow happend before sampling */


      :
      : [tcnt]          "i" (_SFR_IO_ADDR(TCNT0)),
        [tcnthl]		"i"	(_SFR_MEM_ADDR(HWCLOCK_MSBTIMER_VALUEREG_LOW)),
        [tcntll]		"i"	(_SFR_MEM_ADDR(HWCLOCK_LSBTIMER_VALUEREG_LOW))
      : "r20", "r19", "r18"
           );
} /* end of hardware selected "__hwclock_timer_start" */
#else
#	error unsupported AVR
#endif
///////////////////////////////////////////////////////


void init_cpu(void) {
  cli();
  bootupreason=MCUBOOTREASONREG;
  MCUBOOTREASONREG=0;
  wdt_disable();
}

int main(void) {
  init_cpu();

  // YOUR CODE HERE:
  extfunc_initialize();
  EXTFUNC_callByName(cpucontext_initialize);
  EXTFUNC_callByName(hwclock_initialize);

  while (1) {
  }

  EXTFUNC_callByName(hwclock_finalize);
  EXTFUNC_callByName(cpucontext_finalize);
  extfunc_finalize();

  bootloader_startup();
  return 0;
}
