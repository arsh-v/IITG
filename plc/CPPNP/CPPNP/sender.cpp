#include <iostream>
#include <cstring>
#include <thread>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include "streaminfo.cpp"
#include <fstream>

#define MULTICAST_IP "239.0.0.1"
#define PORT 8888
#define BUFFER_SIZE 32000

StreamInfo stream("0.0.0.0", 0, "video1", "videoDescription", MULTICAST_IP, PORT);

void handlerTracker(const std::string& ip_address, const int port)
{
  int sockfd;
  struct sockaddr_in tracker_addr;

  // Create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("Failed to create socket");
  }

  // Set up the tracker address
  memset(&tracker_addr, 0, sizeof(tracker_addr));
  tracker_addr.sin_family = AF_INET;
  tracker_addr.sin_port = htons(port);
  tracker_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());

  // Connect to the tracker
  if (connect(sockfd, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0)
  {
    perror("Failed to connect to tracker");
  }

  // Send the existence message
  std::string message = stream.encode() + "\n";
  if (send(sockfd, message.c_str(), message.length(), 0) < 0)
  {
    perror("Failed to send message");
  }

  // sendHeartbeat
  while (true)
  {
    std::string heartbeat = "heartbeat\n";
    std::cout << "Sending heartbeat to " << ip_address << ":" << port << std::endl;
    if (send(sockfd, heartbeat.c_str(), heartbeat.length(), 0) < 0)
    {
      perror("Failed to send heartbeat");
    }
    sleep(2);
  }
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << " <ip_address> <port>" << std::endl;
    return -1;
  }

  std::string ip_address = argv[1];
  int port = atoi(argv[2]);

  std::thread trackerThread(handlerTracker, ip_address, port);

  // Construct the ffmpeg command
  std::string ffmpegCmd = "ffmpeg -re -i Up.mkv -c:v mpeg2video -b:v 4M -f mpegts udp://" + std::string(MULTICAST_IP) + ":" + std::to_string(PORT);

  // Execute the ffmpeg command
  int ret = system(ffmpegCmd.c_str());
  if (ret != 0)
  {
    std::cerr << "ffmpeg command failed." << std::endl;
    return -1;
  }

  trackerThread.join();
  return 0;
}