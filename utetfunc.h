/* utetfunc.h */
#ifndef UTETFUNC_H_HEADER____
#define UTETFUNC_H_HEADER____

#include <Windows.h>

int win_rnd();

#define UT_ASSERT(e)
#define UT_RND()            win_rnd()
#define UT_RND_INIT()
#define UT_SLEEP_MS(ms)     Sleep(ms)

/* Define if rendering not required */
/* #define NORENDER */

/*
    Define when C libraries not used,
    but compiler complains about
	intrinsic memset being undefined.
*/
#define NOCLIB

#endif /* UTETFUNC_H_HEADER____ */
