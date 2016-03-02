XCC			=	gcc
AR			=	ar
ARFLAGS		=	rcs
CFLAGS		=	-c -Wall -g

MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}}	# makefile name

RPCSRC		=	rpc.c
RPCOBJECT	=	rpc.o
RPCLIB		=	librpc.a


OBJECTS		=	${RPCOBJECT}
LIBS		=	${RPCLIB}




#DEPENDS = ${OBJECTS:.o=.d}


# all targets 
all	:	${LIBS}


${RPCLIB} : ${RPCOBJECT}
	${AR} ${ARFLAGS} ${RPCLIB} ${RPCOBJECT}

${RPCOBJECT} : ${RPCSRC}
	${XCC} ${CFLAGS} ${RPCSRC} -o ${RPCOBJECT}


${OBJECTS} : ${MAKEFILE_NAME}					# OPTIONAL : changes to this file => recompile


#-include ${DEPENDS}


.PHONY: clean

clean:
	rm -f ${OBJECTS} ${LIBS} #${DEPENDS}

