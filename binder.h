#ifndef		__BINDER_H__
#define		__BINDER_H__

#include <map>
#include <set>
#include <queue>
#include "defs.h"

#define 	TERMINATE_ALL		-500
	

class Binder{
	int binder_sockFD;
	std::map<proc_sig, std::set<location> > proc_to_location;
	std::queue<location> server_queue;
	
  public:
  	Binder();
  	~Binder();

  	// method declarions
  	void start();
  	void create_binder();
  	int handle_message(int sockFD);
  	void proc_registration(int msg_len, char * message);
  	void terminateServers();
};



#endif