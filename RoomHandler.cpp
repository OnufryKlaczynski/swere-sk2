#pragma once

#include <string>
#include <vector>
#include <set>
#include <chrono>

#include "lib/json.hpp"
#include "utils.cpp"
#include "Room.cpp"

std::map<std::string, Room> checkRoomTimers(std::map<std::string, Room> rooms)
{
    std::chrono::time_point<std::chrono::system_clock> current_time = std::chrono::system_clock::now();
    std::map<std::string, Room>::iterator it = rooms.begin();

    while (it != rooms.end())
    {
        if (it->second.isGameStarted)
        {
            std::time_t ttc1 = std::chrono::system_clock::to_time_t(current_time);
            std::time_t ttc2 = std::chrono::system_clock::to_time_t(it->second.start_time);
            std::time_t ttc3 = ttc1 - ttc2;
            struct tm *tmp = gmtime(&ttc3);
            if (tmp->tm_sec == 60)
            {
                it->second.gameFinished = true;
                it->second.timeRunOut = true;
                for (int descriptor : it->second.usersDescriptors)
                {
                    json responseMessage = R"(
                                        {
                                            "type": "GAME_LOST",
                                            "result": "SUCCESS"
                                        }
                                    )"_json;
                    responseMessage["word"] = it->second.wordToFind;
                    responseMessage["timeRunOut"] = it->second.timeRunOut;
                    sendMessage(descriptor, responseMessage);
                }
                rooms.erase(it->first);
                if (rooms.empty())
                    break;
                it = rooms.begin();
            }
        }
        it++;
    }
    return rooms;
}

std::map<std::string, Room> createRoom(std::string roomName, std::string hostNick, int clientFd, std::map<std::string, Room> rooms, std::vector<int> clients)
{
    bool nameOk = false;
    if (!rooms.count(roomName))
    {
        nameOk = true;
        Room newRoom(roomName, hostNick);
        rooms.insert(std::pair<std::string, Room>(roomName, newRoom));
        Room &room = rooms.at(roomName);
        room.nicks.push_back(hostNick);
        room.usersDescriptors.push_back(clientFd);
        std::set<std::string> emptySet = std::set<std::string>();
        room.userLettersMap.insert(std::pair<std::string, std::set<std::string>>(hostNick, emptySet));
        room.userWrongCounterMap.insert(std::pair<std::string, int>(hostNick, 0));
        room.hostNick = hostNick;
    }
    json responseMessage = R"(
                           {
                             "type": "ROOM_CREATED",
                              "result": "SUCCESS"
                               }
                            )"_json;
    responseMessage["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));
    responseMessage["nameOk"] = nameOk;

    if (nameOk)
    {
        for (int descriptor : clients)
        {
            sendMessage(descriptor, responseMessage);
        }
        responseMessage = R"(
                                                    {
                                                        "type": "USER_JOINED_CREATED_ROOM",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
        Room &room = rooms.at(roomName);
        responseMessage["otherPlayersInRoom"] = vectorToJson(room.nicks);
        sendMessage(clientFd, responseMessage);
    }
    else
    {
        sendMessage(clientFd, responseMessage);
    }
    return rooms;
}
void joinRoom(std::string nick, int clientFd, Room &room, const int MAX_PLAYERS_IN_ROOM)
{
    room.nicks.push_back(nick);
    if (room.nicks.size() >= MAX_PLAYERS_IN_ROOM)
    {
        room.maxPlayers = true;
    }
    std::set<std::string> emptySet = std::set<std::string>();
    room.usersDescriptors.push_back(clientFd);
    room.userLettersMap.insert(std::pair<std::string, std::set<std::string>>(nick, emptySet));
    room.userWrongCounterMap.insert(std::pair<std::string, int>(nick, 0));
    json responseMessage = R"(
                                                    {
                                                        "type": "USER_JOINED_ROOM",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
    responseMessage["otherPlayersInRoom"] = vectorToJson(room.nicks);

    for (int descriptor : room.usersDescriptors)
    {
        sendMessage(descriptor, responseMessage);
    }
}

std::map<std::string, Room> leaveRoom(std::string nick, std::string roomName, std::vector<int> clients, int position, std::map<std::string, Room> rooms, const int MAX_PLAYERS_IN_ROOM, bool leave)
{
    Room &room = rooms.at(roomName);
    if (!leave)
    {
        room.nicks.erase(std::remove(room.nicks.begin(), room.nicks.end(), nick), room.nicks.end());
        room.userLettersMap.erase(nick);
        room.userWrongCounterMap.erase(nick);
    }
    if (room.nicks.size() < MAX_PLAYERS_IN_ROOM)
    {
        room.maxPlayers = false;
    }
    json responseMessage = R"(
                                                    {
                                                        "type": "USER_LEFT_ROOM",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
    responseMessage["otherPlayersInRoom"] = vectorToJson(room.nicks);
    responseMessage["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));

    for (int descriptor : room.usersDescriptors)
    {
        sendMessage(descriptor, responseMessage);
    }
    if (!leave)
        room.usersDescriptors.erase(std::remove(room.usersDescriptors.begin(), room.usersDescriptors.end(), clients[position]), room.usersDescriptors.end());
    return rooms;
}

std::map<std::string, Room> leaveGame(std::string nick, std::string roomName, int clientFd, std::map<std::string, Room> rooms)
{
    Room &room = rooms.at(roomName);
    room.nicks.erase(std::remove(room.nicks.begin(), room.nicks.end(), nick), room.nicks.end());
    room.usersDescriptors.erase(std::remove(room.usersDescriptors.begin(), room.usersDescriptors.end(), clientFd), room.usersDescriptors.end());
    room.userLettersMap.erase(nick);
    room.userWrongCounterMap.erase(nick);
    json responseMessage = R"(
                                                    {
                                                        "type": "USER_LEFT_GAME",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
    responseMessage["otherPlayersInRoom"] = vectorToJson(room.nicks);
    responseMessage["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));

    for (int descriptor : room.usersDescriptors)
    {
        sendMessage(descriptor, responseMessage);
    }
    return rooms;
}