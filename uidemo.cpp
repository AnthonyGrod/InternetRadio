#include "UIHandler.hpp"

#include <termios.h>

int main() {
    UIHandler uiHandler;
    
    uiHandler.runTelnetServer();
}