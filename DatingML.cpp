// DatingML.cpp : Defines the entry point for the console application.
//

#include "MainServer.h"

int main() {
	freopen("nul", "a", stderr);
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, ".");
	try {
		MainServer server(0);
		server.loop();
	} catch (std::exception exc) {
}
    return 0;
}
