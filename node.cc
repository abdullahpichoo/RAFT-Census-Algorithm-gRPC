#include <stdarg.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std;

#include "raft.grpc.pb.h"

#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using raft::dummyRaftService;
using raft::MessageContent;
using raft::MessageReply;
using raft::VoteReply;
using raft::VoteRequest;

// Global Variables
int TERM_NO = 1;
fstream file_wtr;
string FILE_NAME;

// Node Structure
struct node_details {
  int node_id;
  string node_status;
  int term_no;
  static bool hasVoted;
  vector<string> voters;
};
// Defining Voted Status as Static so that it can be accessed everywhere
bool node_details::hasVoted = false;

// Helper Functions
bool kbhit() {
  termios term;
  tcgetattr(0, &term);

  termios term2 = term;
  term2.c_lflag &= ~ICANON;
  tcsetattr(0, TCSANOW, &term2);

  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);

  tcsetattr(0, TCSANOW, &term);

  return byteswaiting > 0;
}
string get_port(string addr) {
  // Returns the Port Number of an Address String
  bool port_found = false;
  string port;
  for (int i = 0; i < addr.size(); i++) {
    if (addr[i] == ':') {
      port_found = true;
    } else if (port_found) {
      port += addr[i];
    }
  }
  return port;
}

void print_node_details(node_details node) {
  cout << "\nMy ID: " << node.node_id;
  cout << "\nMy Status: " << node.node_status;
  cout << "\nMy Term No: " << node.term_no;
}

void get_nodes_list(string port, vector<string>& node_addresses) {
  cout << "\nThis node will send messages to the following nodes:" << endl;
  for (int i = 0; i < node_addresses.size(); i++) {
    if (get_port(node_addresses[i]) == port) {
      node_addresses.erase(node_addresses.begin() + i);
    }
  }
  for (int i = 0; i < node_addresses.size(); i++) {
    cout << "ID: " << node_addresses[i] << endl;
  }
}
// Server Side Code
class RaftService final : public dummyRaftService::Service {
  Status send_message(ServerContext* context, const MessageContent* msg_content,
                      MessageReply* msg_reply) override {
    // Increase term no as soon as a message is received
    TERM_NO += 1;

    // Printing the message
    cout << "\n---------------------------------\n";
    cout << "Received Message from Node\n";
    cout << "ID: " << msg_content->node_id()
         << ", Status: " << msg_content->node_status()
         << ", Message Term No: " << msg_content->term_no()
         << "\nMy Term No: " << TERM_NO;
    cout << "\n---------------------------------\n";

    //   Writing to Log File
    file_wtr.open(FILE_NAME, ios::out | ios::app);
    file_wtr << "RECEVIED"
             << " from: " << msg_content->node_id() << "\n";
    file_wtr << "ID: " << msg_content->node_id()
             << ", Status: " << msg_content->node_status()
             << ", Term No: " << msg_content->term_no()
             << ", My Term No: " << TERM_NO << "\n";
    file_wtr.close();

    // This is unnecessary for now
    int recv_term_no = msg_content->term_no();
    msg_reply->set_term_no(recv_term_no + 1);

    return Status::OK;
  }

  Status vote_request(ServerContext* context, const VoteRequest* vote_request,
                      VoteReply* vote_reply) override {
    // Increase term no as soon as a vote request is received
    TERM_NO += 1;

    // Printing the message
    cout << "\n---------------------------------\n";
    cout << "Received Vote Request from Node\n";
    cout << "ID: " << vote_request->node_id()
         << ", Status: " << vote_request->node_status()
         << ", Message Term No: " << vote_request->term_no()
         << "\nMy Term No: " << TERM_NO << ", My Vote Status: "
         << (node_details::hasVoted ? "Voted" : "Not Voted");

    //   Writing to Log File
    file_wtr.open(FILE_NAME, ios::out | ios::app);
    file_wtr << "RECEVIED"
             << " from: " << vote_request->node_id() << "\n";
    file_wtr << "ID: " << vote_request->node_id()
             << ", Status: " << vote_request->node_status()
             << ", Term No: " << vote_request->term_no()
             << ", My Term No: " << TERM_NO << "\n"
             << ", My Vote Status: "
             << (node_details::hasVoted ? "Voted" : "Not Voted");
    file_wtr.close();

    // If the node has already voted, then it will not vote again
    // If the node has not voted, then it will vote if the term no is greater
    if (node_details::hasVoted == false && TERM_NO <= vote_request->term_no()) {
      vote_reply->set_vote_granted(true);
      node_details::hasVoted = true;
      cout << "\nVote Granted to " << vote_request->node_id();
    } else {
      vote_reply->set_vote_granted(false);
    }
    vote_reply->set_term_no(TERM_NO);
    cout << "\n---------------------------------\n";
    return Status::OK;
  }
};

