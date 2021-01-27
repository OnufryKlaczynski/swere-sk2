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
#include <chrono>

#include "lib/json.hpp"
#include "Room.cpp"
#include "utils.cpp"
#include "LoginHandler.cpp"
#include "GameHandler.cpp"
#include "RoomHandler.cpp"

#include <algorithm>

#define MAX_SIZE 1000

using json = nlohmann::json;

const int QUEUE_SIZE = 10;
pollfd *clientConnections = nullptr;
std::mutex newClientMutex;
std::vector<std::string> users;
std::vector<int> clients;
std::map<std::string, Room> rooms;
const int MAX_PLAYERS_IN_ROOM = 2;
int xd = 0;

void updateClientConnections()
{
    newClientMutex.lock();

    delete clientConnections;

    clientConnections = new pollfd[clients.size()];
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
        currentClientSize = clients.size();
        clientDescriptors[currentClientSize].fd = clientFd;
        clients.push_back(clientFd);
        clientDescriptors[currentClientSize].events = (POLLIN | POLLOUT | POLLRDHUP);
        newClientMutex.unlock();
    }
}

void sendInfoAboutRoomsToAll()
{
    json responseMessage = R"(
                           {
                             "type": "SERVERS_INFO",
                              "result": "SUCCESS"
                               }
                            )"_json;
    responseMessage["rooms"] = sendOnlyNotStartedRooms(extract_values(rooms));
    for (int descriptor : clients)
    {
        sendMessage(descriptor, responseMessage);
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

    while (1)
    {
        rooms = checkRoomTimers(rooms);

        int gotowe = poll(clientConnections, QUEUE_SIZE, 100);

        if (gotowe == -1)
        {
            error(1, errno, "poll failed");
        }
        int size = clients.size();
        for (int i = 0; i < size; i++)
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
                    std::string nick = users[i];
                    users.erase(std::remove(users.begin(), users.end(), nick), users.end());
                    clients.erase(std::remove(clients.begin(), clients.end(), clientConnections[i].fd), clients.end());
                    clientConnections[i] = clientConnections[clients.size()];
                    shutdown(clientFd, SHUT_RDWR);
                    close(clientFd);
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
                        users = claimUser(messageFromClient, clientFd, sendOnlyNotStartedRooms(extract_values(rooms)), users);
                    }
                    else if (type == "CREATE_ROOM")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string hostNick = (std::string)messageFromClient["nick"];
                        rooms = createRoom(roomName, hostNick, clientFd, rooms, clients);
                    }
                    else if (type == "JOIN_ROOM")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string nick = (std::string)messageFromClient["nick"];
                        Room &room = rooms.at(roomName);
                        if (room.nicks.size() >= MAX_PLAYERS_IN_ROOM)
                        {
                            json responseMessage = R"(
                                                    {
                                                        "type": "USER_JOINED_ROOM",
                                                        "result": "ERROR"
                                                    }
                                                )"_json;
                            sendMessage(clientFd, responseMessage);
                            continue;
                        }
                        joinRoom(nick, clientFd, room, MAX_PLAYERS_IN_ROOM);
                        sendInfoAboutRoomsToAll();
                    }
                    else if (type == "LEAVE_ROOM")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string nick = (std::string)messageFromClient["nick"];
                        int position = find(users.begin(), users.end(), nick) - users.begin();
                        rooms = leaveRoom(nick, roomName, clients, position, rooms, MAX_PLAYERS_IN_ROOM, false);
                        sendInfoAboutRoomsToAll();
                    }
                    else if (type == "LEAVE_GAME")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string nick = (std::string)messageFromClient["nick"];
                        Room &room = rooms.at(roomName);
                        if (room.nicks.empty())
                        {
                            rooms.erase(roomName);
                            continue;
                        }
                        rooms = leaveGame(nick, roomName, clientFd, rooms);
                        sendInfoAboutRoomsToAll();
                    }
                    else if (type == "LEAVE_LOBBY")
                    {
                    }
                    else if (type == "LEAVE_HOSTROOM")
                    {
                        std::string nick = users[i];
                        std::string roomName = (std::string)messageFromClient["roomName"];

                        Room &room = rooms.at(roomName);
                        int position = find(users.begin(), users.end(), nick) - users.begin();
                        const auto roomNickSize = room.nicks.size();
                        room.nicks.erase(std::remove(room.nicks.begin(), room.nicks.end(), nick), room.nicks.end());
                        room.usersDescriptors.erase(std::remove(room.usersDescriptors.begin(), room.usersDescriptors.end(), clientFd), room.usersDescriptors.end());
                        room.userLettersMap.erase(nick);
                        room.userWrongCounterMap.erase(nick);
                        if (room.nicks.size() != roomNickSize)
                        {
                            rooms = leaveRoom(nick, roomName, clients, position, rooms, MAX_PLAYERS_IN_ROOM, true);
                            break;
                        }
                        sendInfoAboutRoomsToAll();
                    }
                    else if (type == "START_GAME")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        rooms = startGame(rooms, roomName, clients);
                    }
                    else if (type == "SEND_LETTER")
                    {
                        std::string roomName = (std::string)messageFromClient["roomName"];
                        std::string letter = (std::string)messageFromClient["letter"];
                        std::string nick = (std::string)messageFromClient["nick"];

                        Room &room = rooms.at(roomName);
                        int lost = handleLosers(room, nick, roomName, letter, clientFd);

                        if (lost == 1)
                        {
                            rooms.erase(roomName);
                            continue;
                        }
                        else if (lost == 2)
                            continue;

                        handleLetter(room, nick, roomName, letter, clientFd);
                        if (room.gameFinished)
                        {
                            rooms.erase(roomName);
                            continue;
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

    clientConnections = new pollfd[0];

    std::thread acceptClientsThread(acceptNewClients, server_socket_descriptor, clientConnections);
    std::thread handleGameThread(handleGame, server_socket_descriptor, clientConnections);

    acceptClientsThread.join();
    handleGameThread.join();
    return 0;
}
