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
#include "helper.h"
#ifdef BAZEL_BUILD
#include "examples/protos/route_guide.grpc.pb.h"
#else
#include "route_guide.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
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

Point MakePoint(long latitude, long longitude) {
  Point p;
  p.set_latitude(latitude);
  p.set_longitude(longitude);
  return p;
}

Feature MakeFeature(const std::string& name, long latitude, long longitude) {
  Feature f;
  f.set_name(name);
  f.mutable_location()->CopyFrom(MakePoint(latitude, longitude));
  return f;
}

RouteNote MakeRouteNote(const std::string& message, long latitude,
                        long longitude) {
  RouteNote n;
  n.set_message(message);
  n.mutable_location()->CopyFrom(MakePoint(latitude, longitude));
  return n;
}

class RouteGuideClient {
 public:
  RouteGuideClient(std::shared_ptr<Channel> channel)
      : stub_(RouteGuide::NewStub(channel)) {}

  void GetFeature() {
    Point point;
    Feature feature;
    point = MakePoint(409146138, -746188906);
    GetOneFeature(point, &feature);
    point = MakePoint(0, 0);
    GetOneFeature(point, &feature);
  }

  void ListFeatures() {
    routeguide::IdData iddata;
    RecvData recvdata;
    ClientContext context;

    iddata.set_length(1);
    iddata.set_command(2);
    iddata.set_dest(3);
    iddata.set_msgid(4);
    
    std::unique_ptr<ClientReader<RecvData> > reader(
        stub_->ListFeatures(&context, iddata));
    
    while (reader->Read(&recvdata)) {
      std::cout << "Dest =  " << recvdata.dest() << std::endl;
    }
    Status status = reader->Finish();
    if (status.ok()) {
      std::cout << "ListFeatures rpc succeeded." << std::endl;
    } else {
      std::cout << "ListFeatures rpc failed." << std::endl;
    }
  }

  void RecordRoute(int ws) {
    SendData senddata;
    Response stats;
    ClientContext context;
    const int kPoints = 100000;
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

    //std::default_random_engine generator(seed);
    //std::uniform_int_distribution<int> feature_distribution(
    //   0, feature_list_.size() - 1);
    //std::uniform_int_distribution<int> delay_distribution(500, 1500);

    std::unique_ptr<ClientWriter<SendData>> writer(
        stub_->RecordRoute(&context, &stats));
    
    for (int i = 0; i < ws; i++) {
      SendData senddata;
      senddata.set_length(1);
      senddata.set_command(2);
      senddata.set_dest(i);     
      senddata.set_message("5");
      senddata.set_t_1(6);
      senddata.set_t_2(7);
      senddata.set_t_3(8);
      senddata.set_t_4(9);
      //std::cout << i << std::endl;
      
    //std::cout << "Visiting point " << f.location().latitude() / kCoordFactor_
    //            << ", " << f.location().longitude() / kCoordFactor_
    //            << std::endl;
      if (!writer->Write(senddata)) {
        // Broken stream.
        break;
      }
      //  std::this_thread::sleep_for(
      //    std::chrono::milliseconds(delay_distribution(generator)));
    }
    writer->WritesDone();
    Status status = writer->Finish();
    if (status.ok()) {
      std::cout << "Finished trip with " << stats.length() << " points\n"
                << "Passed " << stats.command() << " features\n"
                << "Travelled " << stats.dest() << " meters\n"
                << "It took " << stats.msgid() << " seconds"
                << std::endl;
    } else {
      std::cout << "RecordRoute rpc failed." << std::endl;
    }
  }

  void RouteChat() {
    ClientContext context;

    std::shared_ptr<ClientReaderWriter<RouteNote, RouteNote> > stream(
        stub_->RouteChat(&context));

    std::thread writer([stream]() {
      std::vector<RouteNote> notes{MakeRouteNote("First message", 0, 0),
                                   MakeRouteNote("Second message", 0, 1),
                                   MakeRouteNote("Third message", 1, 0),
                                   MakeRouteNote("Fourth message", 0, 0)};
      for (const RouteNote& note : notes) {
        std::cout << "Sending message " << note.message() << " at "
                  << note.location().latitude() << ", "
                  << note.location().longitude() << std::endl;
        stream->Write(note);
      }
      stream->WritesDone();
    });

    RouteNote server_note;
    while (stream->Read(&server_note)) {
      std::cout << "Got message " << server_note.message() << " at "
                << server_note.location().latitude() << ", "
                << server_note.location().longitude() << std::endl;
    }
    writer.join();
    Status status = stream->Finish();
    if (!status.ok()) {
      std::cout << "RouteChat rpc failed." << std::endl;
    }
  }

 private:
  bool GetOneFeature(const Point& point, Feature* feature) {
    ClientContext context;
    Status status = stub_->GetFeature(&context, point, feature);
    if (!status.ok()) {
      std::cout << "GetFeature rpc failed." << std::endl;
      return false;
    }
    if (!feature->has_location()) {
      std::cout << "Server returns incomplete feature." << std::endl;
      return false;
    }
    if (feature->name().empty()) {
      std::cout << "Found no feature at "
                << feature->location().latitude() / kCoordFactor_ << ", "
                << feature->location().longitude() / kCoordFactor_ << std::endl;
    } else {
      std::cout << "Found feature called " << feature->name() << " at "
                << feature->location().latitude() / kCoordFactor_ << ", "
                << feature->location().longitude() / kCoordFactor_ << std::endl;
    }
    return true;
  }

  const float kCoordFactor_ = 10000000.0;
  std::unique_ptr<RouteGuide::Stub> stub_;
  std::vector<Feature> feature_list_;
};

int main(int argc, char** argv) {
  // Expect only arg: --db_path=path/to/route_guide_db.json.
  int count;
  int ws;
  int loop_count;
  
  if(argc > 1) {
    count = atoi(argv[1]);
  } else {
    count = 1000;
  }
  if(argc > 2) {
    ws = atoi(argv[2]);
  } else {
    ws = 100;
  }
  if(count < ws) {
    std::cout << "count < ws" << std::endl;
    return 0;
  }

  loop_count = count / ws;
  
  //std::string db = routeguide::GetDbFileContent(argc, argv);
  RouteGuideClient guide(
      grpc::CreateChannel("localhost:50051",
                          grpc::InsecureChannelCredentials()));

  // std::cout << "-------------- GetFeature --------------" << std::endl;
  //guide.GetFeature();
  for(int i = 0; i < loop_count;i++){
    std::cout << "loop_count "<<  i << std::endl;
    guide.RecordRoute(ws);
  }
  //std::cout << "-------------- ListFeatures --------------" << std::endl;
  //guide.ListFeatures();
  //std::cout << "-------------- RouteChat --------------" << std::endl;
  //guide.RouteChat();

  return 0;
}
