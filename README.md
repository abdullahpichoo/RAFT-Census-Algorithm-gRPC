## RAFT Census Algorithm
This is an implementation of a distributed system using the Raft consensus algorithm. The library used for the implementation is gRPC and the programming language used is C++.

The code defines a `node_details` struct to represent each node in the distributed system. The struct contains the `node_id`, `node_status`, `term_no`, and a static `hasVoted` boolean to indicate if the node has already voted in the current term. 

The server side of the system is implemented using the `RaftService` class, which extends `dummyRaftService::Service`. This class implements two methods - `send_message` and `vote_request`. Both methods increment the `TERM_NO` global variable to indicate that a new term has begun.

The `send_message` method receives a `MessageContent` object from a client and prints the message to the console. It also appends the message to a log file. The method then creates a `MessageReply` object and increases its `term_no` attribute before returning the object.

The `vote_request` method receives a `VoteRequest` object from a client and prints the message to the console. It also appends the message to a log file. If the node has not yet voted in the current term, it creates a `VoteReply` object and sets its `vote_granted` attribute to `true`. It then sets the `hasVoted` attribute of the `node_details` struct to `true` and returns the `VoteReply` object. If the node has already voted in the current term, it creates a `VoteReply` object and sets its `vote_granted` attribute to `false` before returning the object.

The `main` function of the code reads in a configuration file containing the addresses of all the nodes in the system. It then creates a `node_details` object for each node and initializes its attributes. It also creates a `RaftService` object and starts a server on each node. The main function then enters a loop where it listens for user input. If a user enters a message, it sends the message to all other nodes in the system using the `send_message` method. If a user enters a vote request, it sends the request to all other nodes in the system using the `vote_request` method.

### How to run this code on your system:
- Install gRPC by following their offical documentation.
- Clone this repo in grpc/examples/cpp/
- Open the given directory in terminal and run the following commands:
  - $ mkdir -p cmake/build
  - $ pushd cmake/build
  - $ cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..
  - $ make -j 4
- After running the commands, you can execute the node.cc file by typing in the terminal:
 $ ./node 4001 Follower term_no
 where term_no can be any number and 4001 is the port number
 Note that you are supposed to run this executable in multiple terminals (max 4) in order to simulate the distributed environment. You can only the following port numbers: 4001, 4002, 4003, 4004.
