//   Request-reply client in C++
//   Connects REQ socket to tcp://localhost:5559
//   Sends "Hello" to server, expects "World" back
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>


#include "zhelpers.hpp"
#include <vector>

#define N_WORKERS 32
#define S_PORT 5560

int main (int argc, char *argv[])
{
  zmq::context_t context(1);

  std::vector<zmq::socket_t> socket_reqs;

  // Check workers
  int nworkers=N_WORKERS;
  int starting_port=S_PORT;
  for(int i=0; i<nworkers; ++i)
    std::stringstream addr_stream;
    addr_str_stream << "tcp://localhost:" << starting_port++;
    zmq::socket_t requester(context, ZMQ_REQ);
    requester.connect(add_str_stream);
    s_send(requester, "Hello");
    socket_reqs.push_back(requester(context, ZMQ_REQ));
  }

  // Get worker's rank and put in hashmap
  std::unordered_map<int, zmq::socket_t> workers;
  for(auto socket_req : socket_reqs){
    std::string str = s_recv(socket_req);
    std::cout << str << " is alive." << std::endl;
    // rank=1 -> 1; rank=2 -> 2; ...
    workers.insert(std::stoi(std), socket_req);
  }


  // At this point I know every one is alive
  // Send msg/projections
  for(auto w : workers){
    std::string msg = "hi " + std::to_string(w.key());
    s_send(msg);
  }

  // Check if they received it
  for(auto w : workers){
    std::string msg = s_recv(w);
    std::cout << "w message receieved: " << msg << std::endl;
  }
}
