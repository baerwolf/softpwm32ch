/*
 * main.c
 */
#define __MAIN_C_dc83edef7fb74d0f88488010fe346ac7	1

#include "main.h"
#ifdef __AVR_ATmega103__
    #ifndef SPM_PAGESIZE
        #define SPM_PAGESIZE (0)
    #endif
#endif
// #include "libraries/API/apipage.h"
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

#ifndef SOFTPWM_INSANE_OPTIMIZATION
#   define SOFTPWM_INSANE_OPTIMIZATION 0
#endif
#ifndef SOFTPWM_UPDATECYCLES
#   define SOFTPWM_UPDATECYCLES    (32 /*must not be smaller than 32*/)
#endif
static uint8_t __attribute__ ((used,aligned(256))) pwmseq[256];
static uint8_t __attribute__ ((used,aligned(256))) pwmseqalternative[256];
static uint8_t *pwmframe[2] = {pwmseq, pwmseqalternative};

// hardware depended - only avrs for tinyusbboard at the moment //
#if (defined (__AVR_ATmega8__) || defined (__AVR_ATmega8A__) || \
     defined (__AVR_ATmega128__) || defined (__AVR_ATmega103__) || \
     defined (__AVR_ATmega88__) || defined (__AVR_ATmega88P__) || defined (__AVR_ATmega88A__) || defined (__AVR_ATmega88PA__) || \
     defined (__AVR_ATmega168__) || defined (__AVR_ATmega168P__) || defined (__AVR_ATmega168A__) || defined (__AVR_ATmega168PA__) || \
     defined (__AVR_ATmega328__) || defined (__AVR_ATmega328P__) || defined (__AVR_ATmega328A__) || defined (__AVR_ATmega328PA__) || \
     defined (__AVR_ATmega164__) || defined (__AVR_ATmega164P__) || defined (__AVR_ATmega164A__) || defined (__AVR_ATmega164PA__) || \
     defined (__AVR_ATmega324__) || defined (__AVR_ATmega324P__) || defined (__AVR_ATmega324A__) || defined (__AVR_ATmega324PA__) || \
     defined (__AVR_ATmega644__) || defined (__AVR_ATmega644P__) || defined (__AVR_ATmega644A__) || defined (__AVR_ATmega644PA__) || \
     defined (__AVR_ATmega1284__)|| defined (__AVR_ATmega1284P__)|| defined (__AVR_ATmega16__)   || defined (__AVR_ATmega32__)    || \
   0)
void __hwclock_timer_init(void) {
#if (defined (__AVR_ATmega8__) || defined (__AVR_ATmega8A__) || \
    defined (__AVR_ATmega128__) || defined (__AVR_ATmega103__) || defined (__AVR_ATmega16__) || defined (__AVR_ATmega32__))
#define __AVR_mylegacy 1
  OCR2=SOFTPWM_UPDATECYCLES; /* cyles to PWM update */
#else
  OCR2A=SOFTPWM_UPDATECYCLES; /* cyles to PWM update */
#endif
  OCR1A=0xffff;
  OCR1B=0x8000;
  TCNT2=0;
  TCNT1=0;
  TCNT0=0;
}

