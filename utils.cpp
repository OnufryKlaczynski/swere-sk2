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


using json = nlohmann::json;


void sendMessage(int clientFd, json message)
{
  std::string responseMessageStr = message.dump();
  ssize_t bytesSend = write(clientFd, responseMessageStr.c_str(), responseMessageStr.length());
  if (bytesSend == -1)
  {
    printf("Error while sending reponse: %s\n\n", responseMessageStr.c_str());
  }
  else
  {
    printf("Response send message:\n %s\nbytesSend: %ld\n\n", responseMessageStr.c_str(), bytesSend);
  }
}

std::vector<size_t> findAllStringPositions(std::string str, std::string subStr)
{
  std::vector<size_t> positions; // holds all the positions that sub occurs within str

  size_t pos = str.find(subStr, 0);
  while (pos != std::string::npos)
  {
    positions.push_back(pos);
    pos = str.find(subStr, pos + 1);
  }
  return positions;
}

template <class T>
json vectorToJson(std::vector<T> vector)
{
  auto jsonObject = json::array();
  for (auto &element : vector)
  {
    jsonObject.push_back(element);
  }
  return jsonObject;
}

template <typename TK, typename TV>
std::vector<TK> extract_keys(std::map<TK, TV> const &input_map)
{
  std::vector<TK> retval;
  for (auto const &element : input_map)
  {
    retval.push_back(element.first);
  }
  return retval;
}

template <typename TK, typename TV>
std::vector<TV> extract_values(std::map<TK, TV> const &input_map)
{
  std::vector<TV> retval;
  for (auto const &element : input_map)
  {
    retval.push_back(element.second);
  }
  return retval;
}


