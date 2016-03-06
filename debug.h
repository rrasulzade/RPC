#ifndef		__DEBUG_H__
#define		__DEBUG_H__

#include <stdio.h>


#define 	debug(msg, ...) 		printf("DEBUG %s \n", msg)


#define	DEBUGALL


#define __INFO(M, ...) fprintf(stderr, "[\033[1;32mDEBUG\033[0m] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#ifdef DEBUGALL
	#define DEBUG(M, ...) __INFO(M, ##__VA_ARGS__)
#else
	#define DEBUG(M, ...)
#endif





#endif