void __hwclock_timer_start(void) {
//TIMER2
#if (defined(__AVR_mylegacy))
  TCCR2=0b00001001;
#else
  TCCR2A=0b0000001;
  TCCR2B=0b0000001;
#endif

//PWM TIMER1 (R&G)
  TCCR1A=0b00000000;
  TCCR1B=0b00000100;

//TIMER0
#if (defined (__AVR_mylegacy))
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

#if (defined (__AVR_mylegacy))
#   define SOFTPWM_ENABLE(x)   TIMSK|=_BV(OCIE2)
#   define SOFTPWM_DISABLE(x)  TIMSK&=~(_BV(OCIE2))
#   define SOFTPWM_WAITPENDING(x) {while ((TIFR & _BV(OCF2))) { _NOP();}}
#   define SOFTPWM_CLEARPENDING(x) {if (TIFR & _BV(OCF2)) TIFR|=_BV(OCF2);}
#else
#   define SOFTPWM_ENABLE(x)   TIMSK2|=_BV(OCIE2A)
#   define SOFTPWM_DISABLE(x)  TIMSK2&=~(_BV(OCIE2A))
#   define SOFTPWM_WAITPENDING(x) {while ((TIFR2 & _BV(OCF2A))) { _NOP();}}
#   define SOFTPWM_CLEARPENDING(x) {if (TIFR2 & _BV(OCF2A)) TIFR2|=_BV(OCF2A);}
#endif
#else
#	error unsupported AVR
#endif


#if (SOFTPWM_INSANE_OPTIMIZATION)
void __attribute__ ((section(".vectors"),naked,used,no_instrument_function)) __reset__(void);
void __attribute__ ((section (".init0"),naked,used,no_instrument_function)) __startup(void);
int main(void) __attribute__ ((OS_main, section (".init9")));
void __reset__(void) { /* fake __vectors */
       asm volatile (
               "rjmp __startup\n\t"
/* the following is extremly hardware depended - we basically need to do it for every AVRs individually */
#if (defined (__AVR_ATmega8__) || defined (__AVR_ATmega8A__))
               ".space 4        \n\t" /*=NOP*/
#elif (defined (__AVR_ATmega128__) || defined (__AVR_ATmega103__))
               ".space 34       \n\t"
#elif (defined (__AVR_ATmega16__))
               ".space 10       \n\t"
#elif (defined (__AVR_ATmega32__))
               ".space 14       \n\t"
#elif (defined (__AVR_ATmega88__) || defined (__AVR_ATmega88P__) || defined (__AVR_ATmega88A__) || defined (__AVR_ATmega88PA__))
               ".space 12       \n\t"
#elif (defined (__AVR_ATmega168__) || defined (__AVR_ATmega168P__) || defined (__AVR_ATmega168A__) || defined (__AVR_ATmega168PA__) ||\
       defined (__AVR_ATmega328__) || defined (__AVR_ATmega328P__) || defined (__AVR_ATmega328A__) || defined (__AVR_ATmega328PA__))
               ".space 26       \n\t"
#elif (defined (__AVR_ATmega164__) || defined (__AVR_ATmega164P__) || defined (__AVR_ATmega164A__) || defined (__AVR_ATmega164PA__) || \
       defined (__AVR_ATmega324__) || defined (__AVR_ATmega324P__) || defined (__AVR_ATmega324A__) || defined (__AVR_ATmega324PA__) || \
       defined (__AVR_ATmega644__) || defined (__AVR_ATmega644P__) || defined (__AVR_ATmega644A__) || defined (__AVR_ATmega644PA__) || \
       defined (__AVR_ATmega1284__)|| defined (__AVR_ATmega1284P__))
               ".space 34       \n\t"
#else
#error unknown AVR - can not optimize. Please deactivate "SOFTPWM_INSANE_OPTIMIZATION"
#endif
               :
               :
       );
