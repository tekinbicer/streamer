//   Request-reply service in C++
//   Connects REP socket to tcp://localhost:5560
//   Expects "Hello" from client, replies with "World"
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>


#include "zhelpers.hpp"

#define REP_ADDR "tcp://164.54.143.3"
#define S_PORT 5560

int main (int argc, char *argv[])
{
  // Setup MPI
  int my_rank, world_size;
  MPI_Init(argc, argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  zmq::context_t context(1);

  std::stringstream addr_stream;
  std::string rep_addr(REP_ADDR);
  int starting_port = S_PORT;
  addr_str_stream << rep_addr << (starting_port+my_rank);
  zmq::socket_t socket_rep(context, ZMQ_REP);
  socket_rep.connect(addr_str_stream);


  // Wait for the initial greeting
  std::string msg_rep = s_recv (responder);
  std::cout << "Client says: " << msg_rep << std::endl;

  // Introduce yourself
  std::string msg = std::to_string(my_rank);
  s_send(socket_rep, msg);

  // Check if server has any projection
  msg_rep = s_recv(socket_rep);
  std::cout << "Rep socket msg=" << msg_rep << std::endl;

  // Say you received it
  msg = std::string(std::to_string(my_rank) + "I received it");
  s_send(socket_rep, msg);
  
  MPI_Finalize();
}

