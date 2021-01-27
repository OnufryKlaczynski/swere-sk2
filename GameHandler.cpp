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

std::map<std::string, Room> startGame(std::map<std::string, Room> rooms, std::string roomName, std::vector<int> clients)
{
    Room &room = rooms.at(roomName);
    room.isGameStarted = true;
    room.start_time = std::chrono::system_clock::now();

    json responseMessage = R"(
                                                    {
                                                        "type": "GAME_STARTED",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
    responseMessage["howLongIsTheWord"] = room.wordToFind.size();
    responseMessage["roomName"] = roomName;

    for (int descriptor : room.usersDescriptors)
    {
        sendMessage(descriptor, responseMessage);
    }
    usleep(20);
    json responseMessage2 = R"(
                            {
                                "type": "BLOCK_ROOM",
                                "result": "SUCCESS"
                            }
                        )"_json;
    responseMessage2["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));
    for (int descriptor : clients)
    {
        sendMessage(descriptor, responseMessage2);
    }
    return rooms;
}

void handleLetter(Room &room, std::string nick, std::string roomName, std::string letter, int clientFd)
{
    json responseMessage;
    std::vector<size_t> lettersPositions = room.guessLetter(nick, letter);
    if (!lettersPositions.empty())
    {
        responseMessage = R"(
                                                {
                                                    "type": "LETTER_RECEIVED",
                                                    "result": "SUCCESS"
                                                }
                                            )"_json;
        responseMessage["letterPositions"] = vectorToJson(lettersPositions);
        responseMessage["gameFinished"] = room.gameFinished;
        responseMessage["letterGuessed"] = letter;
        sendMessage(clientFd, responseMessage);
    }
    else
    {
        for (int descriptor : room.usersDescriptors)
        {
            responseMessage = R"(
                                                {
                                                    "type": "SOMEBODY_GUESSED_WRONG"
                                                }
                                            )"_json;
            responseMessage["userWrongCounterMap"] = room.userWrongCounterMap;
            sendMessage(descriptor, responseMessage);
        }
    }
    if (room.gameFinished)
    {
        for (int descriptor : room.usersDescriptors)
        {
            responseMessage = R"(
                                        {
                                            "type": "GAME_FINISHED",
                                            "result": "SUCCESS"
                                        }
                                    )"_json;
            responseMessage["winner"] = nick;
            responseMessage["word"] = room.wordToFind;
            sendMessage(descriptor, responseMessage);
        }
    }
}

int handleLosers(Room &room, std::string nick, std::string roomName, std::string letter, int clientFd)
{
    json responseMessage;
    if (room.userWrongCounterMap[nick] == 10)
    {
        if (std::find(room.losers.begin(), room.losers.end(), nick) == room.losers.end())
            room.losers.push_back(nick);
        if (room.losers.size() == room.userWrongCounterMap.size())
        {
            room.gameFinished = true;
            for (int descriptor : room.usersDescriptors)
            {
                responseMessage = R"(
                                        {
                                            "type": "GAME_LOST",
                                            "result": "SUCCESS"
                                        }
                                    )"_json;
                responseMessage["word"] = room.wordToFind;
                sendMessage(descriptor, responseMessage);
            }
            return 1;
        }
        responseMessage = R"(
                                {
                                    "type": "YOU_LOST",
                                    "result": "SUCCESS"
                                }
                            )"_json;
        responseMessage["loser"] = nick;
        sendMessage(clientFd, responseMessage);
        return 2;
    }
    return 0;
}