#else
/* A = {r2:r3}    B = {r4:r5}     TEMP = r6  */
/*                                           */
/* X --> B                                   */
/*       A --> X                             */
/*  since A is aligned and exactly 256byte   */
/*               r3 (hi(A)) can not change   */
/* ***************************************** */
/* SREG is not saved - only few ops allowed  */
/* !! WE MUST NOT USE SREG CHANGING ISNS !!  */
/* ***************************************** */
#if (defined (__AVR_mylegacy))
ISR(TIMER2_COMP_vect, ISR_NAKED) {
#else
ISR(TIMER2_COMPA_vect, ISR_NAKED) {
#endif
#endif
    asm volatile (
#ifdef __AVR_HAVE_MOVW__
        /* backup X register */
        "movw	r4      ,		r26                     \n\t" /* movw B  , X       --> 1 */

        /* prepare X pointer */
        "movw   r26     ,       r2                      \n\t" /* movw X  , A       --> 1 */
#else
# if (SOFTPWM_UPDATECYCLES < (32+3))
        /* without movw this is slower */
#       warning "SOFTPWM_UPDATECYCLES" might be too low
# endif
        /* backup X register */
        "mov    r4      ,       r26                     \n\t" /* mov  Blo  , Xlo   --> 1 */
        "mov    r5      ,       r27                     \n\t" /* mov  Bhi  , Xhi   --> 1 */

        /* prepare X pointer */
        "mov    r26     ,       r2                      \n\t" /* mov  Xlo  , Alo   --> 1 */
        "mov    r27     ,       r3                      \n\t" /* mov  Xhi  , Ahi   --> 1 */
#endif

        /* since we havn't restored X from Ahi, we use it as temp */
        "ld     r6      ,       X+                      \n\t" /* ldd tmp, X+       --> 2 */
        "out    %[portone] ,    r6                      \n\t" /* out PORTone, tmp  --> 1 */
        "ld     r6      ,       X+                      \n\t" /* ldd tmp, X+       --> 2 */
        "out    %[porttwo] ,    r6                      \n\t" /* out PORTtwo, tmp  --> 1 */
        "ld     r6      ,       X+                      \n\t" /* ldd tmp, X+       --> 2 */
        "out    %[portthree] ,  r6                      \n\t" /* out PORTthree, tmp--> 1 */
        "ld     r6      ,       X+                      \n\t" /* ldd tmp, X+       --> 2 */
        "out    %[portfour] ,   r6                      \n\t" /* out PORTfour, tmp --> 1 */

        /* update only r2 to delete possible overflow - since A is aligned and exactly 256byte */
        "mov    r2      ,       r26                     \n\t" /* mov Alo, Xlo      --> 1 */

        /* recover X register */
#ifdef __AVR_HAVE_MOVW__
        "movw	r26     ,		r4                      \n\t" /* movw X  , B       --> 1 */
#else
        "mov    r26     ,       r4                      \n\t" /* mov  Xlo  , Blo   --> 1 */
        "mov    r27     ,       r5                      \n\t" /* mov  Xhi  , Bhi   --> 1 */
#endif

        /* return from isr */
        "reti                                           \n\t" /*                   --> 5 */
        :
        : [portone]	 "i"	(_SFR_IO_ADDR(PORT_OF(DUMMY_PORTONE))),
          [porttwo]	 "i"	(_SFR_IO_ADDR(PORT_OF(DUMMY_PORTTWO))),
          [portthree] "i"	(_SFR_IO_ADDR(PORT_OF(DUMMY_PORTTHREE))),
          [portfour] "i"	(_SFR_IO_ADDR(PORT_OF(DUMMY_PORTFOUR))),
          [sreg]	 "i"	(_SFR_IO_ADDR(SREG))
    );
}
#if (SOFTPWM_INSANE_OPTIMIZATION)
void __startup(void) {
       /* since we optimized the fuck out of this, we need to implement now our own basic startup code */
       asm volatile (
               /* prepare zero-reg and initialize sreg */
               "clr __zero_reg__\n\t"
               "out %[sreg], __zero_reg__\n\t"
               /* initialize stack */
               "ldi r29, %[ramendhi]\n\t"
               "ldi r28, %[ramendlo]\n\t"
               "out %[sphreg], r29\n\t"
               "out %[splreg], r28\n\t"

               /* initialize all static variables */
               "rjmp __do_copy_data\n\t"
               :
               : [ramendhi]     "M"     (((RAMEND) >> 8) & 0xff),
                 [ramendlo]     "M"     (((RAMEND) >> 0) & 0xff),
                 [sphreg]       "i"	(_SFR_IO_ADDR(SPH)),
                 [splreg]       "i"	(_SFR_IO_ADDR(SPL)),
                 [sreg]	        "i"	(_SFR_IO_ADDR(SREG))
       );
}
#endif

void softpwm_configure(void *__thepwmseq) {
    asm volatile (
#ifdef __AVR_HAVE_MOVW__
        "movw r2, r26\n\t"
#else
        "mov  r2, r26\n\t"
        "mov  r3, r27\n\t"
#endif
        :
        : [tps] "x"	(__thepwmseq)
    );
}

void softpwm_disable__finishCycle_spinloop(void) {
    asm volatile (
        "ldi r16, 252\n\t"
        "mov __tmp_reg__, r16\n\t"
        "lds  r16, %[timsk]\n\t"
        "andi r16, %[ocie2val]\n\t"
        "softpwm_disable__finishCycle_spinloop_loopA%=:\n\t"
        "cp   r2, __tmp_reg__\n\t"
        "brne softpwm_disable__finishCycle_spinloop_loopA%=\n\t"
        //since here a magic countup must be happening due to pwm isr
        "softpwm_disable__finishCycle_spinloop_loopB%=:\n\t"
        "tst  r2\n\t"
        "brne softpwm_disable__finishCycle_spinloop_loopB%=\n\t"
        "sts %[timsk], r16\n\t"
        :
#if (defined (__AVR_mylegacy))
        : [timsk]		"i"      (_SFR_MEM_ADDR(TIMSK)),
          [ocie2val]    "M"      ((~(_BV(OCIE2))) & 0xff)
#else
        : [timsk]		"i"      (_SFR_MEM_ADDR(TIMSK2)),
          [ocie2val]    "M"      ((~(_BV(OCIE2A))) & 0xff)
#endif
        : "r16"
    );
}


void init_cpu(void) {
  cli();
  bootupreason=MCUBOOTREASONREG;
  MCUBOOTREASONREG=0;
  wdt_disable();
}

uint8_t getexponent(uint8_t value) {
    uint8_t result;
    for (result=0;result<6;result++) {
        if (value & 0x1) break;
        value>>=1;
    }
    return result;
}

void softpwm_setchannel(uint8_t *pwmseq_to_modify, uint8_t channel, uint8_t value) {
    uint8_t i,j,period,bithigh, bitlow;

    channel&=0x1f; //cut to 32channels
    bitlow=channel&0x7;
    bithigh=_BV(bitlow);
    bitlow=~(bithigh);
    channel>>=3;

    if (value < 64) {
#if (1)
        period=getexponent(value);
        value>>=period;
        period=64>>period;
#else
        period=64;
#endif
    } else {
        value=64;
        period=64;
    }

    i=0; j=0;
    do {
        if (j>=period) j=0;
        if (j<value) {
            // channel output high setzten
            pwmseq_to_modify[i+channel]|=bithigh;
        } else {
            // channel output low
            pwmseq_to_modify[i+channel]&=bitlow;
        }

        j++;
        i+=4;
    } while (i!=0);
}


int main(void) {
  uint8_t buffer=0;
  init_cpu();

  // YOUR CODE HERE:
  extfunc_initialize();
  EXTFUNC_callByName(cpucontext_initialize);
  EXTFUNC_callByName(hwclock_initialize);

  memset(pwmseq, 0, sizeof(pwmseq));
  memset(pwmseqalternative, 0, sizeof(pwmseqalternative));
  softpwm_configure(pwmseq);

  CFG_OUTPUT(LED_RIGHT);
  DDR_OF(DUMMY_PORTONE)  =0xff;
  DDR_OF(DUMMY_PORTTWO)  =0xff;
  DDR_OF(DUMMY_PORTTHREE)=0xff;
  DDR_OF(DUMMY_PORTFOUR) =0xff;

  softpwm_setchannel(pwmseq           , 0, 9);
  softpwm_setchannel(pwmseqalternative, 7, 17);

  sei();
  SOFTPWM_ENABLE();
  while (1) {
      int i;
      buffer=1-buffer; /* switch to double buffer */

      /* demonstrate slowing down normal processing due to ISR */
      for (i=0;i<16384;i++) asm volatile("nop");

      softpwm_disable__finishCycle_spinloop();
      SOFTPWM_CLEARPENDING();
      softpwm_configure(pwmframe[buffer]);
      SOFTPWM_ENABLE();
      TOGGLE(LED_RIGHT);
  }

  EXTFUNC_callByName(hwclock_finalize);
  EXTFUNC_callByName(cpucontext_finalize);
  extfunc_finalize();

//   bootloader_startup();
  return 0;
}
