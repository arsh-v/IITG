#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <ctime>

class StreamInfo
{
private:
  std::string ipAddress;         // IP address of the stream server
  int port;                      // Port number of the stream server
  std::string name;              // Unique identifier for the stream/channel
  std::string description;       // Description or name for the stream
  std::set<std::string> viewers; // List of all viewers
  time_t lastHeartbeat;          // Last time the stream was updated
  std::string streamingIP;       // IP address of the actual streaming source
  int streamingPort;             // Port number on which the stream is being broadcasted

public:
  StreamInfo(const std::string &ip, int port, const std::string &name, const std::string &desc,
             const std::string &streamIP, int streamPort)
      : ipAddress(ip), port(port), name(name), description(desc), viewers({}), lastHeartbeat(time(NULL)),
        streamingIP(streamIP), streamingPort(streamPort) {}
  // Getters and Setters
  std::string getIpAddress() const { return ipAddress; }
  int getPort() const { return port; }
  std::string getName() const { return name; }
  std::string getDescription() const { return description; }
  int getNumOfViewers() const { return viewers.size(); }
  std::string getStreamingIpAddress() const { return streamingIP; }
  int getStreamingPort() const { return streamingPort; }

  void setStreamingIpAddress(const std::string &ip) { streamingIP = ip; }
  void setStreamingPort(int p) { streamingPort = p; }
  void setIpAddress(const std::string &ip) { ipAddress = ip; }
  void setPort(int p) { port = p; }

  void setDescription(const std::string &desc) { description = desc; }
  void addViewer(const std::string &ip, int port)
  {
    viewers.insert(ip + ":" + std::to_string(port));
  }
  void removeViewer(const std::string &ip, int port)
  {
    viewers.erase(ip + ":" + std::to_string(port));
  }

  bool isAlive() const
  {
    return (time(NULL) - lastHeartbeat) < 4;
  }

  void resetHeartbeat()
  {
    lastHeartbeat = time(NULL);
  }

  StreamInfo(const std::string &encoded)
  {
    int pos = 0;
    int divider = encoded.find_first_of(" ", pos);
    ipAddress = encoded.substr(pos, divider - pos);
    pos = divider + 1;

    divider = encoded.find_first_of(" ", pos);
    port = std::stoi(encoded.substr(pos, divider - pos));
    pos = divider + 1;

    divider = encoded.find_first_of(" ", pos);
    name = encoded.substr(pos, divider - pos);
    pos = divider + 1;

    divider = encoded.find_first_of(" ", pos);
    description = encoded.substr(pos, divider - pos);
    pos = divider + 1;

    divider = encoded.find_first_of(" ", pos);
    streamingIP = encoded.substr(pos, divider - pos);
    pos = divider + 1;

    streamingPort = std::stoi(encoded.substr(pos));

    viewers = {};
    lastHeartbeat = time(NULL);
  }

  std::string encode() const
  {
    return ipAddress + " " + std::to_string(port) + " " + name + " " + description + " " +
           streamingIP + " " + std::to_string(streamingPort);
  }
};