extern int errcode;
extern char errmsg[256];

int ipbench_error(int, char*);

/* total hack alert : these values copied because
   swig doesn't really provide them in any header 
   files

#define  SWIG_MemoryError    1
#define  SWIG_IOError        2
#define  SWIG_RuntimeError   3
#define  SWIG_IndexError     4
#define  SWIG_TypeError      5
#define  SWIG_DivisionByZero 6
#define  SWIG_OverflowError  7
#define  SWIG_SyntaxError    8
#define  SWIG_ValueError     9
#define  SWIG_SystemError   10
#define  SWIG_UnknownError  99
*/

#define  ipbench_MemoryError    1
#define  ipbench_IOError        2
#define  ipbench_RuntimeError   3
#define  ipbench_IndexError     4
#define  ipbench_TypeError      5
#define  ipbench_DivisionByZero 6
#define  ipbench_OverflowError  7
#define  ipbench_SyntaxError    8
#define  ipbench_ValueError     9
#define  ipbench_SystemError   10
#define  ipbench_UnknownError  99
