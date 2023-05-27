#include "UIHandler.hpp"

#include <termios.h>

int main() {
    UIHandler uiHandler;

    // Create a thread to run the runTelnetServer() function
    std::thread serverThread(&UIHandler::runTelnetServer);

    std::string radiostation1 = "Radio Maryja";
    std::string radiostation2 = "Radio Zet";
    std::string radiostation3 = "Radio Zlote Przeboje";

    // Add some radio stations
    uiHandler.addRadioStation(radiostation1);
    uiHandler.addRadioStation(radiostation2);
    uiHandler.addRadioStation(radiostation3);

    uiHandler.removeRadioStation(radiostation2);

    // Wait for the server thread to finish
    serverThread.join();

    return 0;
}