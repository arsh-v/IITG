#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>
#include <thread>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <set>
#include <sstream>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include "streaminfo.cpp"
#include "src/VideoDataFeedManager.cpp"

#define BUFFER_SIZE 1024
std::mutex streamsMutex;
std::vector<std::shared_ptr<StreamInfo>> streams; // List of all streams


void consumeTrackerData(std::stringstream& ss){
  while(true){
    std::string allData = ss.str();
    size_t pos = allData.find("\n");
    if (pos != std::string::npos){
      std::string data = allData.substr(0, pos);
      ss.str(allData.substr(pos+1));

      std::istringstream iss(data);
      std::string line;
      {
        std::lock_guard<std::mutex> lock(streamsMutex);
        while (std::getline(iss, line, '\t'))
        {
          std::shared_ptr<StreamInfo> stream = std::make_shared<StreamInfo>(line);
          bool found = false;
          for (auto &s : streams)
          {
            if (s->getName() == stream->getName())
            {
              s->resetHeartbeat();
              s->setIpAddress(stream->getIpAddress());
              s->setPort(stream->getPort());
              s->setStreamingIpAddress(stream->getStreamingIpAddress());
              s->setStreamingPort(stream->getStreamingPort());
              s->setDescription(stream->getDescription());
              found = true;
              return;
            }
          }
          if (!found)
          {
            streams.push_back(stream);
          }
        }
      }
      sleep(6);
    }
    else{
      break;
    }
  }
}

void handleTracker(const std::string& ip_address, const int port){
  int sockfd;
  struct sockaddr_in server_addr;

  // Create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  // Set up the server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());

  // Connect to server
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Failed to connect to tracker");
    exit(EXIT_FAILURE);
  }

  std::cout << "Connected to tracker" << std::endl;
  // print my port
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
    perror("getsockname");
  else
    printf("My port number is %d\n", ntohs(sin.sin_port));

  // get the list of streams in a loop and update the list of streams

  std::stringstream ss;

  while (true)
  {
    char buffer[BUFFER_SIZE+1];
    int bytesReceived = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (bytesReceived <= 0)
    {
      std::cout << "Trying again in 6 seconds, fucked" << std::endl;
      perror("Error receiving data from tracker");
      sleep(6);
      continue;
    }
    
    buffer[bytesReceived] = '\0';

    ss << buffer;
    consumeTrackerData(ss);
  }
}

void on_show_button_clicked(GtkWidget *widget, gpointer user_data)
{
  int *index = (int *)(user_data);
  std::shared_ptr<StreamInfo> stream = streams[*index];

  VideoDataFeedManager::start(stream);
  
  // For demonstration purposes, let's create a static message.
  std::string message = "Message from stream: " + stream->getName(); // Adjust as necessary

  // Create a new popup to show the message
  GtkWidget *message_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(message_popup), "Stream Message");
  gtk_window_set_default_size(GTK_WINDOW(message_popup), 250, 100);
  g_signal_connect(message_popup, "destroy", G_CALLBACK(VideoDataFeedManager::stop), NULL);

  GtkWidget *message_label = gtk_label_new(message.c_str());
  gtk_container_add(GTK_CONTAINER(message_popup), message_label);

  gtk_widget_show_all(message_popup);
}

void on_widget_destroyed_data_deletion(GtkWidget *widget, gpointer user_data)
{
  int *index = (int *)(user_data);
  delete index;
}
void on_row_selected(GtkListBox *listbox, gpointer user_data)
{
  int* index = (int*) user_data;

  if (*index >= streams.size())
  {
    std::cout << "Index out of bounds" << std::endl;
    return;
  }
  std::shared_ptr<StreamInfo> stream = streams[*index];
  if(stream == NULL) {
    std::cout << "Stream is null" << std::endl;
    return;
  }
  else {
    std::cout << "Stream is not null" << std::endl;
    std::cout << stream->encode() << std::endl;
    std::cout << "not null" << std::endl;
  }

  // Create a new popup window
  GtkWidget *popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(popup), "Stream Details");
  gtk_window_set_default_size(GTK_WINDOW(popup), 250, 150);

  // Vertical box to contain the details and button
  GtkWidget *vbox_popup = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(popup), vbox_popup);


  // Create labels for the name and description
  GtkWidget *name_label = gtk_label_new(("Name: " + stream->getName()).c_str());
  GtkWidget *desc_label = gtk_label_new(("Description: " + stream->getDescription()).c_str()); // Assuming getDescription exists
  gtk_box_pack_start(GTK_BOX(vbox_popup), name_label, 0, 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox_popup), desc_label, 0, 0, 0);

  // Create a "Show" button
  GtkWidget *show_button = gtk_button_new_with_label("Show");
  int* index2 = new int(*index);
  g_signal_connect(show_button, "clicked", G_CALLBACK(on_show_button_clicked), index2);
  g_signal_connect(show_button, "destroy", G_CALLBACK(on_widget_destroyed_data_deletion), index2);
  gtk_box_pack_start(GTK_BOX(vbox_popup), show_button, 0, 0, 0);

  // Show the popup and its child widgets
  gtk_widget_show_all(popup);
}


