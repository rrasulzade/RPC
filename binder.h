#ifndef		__BINDER_H__
#define		__BINDER_H__

#include <map>
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
	location locationInfo;
	int socketFD;
	friend bool operator< (const ProcLocation& lhs, const ProcLocation& rhs);
	friend bool operator==(const ProcLocation& lhs, const ProcLocation& rhs);	
};



class Binder{
	int binder_sockFD;
	std::map<ProcSignature, std::list<ProcLocation> > sig_to_location;
	std::list<ProcLocation> server_queue;
	std::vector<int> server_sockets;

  public:
  	Binder();
  	~Binder();

  	// method declarions
  	void start();
  	void setup_socket();
  	int handle_message(int sockFD);
    int sendLOC_SUCC(int sockFD, location loc);
    int sendResult(int sockFD, int type, int retCode);
    void proc_registration(int sockFD, char * message);
    void proc_location_request(int sockFD, char * message);
  	ProcLocation roundRobinServer(std::list<ProcLocation>& loc_set);
  	void addToServerQueue(std::list<ProcLocation>& loc_set);
  	void addToServerQueue(ProcLocation& loc);
  	int terminateServers();
  	int getServerIndex(int socketFD);
	void checkServers(int socketFD);


  	void printList();
  	void printMap();

};



#endif