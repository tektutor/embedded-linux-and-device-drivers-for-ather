/* pru_blink.c - a tight, deterministic loop on the PRU, independent of Linux.
 * Reference starter code (PRU C compiler). Confirm headers and the output
 * pin bit against your PRU SDK and the pin you route to the PRU.
 */
#include <stdint.h>
#include <pru_cfg.h>

volatile register uint32_t __R30;   /* PRU output register (drives pins) */

void main(void) {
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
    while (1) {
        __R30 |=  (1 << 5);          /* drive the PRU output pin high */
        __delay_cycles(100000000);   /* exact cycle count = exact timing */
        __R30 &= ~(1 << 5);          /* pin low */
        __delay_cycles(100000000);
    }
}
