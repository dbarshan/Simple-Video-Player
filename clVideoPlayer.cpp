#include <iostream>
#include "Player.h"

int main() {

	std::string filename = "C:\\users\\bdas\\videos\\Wildlife.wmv";

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		std::cout << "Could not initialize SDL - " << SDL_GetError();
		return 1;
	}

	Player *player = new Player();
	player->openFile(filename);
	player->getInfo();
	int ret = player->allocateMemory();
	//if (ret < 0) {
	//	std::cout << "Error: Could not allocate player memory "<< std::endl;
	//	delete(player);
	//	return -1;
	//}
	ret = player->createDisplay();
	player->play();

	//ret = player->getFrames();

	
	delete(player);
	std::cout << "Success " << std::endl;
	return 0;

}