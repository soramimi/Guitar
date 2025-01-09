/* vim:set ts=8 sts=4 sw=4 tw=0: */

#if defined(_MSC_VER) && !defined(ANSI_DEBUG)
# include <crtdbg.h>
#else
# include <assert.h>
# ifdef _DEBUG
#  define _ASSERT(x)			assert(x)
#  define _CRT_WARN			stderr
#  define _RPT0(f,s)			fprintf(f,s)
#  define _RPT1(f,s,a1)			fprintf(f,s,a1)
#  define _RPT2(f,s,a1,a2)		fprintf(f,s,a1,a2)
#  define _RPT3(f,s,a1,a2,a3)		fprintf(f,s,a1,a2,a3)
#  define _RPT4(f,s,a1,a2,a3,a4)	fprintf(f,s,a1,a2,a3,a4)
# else
#  define _ASSERT(x)
#  define _RPT0(f,s)
#  define _RPT1(f,s,a1)
#  define _RPT2(f,s,a1,a2)
#  define _RPT3(f,s,a1,a2,a3)
#  define _RPT4(f,s,a1,a2,a3,a4)
# endif
#endif
