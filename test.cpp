#include <assert.h> 
#include <vector>

#include "utils.cpp"
#include "Room.cpp"


void shouldSendOnlyNotStaretedRooms() {
    Room game1{"room1", "hoscik"};
    Room game2{"room2", "hoscik"};
    Room game3{"room3", "hoscik"};
    game1.isGameStarted = true;

    std::vector<Room> roomsVect;
    roomsVect.push_back(game1);
    roomsVect.push_back(game2);
    roomsVect.push_back(game3);

    std::vector<Room> clearedRooms = sendOnlyNotStartedRooms(roomsVect);
    assert(clearedRooms.size() == 2);
}

int main(int argc, char **argv)
{ 
    shouldSendOnlyNotStaretedRooms();

}

