# RPC - Remote Procedure Call - Distributed Systems project
Binder is implemented in C++ and compiled with "g++" 
Rpc is implemented in C and compiled with "gcc" 

The makefile is written for the GNU Make 3.81 (the version on the student server).

First of all run 'make' to compile binder.cc and generate RPC library.
Also, 'make binder' compiles binder.cpp separately and to complie the library, type only 'make librpc.a'.

Now, run ./binder, and copy BINDER_ADDRESS and BINDER_PORT which is used in other 
machines to connecting to the binder.

To compile client use:
gcc -L. -std=c99 client1.c -lrpc -pthread -o client
then run it with: ./client

To compile server use:
gcc -L. server_functions.c server_function_skels.c server.c -lrpc -lpthread -o server
then run it with: ./server

Additionally, pthread library is used for RPC library.
NUM_THREADS is the number of worker threads that handle requests from clients.
It is defined in the makefile and currently set to 50. If there is a need for more workers
because there are more clients, the user need to change that value to the desired one (> 0)
and recompile the library.
