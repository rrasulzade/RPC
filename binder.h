#ifndef		__BINDER_H__
#define		__BINDER_H__

#include <map>
#include <set>
#include <queue>
#include "defs.h"

#define 	TERMINATE_ALL		-500


struct ProcSignature{
	proc_sig procInfo;

	friend bool operator< (const ProcSignature& lhs, const ProcSignature& rhs);
	friend bool operator==(const ProcSignature& lhs, const ProcSignature& rhs);
};


struct ProcLocation{
	unsigned long prior_id;
	location locationInfo;

	friend bool operator< (const ProcLocation& lhs, const ProcLocation& rhs);
	friend bool operator==(const ProcLocation& lhs, const ProcLocation& rhs);	
};







class Binder{
	int binder_sockFD;
	std::map<ProcSignature, std::set<ProcLocation> > sig_to_location;
	std::queue<ProcLocation> server_queue;
	unsigned int next_prior_id;

  public:
  	Binder();
  	~Binder();

  	// method declarions
  	void start();
  	void setup_socket();
  	int handle_message(int sockFD);
  	void proc_registration(int msg_len, char * message);
  	void terminateServers();

};



#endif