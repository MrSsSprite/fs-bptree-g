#ifndef BPTR_UTILS_H
#define BPTR_UTILS_H

/*------------------------------ Public Macros -------------------------------*/
#define CEIL_DIV(num, dno) (((num) + (dno) - 1) / (dno))

#define _set_errno_(has_set_e, e_code) do \
{ if (!(has_set_e)) { bptr_errno = e_code; has_set_e = 1; } } while (0)
#define _set_errno(err_code) _set_errno_(has_set_err, err_code)

#define _set_err_code_(has_set_e, e_code_var, e_code) do \
{ if (!(has_set_e)) { (e_code_var) = (e_code); (has_set_e) = 1; } } while (0)
#define _set_err_code(e_code) _set_err_code_(has_set_err, err_code, e_code)
/*---------------------------- Public Macros END -----------------------------*/

#endif
