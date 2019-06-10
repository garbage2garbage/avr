#ifndef _CCI_H_
#define _CCI_H_

/* include CCI manifest macros and attributes if CCI enabled.  */
#ifdef __CCI__ /* ifdef __CCI__ */

/* Manifest macros.  */
#define __XC__  1
#define __XC8__ 1

/* CCI attributes.  */ 
#define __persistent  __attribute__((persistent))
#define __at(addr)    __attribute__((at(addr)))
#define __interrupt(num) __attribute__((handler(num), used, externally_visible))
#define __align(n)  __attribute__((aligned(n)))
#define __pack  __attribute__((packed))
#define __deprecate  __attribute__((deprecated))
#define __section(name) __attribute__((section(name)))
#define asm(arg)  __asm(arg)

/* Unsupported CCI attributes by this compiler.  */
#define __far  __attribute__((unsupported("__far")))
#define __near  __attribute__((unsupported("__near")))
#define __xdata  __attribute__((unsupported("__xdata")))
#define __ydata  __attribute__((unsupported("__ydata")))
#define __eeprom  __attribute__((unsupported("__eeprom")))
#define __nonreentrant  __attribute__((unsupported("__nonreentrant")))
#define __reentrant  __attribute__((unsupported("__reentrant")))
#define __bank(num)  __attribute__((unsupported("__bank(n)")))

#endif  /* ifdef __CCI__ */

#endif  /* ifndef _CCI_H_ */

