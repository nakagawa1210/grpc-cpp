#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
//#include "helper.h"
#include "msg_broker.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using msg::IdData;
using msg::Response;
using msg::RecvData;
using msg::SendData;

class MsgBrokerImpl final : public msg::Frame::Service {
 public:
  Status CheckId(ServerContext* context, const IdData* iddata,
                 Response* response) override {
    response->set_length(iddata->length());
    response->set_command(iddata->command());
    response->set_dest(iddata->dest());
    response->set_msgid(iddata->msgid());
    response->set_rescode(5);
    return Status::OK;
  }
  /*
Status RecordRoute(ServerContext* context, ServerReader<Point>* reader,
                     RouteSummary* summary) override {
    Point point;
    int point_count = 0;
    int feature_count = 0;
    float distance = 0.0;
    Point previous;

    system_clock::time_point start_time = system_clock::now();
    while (reader->Read(&point)) {
      point_count++;
      if (!GetFeatureName(point, feature_list_).empty()) {
        feature_count++;
      }
      if (point_count != 1) {
        distance += GetDistance(previous, point);
      }
      previous = point;
    }
    system_clock::time_point end_time = system_clock::now();
    summary->set_point_count(point_count);
    summary->set_feature_count(feature_count);
    summary->set_distance(static_cast<long>(distance));
    auto secs =
        std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    summary->set_elapsed_time(secs.count());

    return Status::OK;
  }


   private:
  std::vector<Feature> feature_list_;
  std::mutex mu_;
  std::vector<RouteNote> received_notes_;
*/
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  //builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}


int main(int argc, char** argv) {
  RunServer();

  return 0;
}
