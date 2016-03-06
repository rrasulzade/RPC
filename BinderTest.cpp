
#include <iostream>
#include <iomanip> 
#include <string>
#include <vector>
#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "binder.h"

#include "debug.h"



using namespace std;



void Binder::printMap(){
  // debug("print");
  map<ProcSignature, list<ProcLocation> >::iterator map_it = sig_to_location.begin();
  for(; map_it != sig_to_location.end(); map_it++){
    cout << map_it->first.procInfo.proc_name <<  " Arglen: " <<  map_it->first.procInfo.argLen <<" -> " << endl;
    list<ProcLocation>:: iterator set_it = map_it->second.begin();
    for(; set_it != map_it->second.end(); set_it++){
      cout << "   "
         << set_it->locationInfo.s_id.addr.hostname << " "      
             << set_it->locationInfo.s_port << endl;
    }
    cout << endl;
  }
}


void Binder::printList(){
  list<ProcLocation>::iterator list_it = server_queue.begin();
  for(;list_it != server_queue.end(); list_it++){
    cout << "   "
       << list_it->locationInfo.s_id.addr.hostname << " "     
           << list_it->locationInfo.s_port << endl;
  }
}



int main(int argc, const char * argv[]) {
    Binder binder;
  	

  	proc_sig f;
  	memset(&f, 0, sizeof(proc_sig));
  	char nameF[] = "f";
  	strncpy(f.proc_name, nameF, MAX_PROC_NAME_SIZE);
  	f.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	f.argLen = 2;
  	f.argTypes = new int[f.argLen+1];

  	f.argTypes[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);           
    f.argTypes[1] = (1 << ARG_INPUT)  | (ARG_INT << 16) | 10;
    f.argTypes[2] = 0; 
          
    

  	proc_sig g;
  	memset(&g, 0, sizeof(proc_sig));
  	char nameG[] = "g";
  	strncpy(g.proc_name, nameG, MAX_PROC_NAME_SIZE);
  	g.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	g.argLen = 3;
  	g.argTypes = new int[g.argLen+1];

  	g.argTypes[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);
  	g.argTypes[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
  	g.argTypes[2] = (1 << ARG_INPUT) | (ARG_INT << 16);
  	g.argTypes[3] = 0;


  	proc_sig h;
  	memset(&h, 0, sizeof(proc_sig));
  	char nameH[] = "h";
  	strncpy(h.proc_name, nameH, MAX_PROC_NAME_SIZE);
  	h.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	h.argLen = 1;
  	h.argTypes = new int[h.argLen+1];

  	h.argTypes[0] = (1 << ARG_OUTPUT) | (1 << ARG_INPUT) | (ARG_LONG << 16) | 11;
  	h.argTypes[1] = 0;


  	proc_sig z;
  	memset(&z, 0, sizeof(proc_sig));
  	char nameZ[] = "z";
  	strncpy(z.proc_name, nameZ, MAX_PROC_NAME_SIZE);
  	z.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	z.argLen = 5;
  	z.argTypes = new int[z.argLen+1];

  	z.argTypes[0] = (1 << ARG_OUTPUT) | (ARG_LONG << 16) | 11;
  	z.argTypes[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
  	z.argTypes[2] =	(1 << ARG_INPUT) | (ARG_CHAR << 16); 
  	z.argTypes[3] =	(1 << ARG_INPUT) | (ARG_CHAR << 16) | 22;
  	z.argTypes[4] = 0;


  	///////////////////////////////   SERVERS  //////////////////////////////////


  	location serverA;
  	memset(&serverA, 0, sizeof(location));
  	serverA.s_port = 555;
  	serverA.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverA.s_id.addr.hostname, "serverA-002-RR", MAX_HOSTNAME_SIZE);
  	serverA.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	location serverB;
  	memset(&serverB, 0, sizeof(location));
  	serverB.s_port = 444;
  	serverB.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverB.s_id.addr.hostname, "serverB-002-RR", MAX_HOSTNAME_SIZE);
  	serverB.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	location serverC;
  	memset(&serverC, 0, sizeof(location));
  	serverC.s_port = 333;
  	serverC.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverC.s_id.addr.hostname, "serverC-002-RR", MAX_HOSTNAME_SIZE);
  	serverC.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';

  	location serverD;
  	memset(&serverD, 0, sizeof(location));
  	serverD.s_port = 222;
  	serverD.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverD.s_id.addr.hostname, "serverD-002-RR", MAX_HOSTNAME_SIZE);
  	serverD.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	location serverE;
  	memset(&serverE, 0, sizeof(location));
  	serverE.s_port = 111;
  	serverE.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverE.s_id.addr.hostname, "serverE-002-RR", MAX_HOSTNAME_SIZE);
  	serverE.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	location serverF;
  	memset(&serverF, 0, sizeof(location));
  	serverF.s_port = 999;
  	serverF.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverF.s_id.addr.hostname, "serverF-002-RR", MAX_HOSTNAME_SIZE);
  	serverF.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	///////////////////   F  //////////////////////////////////

  	int requestLen = sizeof(location) + sizeof(proc_sig);
  	char requestF1[requestLen+1];
  	memcpy(requestF1, &serverA, sizeof(location));
  	
  	memcpy(requestF1 + sizeof(location), f.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestF1 + sizeof(location) + MAX_PROC_NAME_SIZE, &f.argLen, sizeof(unsigned int));
  	memcpy(requestF1 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), f.argTypes, (f.argLen+1)*sizeof(int));
  	
  	requestF1[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestF1);


  	char requestF2[requestLen+1];
  	memcpy(requestF2, &serverD, sizeof(location));

  	memcpy(requestF2 + sizeof(location), f.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestF2 + sizeof(location) + MAX_PROC_NAME_SIZE, &f.argLen, sizeof(unsigned int));
  	memcpy(requestF2 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), f.argTypes, (f.argLen+1)*sizeof(int));
  	
  	requestF2[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestF2);



  	///////////////////   G  //////////////////////////////////

  	char requestG1[requestLen+1];
  	memcpy(requestG1, &serverA, sizeof(location));
  	
  	memcpy(requestG1 + sizeof(location), g.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestG1 + sizeof(location) + MAX_PROC_NAME_SIZE, &g.argLen, sizeof(unsigned int));
  	memcpy(requestG1 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), g.argTypes, (g.argLen+1)*sizeof(int));
  	requestG1[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestG1);


  	char requestG2[requestLen+1];
  	memcpy(requestG2, &serverB, sizeof(location));
  	
  	memcpy(requestG2 + sizeof(location), g.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestG2 + sizeof(location) + MAX_PROC_NAME_SIZE, &g.argLen, sizeof(unsigned int));
  	memcpy(requestG2 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), g.argTypes, (g.argLen+1)*sizeof(int));
  	requestG2[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestG2);


  	char requestG3[requestLen+1];
  	memcpy(requestG3, &serverD, sizeof(location));

  	memcpy(requestG3 + sizeof(location), g.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestG3 + sizeof(location) + MAX_PROC_NAME_SIZE, &g.argLen, sizeof(unsigned int));
  	memcpy(requestG3 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), g.argTypes, (g.argLen+1)*sizeof(int));
  	requestG3[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestG3);


  	///////////////////   H  //////////////////////////////////

  	char requestH1[requestLen+1];
  	memcpy(requestH1, &serverA, sizeof(location));

  	memcpy(requestH1 + sizeof(location), h.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestH1 + sizeof(location) + MAX_PROC_NAME_SIZE, &h.argLen, sizeof(unsigned int));
  	memcpy(requestH1 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), h.argTypes, (h.argLen+1)*sizeof(int));
  	requestH1[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestH1);


  	char requestH2[requestLen+1];
  	memcpy(requestH2, &serverB, sizeof(location));

  	memcpy(requestH2 + sizeof(location), h.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestH2 + sizeof(location) + MAX_PROC_NAME_SIZE, &h.argLen, sizeof(unsigned int));
  	memcpy(requestH2 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), h.argTypes, (h.argLen+1)*sizeof(int));
  	requestH2[requestLen] = '\0';

	binder.proc_registration(requestLen, requestH2);


	char requestH3[requestLen+1];
  	memcpy(requestH3, &serverC, sizeof(location));

  	memcpy(requestH3 + sizeof(location), h.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestH3 + sizeof(location) + MAX_PROC_NAME_SIZE, &h.argLen, sizeof(unsigned int));
  	memcpy(requestH3 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), h.argTypes, (h.argLen+1)*sizeof(int));
  	requestH3[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestH3);


  	///////////////////   Z  //////////////////////////////////

  	char requestZ1[requestLen+1];
  	memcpy(requestZ1, &serverE, sizeof(location));

  	memcpy(requestZ1 + sizeof(location), z.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestZ1 + sizeof(location) + MAX_PROC_NAME_SIZE, &z.argLen, sizeof(unsigned int));
  	memcpy(requestZ1 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), z.argTypes, (z.argLen+1)*sizeof(int));
  	requestZ1[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestZ1);






  	cout << "//////////////// REGISTRATION ////////////////\n" << endl;

  	binder.printMap();
  	cout << "//////////////// END ////////////////\n" << endl;



    /////////////////////////////////	LOCATION REQUEST 	/////////////////////////////////


  	requestLen = sizeof(proc_sig);
  	char requestGG1[requestLen+1];
  	memcpy(requestGG1, g.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestGG1 + MAX_PROC_NAME_SIZE, &g.argLen, sizeof(unsigned int));
  	memcpy(requestGG1 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), g.argTypes, (g.argLen+1)*sizeof(int));
  	requestGG1[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestGG1);


  	requestLen = sizeof(proc_sig);
  	char requestHH1[requestLen+1];
  	memcpy(requestHH1, h.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestHH1 + MAX_PROC_NAME_SIZE, &h.argLen, sizeof(unsigned int));
  	memcpy(requestHH1 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), h.argTypes, (h.argLen+1)*sizeof(int));
  	requestHH1[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestHH1);


  	requestLen = sizeof(proc_sig);
  	char requestZZ1[requestLen+1];
  	memcpy(requestZZ1, z.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestZZ1 + MAX_PROC_NAME_SIZE, &z.argLen, sizeof(unsigned int));
  	memcpy(requestZZ1 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), z.argTypes, (z.argLen+1)*sizeof(int));
  	requestZZ1[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestZZ1);


  	requestLen = sizeof(proc_sig);
  	char requestFF1[requestLen+1];
  	memcpy(requestFF1, f.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestFF1 + MAX_PROC_NAME_SIZE, &f.argLen, sizeof(unsigned int));
  	memcpy(requestFF1 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), f.argTypes, (f.argLen+1)*sizeof(int));
  	requestFF1[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestFF1);



 	///  register server F for proc Z
  	requestLen = sizeof(location) + sizeof(proc_sig);
  	char requestZ2[requestLen+1];
  	memcpy(requestZ2, &serverF, sizeof(location));

  	memcpy(requestZ2 + sizeof(location), z.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestZ2 + sizeof(location) + MAX_PROC_NAME_SIZE, &z.argLen, sizeof(unsigned int));
  	memcpy(requestZ2 + sizeof(location) + MAX_PROC_NAME_SIZE + sizeof(unsigned int), z.argTypes, (z.argLen+1)*sizeof(int));
  	requestZ2[requestLen] = '\0';

  	binder.proc_registration(requestLen, requestZ2);
  		binder.printMap();



  	requestLen = sizeof(proc_sig);
  	char requestZZ2[requestLen+1];
  	memcpy(requestZZ2, z.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestZZ2 + MAX_PROC_NAME_SIZE, &z.argLen, sizeof(unsigned int));
  	memcpy(requestZZ2 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), z.argTypes, (z.argLen+1)*sizeof(int));
  	requestZZ2[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestZZ2);

  		
  	requestLen = sizeof(proc_sig);	
  	char requestHH2[requestLen+1];
  	memcpy(requestHH2, h.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestHH2 + MAX_PROC_NAME_SIZE, &h.argLen, sizeof(unsigned int));
  	memcpy(requestHH2 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), h.argTypes, (h.argLen+1)*sizeof(int));
  	requestHH2[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestHH2);

  	

  	/// send request for proc Z
  	requestLen = sizeof(proc_sig);
  	char requestZZ3[requestLen+1];
  	memcpy(requestZZ3, z.proc_name, MAX_PROC_NAME_SIZE);
  	memcpy(requestZZ3 + MAX_PROC_NAME_SIZE, &z.argLen, sizeof(unsigned int));
  	memcpy(requestZZ3 + MAX_PROC_NAME_SIZE + sizeof(unsigned int), z.argTypes, (z.argLen+1)*sizeof(int));
  	requestZZ3[requestLen] = '\0';

  	binder.proc_location_request(requestLen, requestZZ3);


  	    delete [] f.argTypes;
    delete [] g.argTypes;
    delete [] h.argTypes;
    delete [] z.argTypes;
    return 0;
}
