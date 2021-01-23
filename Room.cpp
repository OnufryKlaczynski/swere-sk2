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
    std::vector<std::string> losers;
    std::vector<int> usersDescriptors;
    std::map<std::string, std::set<std::string>> userLettersMap;
    bool isGameStarted;
    bool gameFinished;
    std::string wordToFind;
    std::map<std::string, int> userWrongCounterMap;

    Room(std::string roomName, std::string hostNick)
    {
        this->roomName = roomName;
        this->hostNick = hostNick;
        this->wordToFind = "kurwa";
        this->isGameStarted = false;
        this->gameFinished = false;
        this->userLettersMap = std::map<std::string, std::set<std::string>>();
        this->userWrongCounterMap = std::map<std::string, int>();
    }

    std::vector<size_t> guessLetter(std::string userNick, std::string letter)
    {
        std::set<std::string> &guessedLetters = this->userLettersMap.at(userNick);
        guessedLetters.insert(letter);
        auto found = wordToFind.find(letter);
        checkIfGameHasFinished(guessedLetters);

        std::vector<size_t> positions = findAllStringPositions(this->wordToFind, letter);
        if (positions.size() == 0)
        {
            this->userWrongCounterMap.at(userNick)++;
        }
        return positions;
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
    j = json{
        {"roomName", g.roomName},
        {"isGameStarted", g.isGameStarted},
        {"hostNick", g.hostNick},
        {"gameFinished", g.gameFinished},
        {"userWrongCounterMap", g.userWrongCounterMap}};
}

void from_json(const json &j, Room &g)
{
    j.at("roomName").get_to(g.roomName);
}

std::vector<Room> sendOnlyNotStartedRooms(std::vector<Room> games)
{
  auto it = games.begin();
  while (it != games.end())
  {
    auto curr = it++;
    if (curr->roomName.empty())
      return games;
    if (curr->isGameStarted)
    {
      games.erase(curr);
    }
  }
  return games;
}