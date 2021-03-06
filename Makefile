XCC			=	gcc
AR			=	ar
ARFLAGS		=	rcs
CFLAGS		=	-c -Wall -g -std=c99 -pthread -DNUM_THREADS=50
CXX			=	g++
CXXFLAGS	=	-Wall -MMD -g

MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}}	# makefile name

# RPC related
RPCSRC 		= 	rpc.c int_queue.c procedure_list.c
RPCOBJECTS	=	$(patsubst %.c,%.o,$(RPCSRC))
RPCLIB		=	librpc.a

# BINDER related
BINDERSRC	=	binder.cpp
BINDEROBJ	=	binder.o
BINDEREXEC	=	binder

# ALL
EXECS		=	${BINDEREXEC}
OBJECTS		=	${RPCOBJECTS} ${BINDEROBJ}
LIBS		=	${RPCLIB}

DEPENDS = ${OBJECTS:.o=.d}

# all targets 
all	:	${LIBS} ${EXECS}



${RPCLIB} : ${RPCOBJECTS}
	${AR} ${ARFLAGS} ${RPCLIB} ${RPCOBJECTS}


${BINDEREXEC} : ${BINDERSRC}
	${CXX} ${CXXFLAGS} binder.cpp -o $@



${OBJECTS} : ${MAKEFILE_NAME}					# OPTIONAL : changes to this file => recompile


-include ${DEPENDS}


.PHONY: clean

clean:
	rm -f *.d *.o *.a ${EXECS}

