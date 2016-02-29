#ifndef		__BINDER_H__
#define		__BINDER_H__

#include <map>
#include <set>
#include <queue>
#include "defs.h"

#define 	TERMINATE_ALL		-500
	

class Binder{
	std::map<proc_sig, std::set<location> > proc_to_location;
	std::queue<location> server_queue;

  public:
  	Binder();
  	~Binder();
  	
  	// public variables
  	int binder_sockFD;

  	// method declarions
  	void create_binder();
  	int handle_message(int sockFD);
  	void terminateAllServers();
};



#endif