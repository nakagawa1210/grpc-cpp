#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "msg_broker.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using msg::IdData;
using msg::Response;
using msg::RecvData;
using msg::SendData;


class SendClient {
 public:
  SendClient(std::shared_ptr<Channel> channel)
    : stub_(msg::Frame::NewStub(channel)) {}
  
  void CheckId() {
    IdData iddata;
    iddata.set_length(1);
    iddata.set_command(2);
    iddata.set_dest(3);
    iddata.set_msgid(4);
    
    Response response;
    ClientContext context;
    Status status = stub_->CheckId(&context, iddata, response);
    
    if(status.ok()) {
      int length = response.length();
      std::cout << "length=" << length << std::endl;
      std::cout << "command=" << response.command() << std::endl;
      std::cout << "dest=" << response.dest() << std::endl;
      std::cout << "msgid=" << response.msgid() << std::endl;      
      std::cout << "rescode=" << response.rescode() << std::endl;
    } else {
       std::cout << "GetFeature rpc failed." << std::endl;
    }
    
    return;
  }
};

int main(int argc, char** argv) {
  SendClient frame(
      grpc::CreateChannel("localhost:50051",
                          grpc::InsecureChannelCredentials()));

  std::cout << "--------------checkid --------------" << std::endl;
  frame.CheckId();

  return 0;
}
