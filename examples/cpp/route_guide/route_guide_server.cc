/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <time.h>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include "helper.h"
#ifdef BAZEL_BUILD
#include "examples/protos/route_guide.grpc.pb.h"
#else
#include "route_guide.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using routeguide::Feature;
using routeguide::Point;
using routeguide::Rectangle;
using routeguide::RouteGuide;
using routeguide::RouteNote;
using routeguide::RouteSummary;
using routeguide::SendData;
using routeguide::Response;
using routeguide::RecvData;
using routeguide::IdData;

using std::chrono::system_clock;

std::queue<SendData> data_queue_;
float ConvertToRadians(float num) { return num * 3.1415926 / 180; }

// The formula is based on http://mathforum.org/library/drmath/view/51879.html
float GetDistance(const Point& start, const Point& end) {
  const float kCoordFactor = 10000000.0;
  float lat_1 = start.latitude() / kCoordFactor;
  float lat_2 = end.latitude() / kCoordFactor;
  float lon_1 = start.longitude() / kCoordFactor;
  float lon_2 = end.longitude() / kCoordFactor;
  float lat_rad_1 = ConvertToRadians(lat_1);
  float lat_rad_2 = ConvertToRadians(lat_2);
  float delta_lat_rad = ConvertToRadians(lat_2 - lat_1);
  float delta_lon_rad = ConvertToRadians(lon_2 - lon_1);

  float a = pow(sin(delta_lat_rad / 2), 2) +
            cos(lat_rad_1) * cos(lat_rad_2) * pow(sin(delta_lon_rad / 2), 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  int R = 6371000;  // metres

  return R * c;
}

std::string GetFeatureName(const Point& point,
                           const std::vector<Feature>& feature_list) {
  for (const Feature& f : feature_list) {
    if (f.location().latitude() == point.latitude() &&
        f.location().longitude() == point.longitude()) {
      return f.name();
    }
  }
  return "";
}

class RouteGuideImpl final : public RouteGuide::Service {
 public:
  explicit RouteGuideImpl(const std::string& db) {
    routeguide::ParseDb(db, &feature_list_);
  }

  Status GetFeature(ServerContext* context, const Point* point,
                    Feature* feature) override {
    feature->set_name(GetFeatureName(*point, feature_list_));
    feature->mutable_location()->CopyFrom(*point);
    return Status::OK;
  }

  Status ListFeatures(ServerContext* context,
                      const routeguide::IdData* iddata,
                      ServerWriter<RecvData>* writer) override {
    SendData qdata;
    RecvData recvdata;
    struct timespec time;
    double time_l,time_u,lg_time;
    
    while (data_queue_.empty()){
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (!data_queue_.empty()) {
      qdata = data_queue_.front();
      
      recvdata.set_length(qdata.length());
      recvdata.set_command(qdata.command());
      recvdata.set_dest(qdata.dest());
      recvdata.set_msgid(9);
      recvdata.set_message(qdata.message());
      recvdata.set_t_1(qdata.t_1());
      recvdata.set_t_2(qdata.t_2());
      recvdata.set_t_4(qdata.t_4());
      
      clock_gettime(CLOCK_MONOTONIC,&time);
      time_u = time.tv_sec;
      time_l = time.tv_nsec;
      time_l = time_l / 1000000000;      
      lg_time = time_u + time_l;      
      recvdata.set_t_3(lg_time);      
      
      writer->Write(recvdata);

      data_queue_.pop();
    }
    return Status::OK;
  }

  Status RecordRoute(ServerContext* context, ServerReader<SendData>* reader,
                     Response* response) override {
    SendData senddata;
    struct timespec time;
    double time_l,time_u,lg_time;    
 
    while (reader->Read(&senddata)) {
      clock_gettime(CLOCK_MONOTONIC,&time);
      time_u = time.tv_sec;
      time_l = time.tv_nsec;
      time_l = time_l / 1000000000;
      lg_time = time_u + time_l;      
      senddata.set_t_2(lg_time);
      data_queue_.push(senddata);
    }
    //system_clock::time_point end_time = system_clock::now();
    response->set_length(senddata.length());
    response->set_command(senddata.command());
    response->set_dest(senddata.dest());
    response->set_msgid(4);
    response->set_rescode(5);
    
    //auto secs =
    //    std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    //summary->set_elapsed_time(secs.count());

    return Status::OK;
  }

  Status RouteChat(ServerContext* context,
                   ServerReaderWriter<RouteNote, RouteNote>* stream) override {
    RouteNote note;
    while (stream->Read(&note)) {
      std::unique_lock<std::mutex> lock(mu_);
      for (const RouteNote& n : received_notes_) {
        if (n.location().latitude() == note.location().latitude() &&
            n.location().longitude() == note.location().longitude()) {
          stream->Write(n);
        }
      }
      received_notes_.push_back(note);
    }

    return Status::OK;
  }

 private:
  std::vector<Feature> feature_list_;
  std::vector<RecvData> recvdata_list_;
  std::mutex mu_;
  std::vector<RouteNote> received_notes_;
};

void RunServer(const std::string& db_path) {
  std::string server_address("0.0.0.0:50051");
  RouteGuideImpl service(db_path);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());

  server->Wait();
}

int main(int argc, char** argv) {
  // Expect only arg: --db_path=path/to/route_guide_db.json.
  std::string db = routeguide::GetDbFileContent(argc, argv);
  RunServer(db);

  return 0;
}