void run_server_on_port(string port) {
  // This function will run the server code on the specified port
  std::string address("0.0.0.0:");
  address += port;

  RaftService service;

  ServerBuilder builder;

  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Node Running on: " << address << std::endl;

  server->Wait();
}

// Client Side Code
class Client {
 public:
  Client(shared_ptr<Channel> channel)
      : stub_(dummyRaftService::NewStub(channel)) {}

  int vote_request(const int& node_id, const string& node_status,
                   const int& term_no) {
    VoteRequest vote_request;

    // Set the variables
    vote_request.set_node_id(node_id);
    vote_request.set_node_status(node_status);
    vote_request.set_term_no(term_no);

    VoteReply vote_reply;

    ClientContext context;

    // Sending the RPC call
    Status status = stub_->vote_request(&context, vote_request, &vote_reply);

    if (status.ok()) {
      // Incremented term no
      TERM_NO += 1;
      if (TERM_NO < vote_reply.term_no()) {
        return -1;
      } else if (vote_reply.vote_granted() == true) {
        return 1;
      } else {
        return 0;
      }
    } else {
      return 0;
    }
  }

  void send_message(const int& node_id, const string& node_status,
                    const int& term_no) {
    MessageContent msg_content;

    // Set the variables
    msg_content.set_node_id(node_id);
    msg_content.set_node_status(node_status);
    msg_content.set_term_no(term_no);

    MessageReply msg_reply;

    ClientContext context;

    // Sending the RPC call
    Status status = stub_->send_message(&context, msg_content, &msg_reply);

    if (status.ok()) {
      // Incremented term no
      msg_reply.term_no();
      TERM_NO += 1;
    } else {
      cout << status.error_code() << ": " << status.error_message() << endl;
      //   return "RPC failed";
    }
  }

 private:
  std::unique_ptr<dummyRaftService::Stub> stub_;
};

void run_simple_message_service(vector<string> node_ids, node_details& node) {
  // This function will run the client code on the specified port
  for (auto node_addr : node_ids) {
    string port = get_port(node_addr);
    Client client_node(
        grpc::CreateChannel(node_addr, grpc::InsecureChannelCredentials()));

    //   Sending messages to all the specified nodes
    client_node.send_message(node.node_id, node.node_status, node.term_no);
    cout << "\n---------------------------------\n";
    cout << "Sent Message to Node: " << node_addr;

    // Updating the term number of the structure
    node.term_no = TERM_NO;
    print_node_details(node);
    cout << "\n---------------------------------\n";

    //   Writing to Log File
    file_wtr.open(FILE_NAME, ios::out | ios::app);
    file_wtr << "SENT"
             << " to: " << node_addr << "\n";
    file_wtr << "ID: " << node.node_id << ", Status: " << node.node_status
             << ", Term No: " << node.term_no << "\n";
    file_wtr.close();
  }
}