void populateListbox(GtkWidget *listbox)
{
  GList *children, *iter;

  children = gtk_container_get_children(GTK_CONTAINER(listbox));
  for (iter = children; iter != NULL; iter = g_list_next(iter))
    gtk_widget_destroy(GTK_WIDGET(iter->data));
  g_list_free(children);

  std::lock_guard<std::mutex> lock(streamsMutex);
  for (int i = 0; i < streams.size(); i++)
  {
    if(!streams[i]->isAlive()) continue;
    std::cout << "gtk --- " << streams[i]->encode() << std::endl;
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(streams[i]->getName().c_str()); // Assuming your StreamInfo has a toString method
    gtk_container_add(GTK_CONTAINER(row), label);
    gtk_list_box_insert(GTK_LIST_BOX(listbox), row, -1);

    // Connect the row to the "on_row_selected" callback
    int *index = new int(i);
    std::cout << "index: " << index << std::endl;
    g_signal_connect(row, "activate", G_CALLBACK(on_row_selected), index);
    g_signal_connect(row, "destroy", G_CALLBACK(on_widget_destroyed_data_deletion), index);

    gtk_widget_show(row);
    gtk_widget_show(label);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
  }
}

gboolean updateListBox(gpointer user_data)
{
  GtkWidget *listbox = (GtkWidget *)user_data;
  populateListbox(listbox);
  return TRUE; // This ensures that the timeout function gets called repeatedly
}

void on_refresh_clicked(GtkWidget *widget, gpointer user_data)
{
  GtkWidget *listbox = (GtkWidget *)user_data;
  populateListbox(GTK_WIDGET(listbox));
}

void handleGTK(int argc, char *argv[]){
  gtk_init(&argc, &argv);
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Select Stream");
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  GtkWidget *listbox = gtk_list_box_new();
  gtk_box_pack_start(GTK_BOX(vbox), listbox, 1, 1, 0);

  GtkWidget *refreshButton = gtk_button_new_with_label("Refresh");
  g_signal_connect(refreshButton, "clicked", G_CALLBACK(on_refresh_clicked), listbox);
  gtk_box_pack_start(GTK_BOX(vbox), refreshButton, 0, 0, 0);

  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(window);
  updateListBox(listbox);
  g_timeout_add_seconds(6, updateListBox, listbox);
  gtk_main();
}

int main( int argc, char *argv[] )
{

  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << " <Tracker IP> <port>" << std::endl;
    return -1;
  }

  std::string ip_address = argv[1];
  int port = atoi(argv[2]);

  std::thread trackerThread(handleTracker, ip_address, port);
  std::thread gtkThread(handleGTK, argc, argv);

  std::string input;
  std::cout << "Receiver is running" << std::endl;
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
      std::lock_guard<std::mutex> lock(streamsMutex);
      std::cout << "List of all streams:" << std::endl;
      std::cout << "Name Description Viewers IP:Port Alive StreamingIP:Port" << std::endl;
      for (auto &stream : streams)
      {
        std::cout << stream->getName() << "\t" << stream->getDescription() << "\t" << stream->getNumOfViewers() << "\t" << stream->getIpAddress() << ":" << stream->getPort() << "\t" << std::boolalpha << stream->isAlive() << "\t" << stream->getStreamingIpAddress() << ":" << stream->getStreamingPort() << std::endl;
      }
    }
  }

  trackerThread.detach();
  gtkThread.detach();
  return 0;
}
