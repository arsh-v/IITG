#include <iostream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <set>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <ctime>
#include <mutex>
#include <sstream>

#include "streaminfo.cpp"

#define masterClientPort 23008
#define masterServerPort 23007
#define BUFFER_SIZE 1024

std::vector<StreamInfo> streams; // List of all streams
std::mutex streams_mutex;        // Mutex for protecting the streams vector

void sendStreamersList(int clientSocket)
{
  while (true)
  {
    std::string clientList = "";
    // Lock the mutex to safely access the shared streams vector
    {
      std::lock_guard<std::mutex> lock(streams_mutex);
      clientList.clear();
      for (auto &stream : streams)
      {
        if (stream.isAlive())
        {
          clientList += stream.encode() + "\t";
        }
      }
    }
    clientList += "\n";
    std::cout << "Sending streamers list to " << clientSocket << std::endl;

    ssize_t bytesSent = send(clientSocket, clientList.c_str(), clientList.length(), 0);
    if (bytesSent < 0)
    {
      perror("Error sending data to client");
      close(clientSocket); // Close the client socket
      break;               // Exit the function instead of terminating the entire program
    }
    sleep(3);
  }
}

void handleClients()
{
  int masterSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (masterSocket < 0)
  {
    perror("Error creating socket");
    exit(EXIT_FAILURE);
  }

  sockaddr_in masterAddr;
  masterAddr.sin_family = AF_INET;
  masterAddr.sin_port = htons(masterClientPort);
  masterAddr.sin_addr.s_addr = INADDR_ANY;

  if (bind(masterSocket, (sockaddr *)&masterAddr, sizeof(masterAddr)) < 0)
  {
    perror("Error binding socket");
    exit(EXIT_FAILURE);
  }

  if (listen(masterSocket, 5) < 0)
  {
    perror("Error listening on socket");
    exit(EXIT_FAILURE);
  }

  while (true)
  {
    sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
    int clientSocket = accept(masterSocket, (sockaddr *)&clientAddr, &clientAddrSize);
    if (clientSocket < 0)
    {
      perror("Error accepting client");
      exit(EXIT_FAILURE);
    }

    std::thread T_clientThread(sendStreamersList, clientSocket);
    T_clientThread.detach();
  }
  close(masterSocket);
}

void consumeServerData(std::stringstream &ss, const std::string &ipAddress, const int port)
{
  while (true)
  {
    std::string allData = ss.str();
    size_t pos = allData.find("\n");
    if (pos != std::string::npos)
    {
      std::string data = allData.substr(0, pos);
      ss.str(allData.substr(pos + 1));
      // check for heartbeat
      if (data == "heartbeat")
      {
        std::lock_guard<std::mutex> lock(streams_mutex);
        // std::cout << "Received heartbeat from " << ipAddress << ":" << port << std::endl;
        for (auto &stream : streams)
        {
          if (stream.getIpAddress() == ipAddress && stream.getPort() == port)
          {
            stream.resetHeartbeat();
            break;
          }
        }
        continue;
      }

      StreamInfo incomingStream(data);

      bool found = false;
      std::lock_guard<std::mutex> lock(streams_mutex);
      for (auto &stream : streams)
      {
        if (stream.getName() == incomingStream.getName())
        {
          found = true;
          stream.setDescription(incomingStream.getDescription());
          stream.setStreamingIpAddress(incomingStream.getStreamingIpAddress());
          stream.setStreamingPort(incomingStream.getStreamingPort());
          stream.resetHeartbeat();
          stream.setIpAddress(ipAddress);
          stream.setPort(port);
          // std::cout << "Updated stream " << name << " from " << ipAddress << ":" << port << std::endl;
          break;
        }
      }

      if (!found)
      {
        incomingStream.setIpAddress(ipAddress);
        incomingStream.setPort(port);
        // std::cout << "Added stream " << name << " from " << ipAddress << ":" << port << std::endl;
        streams.push_back(incomingStream);
      }
    }
    else
    {
      break;
    }
  }
}

void handleServerData(int serverSocket, const std::string &ipAddress, const uint16_t port){
  std::stringstream ss;
  char buffer[BUFFER_SIZE + 1];
  while(true){
    int bytesReceived = recv(serverSocket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived < 0)
    {
      perror("Error receiving data from server");
      exit(EXIT_FAILURE);
    }
    if (bytesReceived == 0)
    {
      close(serverSocket);
      std::cout << "Server disconnected: " << ipAddress << ":" << port << std::endl;
      break;
    }
    buffer[bytesReceived] = '\0';

    // std::cout << "Received: " << buffer << " from " << ipAddress << ":" << port << std::endl;
    ss << buffer;
    consumeServerData(ss, ipAddress, port);
  }
}

void handleServers()
{
  int masterSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (masterSocket < 0)
  {
    perror("Error creating socket");
    exit(EXIT_FAILURE);
  }

  sockaddr_in masterAddr;
  masterAddr.sin_family = AF_INET;
  masterAddr.sin_port = htons(masterServerPort);
  masterAddr.sin_addr.s_addr = INADDR_ANY;

  if (bind(masterSocket, (sockaddr *)&masterAddr, sizeof(masterAddr)) < 0)
  {
    perror("Error binding socket");
    exit(EXIT_FAILURE);
  }

  if (listen(masterSocket, 5) < 0)
  {
    perror("Error listening on socket");
    exit(EXIT_FAILURE);
  }


  std::stringstream ss;

  while (true)
  {
    sockaddr_in serverAddr;
    socklen_t serverAddrSize = sizeof(serverAddr);
    int serverSocket = accept(masterSocket, (sockaddr *)&serverAddr, &serverAddrSize);
    if (serverSocket < 0)
    {
      perror("Error accepting server");
      exit(EXIT_FAILURE);
    }

    const std::string serverIp = inet_ntoa(serverAddr.sin_addr);
    const uint16_t serverPort = ntohs(serverAddr.sin_port);

    std::cout << "Connected to server " << serverIp << ":" << serverPort << std::endl;

    std::thread T_handleServerData(handleServerData, serverSocket, serverIp, serverPort);
    T_handleServerData.detach();
  }
}

int main()
{
  std::thread T_clientThread(handleClients);
  std::thread T_serverThread(handleServers);

  std::string input;
  std::cout << "Tracker is running" << std::endl;
  std::cout << "Type 'list' to list all streams" << std::endl;
  std::cout << "Type 'exit' to quit" << std::endl;
  while (true)
  {
    std::cin >> input;
    if (input == "exit")
    {
      break;
    }
    else if (input == "list")
    {
      std::lock_guard<std::mutex> lock(streams_mutex);
      std::cout << "List of all streams:" << std::endl;
      std::cout << "Name Description Viewers IP:Port Alive StreamingIP:Port" << std::endl;
      for (auto &stream : streams)
      {
        std::cout << stream.getName() << "\t" << stream.getDescription() << "\t" << stream.getNumOfViewers() << "\t" << stream.getIpAddress() << ":" << stream.getPort() << "\t" << std::boolalpha << stream.isAlive() << "\t" << stream.getStreamingIpAddress() << ":" << stream.getStreamingPort() << std::endl;
      }
    }
  }

  T_serverThread.detach();
  T_clientThread.detach();
  return 0;
}