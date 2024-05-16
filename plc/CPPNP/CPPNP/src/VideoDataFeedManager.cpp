// VideoDataFeedManager.cpp
#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../streaminfo.cpp"
#include <filesystem>
#include <fstream>

#define BUFFER_SIZE 1024

void deleteFile(const std::string& path)
{
  try
  {
    std::filesystem::remove(path);
  }
  catch (const std::filesystem::filesystem_error &err)
  {
    std::cerr << "filesystem error: " << err.what() << '\n';
  }
}

class VideoDataFeedManager
{
public:
  static std::mutex isPlayingMutex;
  static std::atomic<bool> isPlaying;
  static std::thread streamingThread;
  static std::shared_ptr<StreamInfo> stream;

  static void start(std::shared_ptr<StreamInfo> strm);
  static void stop();
  static void start_data_flow();
  ~VideoDataFeedManager()
  {
    stop();
    if (streamingThread.joinable())
    {
      streamingThread.join();
    }
  }
};

std::mutex VideoDataFeedManager::isPlayingMutex;
std::atomic<bool> VideoDataFeedManager::isPlaying(false);
std::thread VideoDataFeedManager::streamingThread;
std::shared_ptr<StreamInfo> VideoDataFeedManager::stream;

void VideoDataFeedManager::start(std::shared_ptr<StreamInfo> strm)
{
  std::lock_guard<std::mutex> lock(isPlayingMutex);
  stream = strm;
  isPlaying = true;
  streamingThread = std::thread(&VideoDataFeedManager::start_data_flow);
}

void VideoDataFeedManager::stop()
{
  std::lock_guard<std::mutex> lock(isPlayingMutex);
  isPlaying = false;
  if (streamingThread.joinable())
  {
    streamingThread.join();
  }
}



void VideoDataFeedManager::start_data_flow()
{
  int sockfd;
  struct sockaddr_in multicastAddr;
  struct ip_mreq mreq;

  // Create socket for receiving multicast datagrams
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Enable SO_REUSEADDR to allow multiple instances of this application to receive copies of the multicast datagrams
  int reuse = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
  {
    perror("Reusing ADDR failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  // Set up destination address
  memset(&multicastAddr, 0, sizeof(multicastAddr));
  multicastAddr.sin_family = AF_INET;
  multicastAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  multicastAddr.sin_port = htons(stream->getStreamingPort());

  // Bind to receive address
  if (bind(sockfd, (struct sockaddr *)&multicastAddr, sizeof(multicastAddr)) < 0)
  {
    perror("bind");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  // Use setsockopt() to request that the kernel join a multicast group
  std::cout << "IP: " << stream->getStreamingIpAddress() << std::endl;
  mreq.imr_multiaddr.s_addr = inet_addr(stream->getStreamingIpAddress().c_str());
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
  {
    perror("setsockopt");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  deleteFile("livedata");

  std::string ffplayCommand = "ffplay -i livedata -fflags nobuffer &";

  system(ffplayCommand.c_str());

  char buffer[BUFFER_SIZE + 1];
  std::ofstream outfile("livedata", std::ios::binary | std::ios::out | std::ios::app);
  while (isPlaying)
  {
    int nbytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
    if (nbytes < 0)
    {
      perror("recvfrom");
      close(sockfd);
      exit(EXIT_FAILURE);
    }
    buffer[nbytes] = '\0'; // null-terminate string
    outfile.write(buffer, nbytes);
    outfile.flush(); // Ensure data is written immediately for ffplay to read
  }
  outfile.close();
  std::cout << "Stopping playback" << std::endl;

  // Stop ffplay when done
  system("pkill -f 'ffplay -i livedata'");
  close(sockfd);
}
