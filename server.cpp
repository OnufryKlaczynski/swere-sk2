#include <thread>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>
#include <poll.h>
#include <error.h>
#include <mutex>

#include "lib/json.hpp"
#include "Room.cpp"
#include "utils.cpp"

#include <algorithm>

#define MAX_SIZE 1000

using json = nlohmann::json;

const int QUEUE_SIZE = 10;
pollfd *clientConnections;
std::mutex newClientMutex;
std::vector<std::string> users;
std::vector<int> clients;
std::map<std::string, Room> rooms;

void updateClientConnections()
{
    newClientMutex.lock();
    clientConnections = new pollfd[clients.size()]; //possible memory leak?? nie wiem nie znam się, działa XD
    int s = clients.size();
    for (int i = 0; i < s; ++i)
    {
        clientConnections[i].fd = clients[i];
        clientConnections[i].events = (POLLIN | POLLOUT | POLLRDHUP);
    }
    newClientMutex.unlock();
}

void acceptNewClients(int server_socket_descriptor, pollfd clientDescriptors[])
{
    int currentClientSize = 0;
    while (1)
    {
        // accept zwraca deskryptor pliku nawiązanego połączenia (czekając na to połączenie, jeśli żadnego nie ma w kolejce)
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientFd = accept(server_socket_descriptor, (sockaddr *)&clientAddr, &clientAddrLen);

        if (clientFd == -1)
        {
            perror("accept failed\n");
        }
        newClientMutex.lock();
        printf("Connection from %s:%hu\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        printf("Client descriptor %d\n\n", clientFd);
        clientDescriptors[currentClientSize].fd = clientFd;
        clients.push_back(clientFd);
        clientDescriptors[currentClientSize].events = (POLLIN | POLLOUT | POLLRDHUP);
        currentClientSize++;
        newClientMutex.unlock();
    }
}

void handleGame(int server_socket_descriptor, pollfd clientConnections[])
{
    Room game1{"room1", "hoscik"};
    Room game2{"room2", "hoscik"};
    Room game3{"room3", "hoscik"};

    std::vector<Room> roomsVect;
    roomsVect.push_back(game1);
    roomsVect.push_back(game2);
    roomsVect.push_back(game3);
    json roomsJson = roomsVect;

    rooms.insert(std::pair<std::string, Room>("room1", game1));
    rooms.insert(std::pair<std::string, Room>("room2", game2));
    rooms.insert(std::pair<std::string, Room>("room3", game3));

    while (1)
    {
        int gotowe = poll(clientConnections, QUEUE_SIZE, 100);

        if (gotowe == -1)
        {
            error(1, errno, "poll failed");
        }
        for (int i = 0; i < clients.size(); i++)
        {
            pollfd &opis = clientConnections[i];
            if (opis.revents & POLLIN)
            {
                char buf[1000] = "";
                int clientFd = opis.fd;
                int read_result = read(clientFd, buf, sizeof(buf));
                if (read_result == 0)
                {
                    std::cout << "Descriptor disconected:" << clientFd << "\n\n";
                    clients.erase(std::remove(clients.begin(), clients.end(), clientConnections[i].fd), clients.end());
                    std::string nick = users[i];
                    for (std::string roomName : extract_keys(rooms))
                    {
                        Room &room = rooms.at(roomName);
                        const auto roomNickSize = room.nicks.size();
                        room.nicks.erase(std::remove(room.nicks.begin(), room.nicks.end(), nick), room.nicks.end());
                        room.usersDescriptors.erase(std::remove(room.usersDescriptors.begin(), room.usersDescriptors.end(), clientFd), room.usersDescriptors.end());
                        room.userLettersMap.erase(nick);
                        room.userWrongCounterMap.erase(nick);
                        if (room.nicks.size() != roomNickSize)
                        {
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
                            break;
                        }
                    }

                    shutdown(clientConnections[i].fd, SHUT_RDWR);
                    close(clientConnections[i].fd);
                }
                if (read_result > 0)
                {
                    std::string type;
                    json messageFromClient;
                    try
                    {
                        std::string messageFromClientStr(buf);
                        messageFromClient = json::parse(messageFromClientStr);
                        type = (std::string)messageFromClient["type"];
                    }
                    catch (...)
                    {
                        std::cout << "Error while parsing json \n\n ";
                        continue;
                    }

                    if (type == "LOGIN")
                    {

                        users.push_back((std::string)messageFromClient["nick"]);
                        std::cout << "Użytkownik " << (std::string)messageFromClient["nick"] << "został dopisany do listy użytkowników" << std::endl
                                  << std::endl;
                        std::cout << "Message from client:\n\n"
                                  << messageFromClient.dump() << std::endl;
                        json responseMessage = R"(
                                                    {
                                                        "type": "USER_AUTHENTICATED",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
                        responseMessage["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));
                        sendMessage(clientFd, responseMessage);
                    }
                    else if (type == "CREATE_ROOM")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string hostNick = (std::string)messageFromClient["nick"];
                        Room newRoom(roomName, hostNick);
                        rooms.insert(std::pair<std::string, Room>(roomName, newRoom));
                        Room &room = rooms.at(roomName);
                        room.nicks.push_back(hostNick);
                        room.usersDescriptors.push_back(clientFd);
                        std::set<std::string> emptySet = std::set<std::string>();
                        room.userLettersMap.insert(std::pair<std::string, std::set<std::string>>(hostNick, emptySet));
                        room.userWrongCounterMap.insert(std::pair<std::string, int>(hostNick, 0));
                        json responseMessage = R"(
                           {
                             "type": "ROOM_CREATED",
                              "result": "SUCCESS"
                               }
                            )"_json;
                        responseMessage["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));
                        for (int descriptor : clients)
                        {
                            sendMessage(descriptor, responseMessage);
                        }
                        responseMessage = R"(
                                                    {
                                                        "type": "USER_JOINED_ROOM",
                                                        "result": "SUCCESS"
                                                    }
                                                )"_json;
                        responseMessage["otherPlayersInRoom"] = vectorToJson(room.nicks);
                        sendMessage(clientFd, responseMessage);
                    }
                    else if (type == "JOIN_ROOM")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string nick = (std::string)messageFromClient["nick"];
                        Room &room = rooms.at(roomName);
                        room.nicks.push_back(nick);
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
                    else if (type == "LEAVE_ROOM")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string nick = (std::string)messageFromClient["nick"];
                        Room &room = rooms.at(roomName);
                        room.nicks.erase(std::remove(room.nicks.begin(), room.nicks.end(), nick), room.nicks.end());
                        room.usersDescriptors.erase(std::remove(room.usersDescriptors.begin(), room.usersDescriptors.end(), clientFd), room.usersDescriptors.end());
                        room.userLettersMap.erase(nick);
                        room.userWrongCounterMap.erase(nick);
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
                    }
                    else if (type == "START_GAME")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        Room &room = rooms.at(roomName);
                        room.isGameStarted = true;
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
                    }
                    else if (type == "SEND_LETTER")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string letter = (std::string)messageFromClient["letter"];
                        std::string nick = (std::string)messageFromClient["nick"];

                        Room &room = rooms.at(roomName);
                        json responseMessage;
                        if (room.userWrongCounterMap[nick] == 10)
                        {
                            if (std::find(room.losers.begin(), room.losers.end(), nick) == room.losers.end())
                                room.losers.push_back(nick);
                            if (room.losers.size() == room.userWrongCounterMap.size())
                            {

                                responseMessage = R"(
                                {
                                    "type": "YOU_LOST",
                                    "result": "SUCCESS"
                                }
                            )"_json;
                                responseMessage["loser"] = nick;
                                sendMessage(clientFd, responseMessage);

                                room.gameFinished = true;
                                for (int descriptor : room.usersDescriptors)
                                {
                                    responseMessage = R"(
                                        {
                                            "type": "GAME_FINISHED",
                                            "result": "SUCCESS"
                                        }
                                    )"_json;
                                    responseMessage["winner"] = room.wordToFind;
                                    sendMessage(descriptor, responseMessage);
                                }
                                //rooms.erase(roomName);
                                continue;
                            }
                            responseMessage = R"(
                                {
                                    "type": "YOU_LOST",
                                    "result": "SUCCESS"
                                }
                            )"_json;
                            responseMessage["loser"] = nick;
                            sendMessage(clientFd, responseMessage);

                            continue;
                        }
                        std::vector<size_t> lettersPositions = room.guessLetter(nick, letter);
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
                                sendMessage(descriptor, responseMessage);
                            }
                            continue;
                        }
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
                }
            }
        }
        updateClientConnections();
    }
}

int main(int argc, char **argv)
{
    const int serverPort = 1111;
    const int reuse_addr_val = 1;

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(serverPort);

    //create socket
    int server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_descriptor < 0)
    {
        fprintf(stderr, "Error while creating a socket...\n");
        exit(1);
    }
    setsockopt(server_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr_val, sizeof(reuse_addr_val));

    //bind socket
    int bind_result = bind(server_socket_descriptor, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    if (bind_result < 0)
    {
        perror("Error while assigning ip addres and port for socket...\n");
        exit(1);
    }

    //set mode to listen
    int listen_result = listen(server_socket_descriptor, QUEUE_SIZE);
    if (listen_result < 0)
    {
        perror("Error while setting up queue size...\n");
        exit(1);
    }

    clientConnections = (pollfd *)malloc(sizeof(pollfd));

    std::thread acceptClientsThread(acceptNewClients, server_socket_descriptor, clientConnections);
    std::thread handleGameThread(handleGame, server_socket_descriptor, clientConnections);

    acceptClientsThread.join();
    handleGameThread.join();
    return 0;
}