void run_vote_service(vector<string> node_ids, node_details& node) {
  // This function will run the client code on the specified port
  vector<int> voters;
  for (auto node_addr : node_ids) {
    string port = get_port(node_addr);
    Client client_node(
        grpc::CreateChannel(node_addr, grpc::InsecureChannelCredentials()));

    //   Sending messages to all the specified nodes
    int vote_success_status =
        client_node.vote_request(node.node_id, node.node_status, node.term_no);
    cout << "\n---------------------------------\n";
    cout << "Sent Vote Request to Node: " << node_addr << endl;

    // Checking the status of the vote
    // 1 ==> Vote Granted
    // 0 ==> Vote Rejected
    //-1 ==> Vote Rejected: Term No is greater than the current term no
    if (vote_success_status == 1) {
      cout << "-> Vote Granted by Node: " << node_addr << "\n";
      node.voters.push_back(node_addr);
    } else if (vote_success_status == 0) {
      cout << "-> Vote Rejected by Node: " << node_addr << "\n";
    } else {
      cout << "-> Vote Rejected by Node: " << node_addr << "\n";
      cout << "Their term No is greater than the my term no!\n";
      node.node_status = "Follower";
      cout << "===============> My Status: Follower!\n";
      cout << "Terminating voting service...\n\n";
      return;
    }

    // Updating the term number of the structure
    node.term_no = TERM_NO;
    print_node_details(node);
    cout << "\n---------------------------------\n";

    //   Writing to Log File
    file_wtr.open(FILE_NAME, ios::out | ios::app);
    file_wtr << "SENT"
             << " to: " << node_addr << "\n";
    file_wtr << "ID: " << node.node_id << ", Status: " << node.node_status
             << ", Term No: " << node.term_no << "\n";
    file_wtr.close();

    sleep(3);
  }
  if (node.voters.size() >= 3) {
    node.node_status = "Leader";
    cout << "===============> My Status: Leader!\n";
    cout << "Terminating voting service...\n\n";
    return;
  }
}

int main(int argc, char** argv) {
  // Defining Node Addresses
  string address1("0.0.0.0:4001");
  string address2("0.0.0.0:4002");
  string address3("0.0.0.0:4003");
  string address4("0.0.0.0:4004");
  string address5("0.0.0.0:4005");
  vector<string> node_addresses = {address1, address2, address3, address4};

  // Extracting the command line arguments
  string port = argv[1];
  string node_status = argv[2];
  string term_no = argv[3];

  cout << "Node ID: " << port << " Status: " << node_status
       << " Term No: " << term_no << endl;

  char c;
  cout << "Continue with these parameters? (y/n): ";
  cin >> c;
  if (c == 'n') {
    cout << "Terminating....\n";
    return 0;
  }

  // Initializing Global Variables
  TERM_NO = stoi(term_no);
  FILE_NAME = "LOGFILE_" + port + ".txt";

  // Initializing Node Structure
  node_details node;
  node.node_id = stoi(port);
  node.node_status = node_status;
  node.term_no = TERM_NO;

  // Removing the address of this node from the list of total node addresses so
  // that this node doesn't send messages to itself
  get_nodes_list(port, node_addresses);

  // Initiating Server Side Code
  thread server_thread(run_server_on_port, port);
  server_thread.detach();

  sleep(1);

  // Initiating Voting Service
  int choice;

  if (node_status == "Candidate") {
    do {
      cout << "Press [1] to start voting service." << endl;
      cout << "Press [2] to start simple messaging service." << endl;
      cin >> choice;
      if (choice == 1) {
        run_vote_service(node_addresses, node);
        break;
      } else if (choice > 2 && choice < 1) {
        cout << "Invalid Choice! Try Again!\n";
      }
    } while (choice > 2 && choice < 1);
  }

  cout << "Initiating Messaging Service!\nRecieved messages will show "
          "here.\nPress any key to send message to all other nodes..."
       << endl;
  while (true) {
    if (kbhit()) {
      break;
    }
  }
  // Initiating Client Side Code
  //   It will keep sending messages as long as I press 1 on every prompt
  while (true) {
    thread client_thread(run_simple_message_service, node_addresses, ref(node));
    client_thread.join();
    cout << "Press [1] to resend messages to all nodes: ";
    cin >> choice;
    if (choice != 1) {
      break;
    }
  }

  cout << "\n==> This node can only recieve messages from now!\nYou can "
          "terminate this node by pressing any key...[DON'T PRESS CTRL-Z]"
       << endl;

  char x;
  cin >> x;

  return 0;
}