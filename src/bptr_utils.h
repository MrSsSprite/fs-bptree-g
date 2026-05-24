#ifndef BPTR_UTILS_H
#define BPTR_UTILS_H

/*------------------------------ Public Macros -------------------------------*/
#define CEIL_DIV(num, dno) (((num) + (dno) - 1) / (dno))

#define _bptr_set_errno(err_code) \
   if (!err_set) { bptr_errno = err_code; err_set = 1; }
/*---------------------------- Public Macros END -----------------------------*/

#endif
