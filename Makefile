XCC			=	gcc
AR			=	ar
ARFLAGS		=	rcs
CFLAGS		=	-c -Wall -g
CXX			=	g++
CXXFLAGS	=	-Wall -MMD -g



MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}}	# makefile name

RPCSRC		=	rpc.c
RPCOBJECT	=	rpc.o
RPCLIB		=	librpc.a

BINDERSRC	=	binder.cpp
BINDEROBJ	=	binder.o
BINDEREXEC	=	binder

EXECS		=	${BINDEREXEC}
OBJECTS		=	${RPCOBJECT} ${BINDEROBJ}
LIBS		=	${RPCLIB}




DEPENDS = ${OBJECTS:.o=.d}


# all targets 
all	:	${LIBS} ${EXECS}


${RPCLIB} : ${RPCOBJECT}
	${AR} ${ARFLAGS} ${RPCLIB} ${RPCOBJECT}

${RPCOBJECT} : ${RPCSRC}
	${XCC} ${CFLAGS} ${RPCSRC} -o ${RPCOBJECT}

${BINDEREXEC} : ${BINDERSRC}
	${CXX} ${CXXFLAGS} $^ -o $@


${OBJECTS} : ${MAKEFILE_NAME}					# OPTIONAL : changes to this file => recompile


-include ${DEPENDS}


.PHONY: clean

clean:
	rm -f ${OBJECTS} ${LIBS} ${EXECS} ${DEPENDS}

