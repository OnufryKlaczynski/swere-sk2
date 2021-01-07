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

#include "lib/json.hpp"
#include "Room.cpp"
#include "utils.cpp"

#define MAX_SIZE 1000

using json = nlohmann::json;

int main(int argc, char **argv)
{

    std::vector<std::string> users;
    std::map<std::string, Room> rooms;

    int serverPort = 1111;
    int reuse_addr_val = 1;
    int QUEUE_SIZE = 2;
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

    // accept zwraca deskryptor pliku nawiązanego połączenia (czekając na to połączenie, jeśli żadnego nie ma w kolejce)
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientFd = accept(server_socket_descriptor, (sockaddr *)&clientAddr, &clientAddrLen);

    if (clientFd == -1)
    {
        perror("accept failed\n");
        return 1;
    }
    printf("Connection from %s:%hu\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    while (1)
    {
        char buf[1000] = " ";
        int read_result = read(clientFd, buf, sizeof(buf));
        if (read_result >= 0)
        {
            std::string messageFromClientStr(buf);
            json messageFromClient = json::parse(messageFromClientStr);
            std::string type = (std::string)messageFromClient["type"];
            if (type == "LOGIN")
            {
                // ---- TOO JESZCZE SIĘ PRZYDA
                // users.push_back("siemanko");
                // users.push_back("tutaj");
                // users.push_back("użytkowniki");

                // auto jsonObjects = json::array();
                // for (const std::string &str : users)
                // {
                //     jsonObjects.push_back(str);
                // }
                // std::string coTuKurwawyszlo = jsonObjects.dump();

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

                std::cout << "Użytkownik się kurwa próbował zalogować japierdole kill me plz...." << (std::string)messageFromClient["nick"] << std::endl
                          << std::endl;
                users.push_back((std::string)messageFromClient["nick"]);
                std::cout << "Użytkownik" << (std::string)messageFromClient["nick"] << "został dopisany do listy użytkowników" << std::endl
                          << std::endl;
                std::cout << "Message from client:\n\n"
                          << messageFromClientStr << std::endl;
                json responseMessage = R"(
                            {
                                "type": "USER_AUTHENTICATED",
                                "result": "SUCCESS"
                            }
                        )"_json;
                responseMessage["rooms"] = roomsJson;
                sendMessage(clientFd, responseMessage);
            }
            else if (type == "CREATE_ROOM")
            {
                std::string roomName = (std::string)messageFromClient["roomName"];
                std::string hostNick = (std::string)messageFromClient["nick"];
                Room newRoom(roomName, hostNick);
                //TODO: add HOST
                rooms.insert(std::pair<std::string, Room>(roomName, newRoom));
            }
            else if (type == "JOIN_ROOM")
            {
                std::string roomName = (std::string)messageFromClient["roomName"];
                std::string nick = (std::string)messageFromClient["nick"];
                Room& room = rooms.at(roomName);
                room.nicks.push_back(nick);
                std::set<std::string> emptySet = std::set<std::string>();
                room.userLettersMap.insert(
                    std::pair<std::string, std::set<std::string>>(nick, emptySet));
                json responseMessage = R"(
                    {
                        "type": "USER_JOINED_ROOM",
                        "result": "SUCCESS"
                    }
                )"_json;
                sendMessage(clientFd, responseMessage);
            }
            else if (type == "START_GAME")
            {
                std::string roomName = (std::string)messageFromClient["roomName"];
                Room room = rooms.at(roomName);
                room.isGameStarted = true;
                json responseMessage = R"(
                    {
                        "type": "GAME_STARTED",
                        "result": "SUCCESS"
                    }
                )"_json;
                responseMessage["howLongIsTheWord"] = room.wordToFind.size();
                sendMessage(clientFd, responseMessage);
            }
            else if (type == "SEND_LETTER")
            {
                std::string roomName = (std::string)messageFromClient["roomName"];
                std::string letter = (std::string)messageFromClient["letter"];
                std::string nick = (std::string)messageFromClient["nick"];

                Room &room = rooms.at(roomName);
                if (room.gameFinished)
                {
                    //SEND MESSAGE TO EVERYONE HIHI;
                    continue;
                }
                std::vector<size_t> lettersPositions = room.guessLetter(nick, letter);
                json responseMessage = R"(
                    {
                        "type": "LETTER_RECEIVED",
                        "result": "SUCCESS"
                    }
                )"_json;
                responseMessage["letterPositions"] = vectorToJson(lettersPositions);
                responseMessage["gameFinished"] = room.gameFinished;

                

                sendMessage(clientFd, responseMessage);
            }
        }
    }

    return 0;
}
