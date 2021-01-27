#pragma once

#include <string>
#include "lib/json.hpp"
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <poll.h>

#include "lib/json.hpp"
#include "Room.cpp"
#include "utils.cpp"

std::vector<std::string> claimUser(json messageFromClient, int clientFd, std::vector<Room> rooms, std::vector<std::string> users)
{
  bool nameOk = true;
  for (int j = 0; j < users.size(); j++)
    if (users[j] == (std::string)messageFromClient["nick"])
      nameOk = false;
  if (nameOk)
  {
    users.push_back((std::string)messageFromClient["nick"]);
  }
  else
    users.push_back((std::string)messageFromClient["nick"] + "123");
  std::cout << "Użytkownik " << (std::string)messageFromClient["nick"] << " "
            << "został dopisany do listy użytkowników" << std::endl
            << std::endl;
  std::cout << "Message from client:\n\n"
            << messageFromClient.dump() << std::endl;
  json responseMessage = R"(
                                                    {
                                                        "type": "USER_AUTHENTICATED",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
  responseMessage["rooms"] = rooms;
  responseMessage["nameOk"] = nameOk;
  sendMessage(clientFd, responseMessage);
  return users;
}