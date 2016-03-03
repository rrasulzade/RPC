#ifndef		__BINDER_H__
#define		__BINDER_H__

#include <map>
#include <set>
#include <list>
#include <exception>

#include "defs.h"

#define 	TERMINATE_ALL		-500


struct failure : public std::exception {
  const char* what() const throw() {return "FAILURE!";}
};

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
	std::list<ProcLocation> server_queue;
	unsigned int next_prior_id;

  public:
  	Binder();
  	~Binder();

  	// method declarions
  	void start();
  	void setup_socket();
  	int handle_message(int sockFD);
  	void proc_registration(int msg_len, char * message);
  	void proc_location_request(int msg_len, char * message);
  	ProcLocation roundRobinServer(std::set<ProcLocation>& loc_set);
  	void addToServerQueue(std::set<ProcLocation>& loc_set);
  	void terminateServers();


  	void printMap();

};



#endif