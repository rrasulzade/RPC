#ifndef		__DEBUG_H__
#define		__DEBUG_H__

#include <stdio.h>
/*
#define	DEBUGALL
#define	DEBUGERROR
#define	DEBUGWARNING

*/



#define __INFO(M, ...)     fprintf(stderr, "[\033[1;32mDEBUG\033[0m] (%s() at %s:%d) " M "\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__)
#define __ERROR(M, ...)    fprintf(stderr, "[\033[1;31mERROR\033[0m] (%s() at %s:%d) " M "\n\r", __func__, __FILE__, __LINE__, ##__VA_ARGS__)
#define __WARNING(M, ...)  fprintf(stderr, "[\033[1;35mWARNING\033[0m] (%s() at %s:%d) " M "\n\r", __func__, __FILE__, __LINE__, ##__VA_ARGS__)



#ifdef DEBUGALL
	#define DEBUG(M, ...) 	__INFO(M, ##__VA_ARGS__)
#else
	#define DEBUG(M, ...)
#endif


#ifdef DEBUGERROR
	#define ERROR(M, ...) 	__ERROR(M, ##__VA_ARGS__)
#else
	#define ERROR(M, ...)
#endif


#ifdef DEBUGWARNING
	#define WARNING(M, ...) __WARNING(M, ##__VA_ARGS__)
#else
	#define WARNING(M, ...)
#endif






#endif
