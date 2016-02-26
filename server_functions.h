/*
 * server_functions.h
 * 
 * This file defines the server functions that will be called by the client.
 *
 * note: "IN" means the argument is an "input to" the server.
         "OUT" means the argument is an "output from" the server.
 *       "INOUT" means the argument is an "input to" and "output from" the server.
 *       There can be only one OUTPUT parameter per function, 
 *       and if there is one it goes in args[0].
 */


int f0(int a, int b);                    /* return: OUT; a, b: IN */

long f1(char a, short b, int c, long d); /* returns: OUT; a, b, c, d: IN */

char* f2(float a, double b);             /* return: OUT string; a, b: IN */
				     
void f3(long a[]);                       /* a: INOUT array */

void f4(char a[]);                       /* a: IN array */
