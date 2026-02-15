#ifndef _GPIOLIB_H_
#define _GPIOLIB_H_
/** ************************************************************************************************
 *	GPIO
 *
 ** ************************************************************************************************
**/

void gpiolib_input_init(void (*f)(int), UBaseType_t uxPriority);
void gpiolib_output_init(void);

#endif
// END OF FILE