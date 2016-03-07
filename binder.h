#ifndef		__BINDER_H__
#define		__BINDER_H__

#include <map>
#include <list>
#include <exception>

#include "defs.h"

#define 	TERMINATE_ALL		-500


#define ARG_CHAR    1
#define ARG_SHORT   2
#define ARG_INT     3
#define ARG_LONG    4
#define ARG_DOUBLE  5
#define ARG_FLOAT   6

#define ARG_INPUT   31
#define ARG_OUTPUT  30




struct failure : public std::exception {
  const char* what() const throw() {return "FAILURE!";}
};

struct ProcSignature{
	proc_sig procInfo;

	friend bool operator< (const ProcSignature& lhs, const ProcSignature& rhs);
	friend bool operator==(const ProcSignature& lhs, const ProcSignature& rhs);
};


struct ProcLocation{
	location locationInfo;

	friend bool operator< (const ProcLocation& lhs, const ProcLocation& rhs);
	friend bool operator==(const ProcLocation& lhs, const ProcLocation& rhs);	
};



class Binder{
	int binder_sockFD;
	std::map<ProcSignature, std::list<ProcLocation> > sig_to_location;
	std::list<ProcLocation> server_queue;
	unsigned int next_prior_id;

  public:
  	Binder();
  	~Binder();

  	// method declarions
  	void start();
  	void setup_socket();
  	int handle_message(int sockFD);
    int sendLOC_SUCC(int sockFD, msg_type type, ProcLocation* loc);
    int sendResult(int sockFD, msg_type type, int retCode);
    void proc_registration(int sockFD, char * message);
    void proc_location_request(int sockFD, char * message);
  	ProcLocation roundRobinServer(std::list<ProcLocation>& loc_set);
  	void addToServerQueue(std::list<ProcLocation>& loc_set);
  	void addToServerQueue(ProcLocation& loc);
  	int terminateServers();


  	void printList();
  	void printMap();

};



#endif