#pragma once
#include <string>
#include <vector>
#include <set>

#include "lib/json.hpp"
#include "utils.cpp"

using json = nlohmann::json;

class Room
{
public:
    std::string roomName;
    std::string hostNick;
    std::vector<std::string> nicks;
    std::vector<int> usersDescriptors;
    std::map<std::string, std::set<std::string>> userLettersMap;
    bool isGameStarted;
    bool gameFinished;
    std::string wordToFind;

    Room(std::string roomName, std::string hostNick)
    {
        this->roomName = roomName;
        this->hostNick = hostNick;
        this->wordToFind = "kurwa";
        this->isGameStarted = false;
        this->gameFinished = false;
        this->userLettersMap = std::map<std::string, std::set<std::string>>();
    }

    std::vector<size_t> guessLetter(std::string userNick, std::string letter)
    {
        std::set<std::string> &guessedLetters = this->userLettersMap.at(userNick);
        guessedLetters.insert(letter);
        auto found = wordToFind.find(letter);
        checkIfGameHasFinished(guessedLetters);

        return findAllStringPositions(this->wordToFind, letter);
    }

private:
    bool checkIfGameHasFinished(std::set<std::string> guessedLetters)
    {   
        bool gameFinished = true;
        for (char const &c : this->wordToFind)
        {
            std::string str(1, c);
            auto it = guessedLetters.find(str);
            if (it == guessedLetters.end())
            {
                gameFinished = false;
                break;
            }
        }
        this->gameFinished = gameFinished;
    }
};

void to_json(json &j, const Room &g)
{
    j = json{{"roomName", g.roomName}, {"isGameStarted", g.isGameStarted}, {"hostNick", g.hostNick}};
}

void from_json(const json &j, Room &g)
{
    j.at("roomName").get_to(g.roomName);
}
