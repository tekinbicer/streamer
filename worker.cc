//   Request worker in C++

#include "zhelpers.hpp"
#include "mpi.h"
#include <string>

#define ADDR_PREFIX "tcp://"

int main (int argc, char *argv[])
{
  if(argc!=3) {
    printf("Usage: %s <dest-ip-address=164.54.143.3> <dest-port=5560>\n", argv[0]);
    exit(0);
  }

  // Setup MPI
  int my_rank, world_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  std::string server_addr(ADDR_PREFIX);
  int dest_port = atoi(argv[2]);
  std::stringstream addr_stream;
  addr_stream << server_addr << argv[1] << ":" << (dest_port+my_rank);
  std::cout << "Destination address=" << addr_stream.str() << std::endl;

  zmq::context_t context(1);

  zmq::socket_t server(context, ZMQ_REQ);
  server.connect(addr_stream.str());

  // Introduce yourself
  s_send(server, std::to_string((long long)my_rank));

  // Check if server has any projection
  std::string msg = s_recv(server);
  std::cout << "Server replied=" << msg << std::endl;

  // Say you received it
  msg = std::string("[" + std::to_string((long long)my_rank) + "]: I received the projection");
  s_send(server, msg);
  
  MPI_Finalize();
}

