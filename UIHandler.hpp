#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>

#define CLEAR_SCREEN "\033[2J\033[H"

class UIHandler {
private:
    static std::vector<std::string> radioStations;
    static int selectedStationIndex;
    static std::mutex selectionMutex;
    static std::vector<int> clientSockets;

public:
    UIHandler() {
        enableRawMode();
    }

    ~UIHandler() {
        disableRawMode();
    }

    void displayRadioUI(int clientSocket) {
        std::string uiOutput = generateTelnetUIOutput();
        send(clientSocket, uiOutput.c_str(), uiOutput.length(), 0);
    }

    void moveSelectionUp(int clientSocket) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        if (selectedStationIndex > 0)
            --selectedStationIndex;
        broadcastUIUpdate();
    }

    void moveSelectionDown(int clientSocket) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        if (selectedStationIndex < radioStations.size() - 1)
            ++selectedStationIndex;
        broadcastUIUpdate();
    }

    char readKey() {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1)
            std::cerr << "Failed to read input character" << std::endl;
        return c;
    }

    void enableRawMode() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    void disableRawMode() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag |= (ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    static std::string generateTelnetUIOutput() {
        std::string output;

        output += CLEAR_SCREEN;
        output += "------------------------------------------------------------------------\r\n";
        output += " SIK Radio\r\n";
        output += "------------------------------------------------------------------------\r\n";

        for (int i = 0; i < radioStations.size(); ++i) {
            if (i == selectedStationIndex)
                output += " > ";
            else
                output += "   ";
            output += radioStations[i] + "\r\n";
        }

        output += "------------------------------------------------------------------------\r\n";

        return output;
    }

    static void handleCommand(const std::string& command) {
        if (command == "next") {
            std::lock_guard<std::mutex> lock(selectionMutex);
            if (selectedStationIndex < radioStations.size() - 1)
                ++selectedStationIndex;
            broadcastUIUpdate();
        } else if (command == "prev") {
            std::lock_guard<std::mutex> lock(selectionMutex);
            if (selectedStationIndex > 0)
                --selectedStationIndex;
            broadcastUIUpdate();
        }
    }

    std::string static trim(const std::string& str) {
        // Find the first non-whitespace character
        auto start = str.find_first_not_of(" \t\r\n");

        // If the string is empty or contains only whitespace, return an empty string
        if (start == std::string::npos)
            return "";

        // Find the last non-whitespace character
        auto end = str.find_last_not_of(" \t\r\n");

        // Return the trimmed substring
        return str.substr(start, end - start + 1);
    }

    static void handleTelnetClient(int clientSocket) {
        char buffer[1024];
        std::string command;

        UIHandler uiHandler;
        uiHandler.displayRadioUI(clientSocket);

        // std::lock_guard<std::mutex> lock(selectionMutex);
        clientSockets.push_back(clientSocket);

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead == -1) {
                std::cerr << "Failed to read data from client" << std::endl;
                break;
            } else if (bytesRead == 0) {
                std::cout << "Telnet client disconnected" << std::endl;
                break;
            }

            command += buffer;

            command = trim(command);
            if (!command.empty()) {
                if (command == "exit") {
                    std::cout << "Telnet client requested to exit" << std::endl;
                    break;
                } else if (command == "\033[A") { // Up arrow key
                    uiHandler.moveSelectionUp(clientSocket);
                    broadcastUIUpdate();
                } else if (command == "\033[B") { // Down arrow key
                    uiHandler.moveSelectionDown(clientSocket);
                    broadcastUIUpdate();
                }

                command.clear();
            }
        }

        {
            std::lock_guard<std::mutex> lock(selectionMutex);
            clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
        }

        close(clientSocket);
    }

    void static broadcastUIUpdate() {
        std::string uiOutput = generateTelnetUIOutput();

        // std::lock_guard<std::mutex> lock(selectionMutex);
        for (int clientSocket : clientSockets) {
            send(clientSocket, uiOutput.c_str(), uiOutput.length(), 0);
        }
    }

    void static runTelnetServer() {
        // Create a socket for the server
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }

        // Set up the server address
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(23); // Telnet port

        // Bind the socket to the server address
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
            std::cerr << "Failed to bind socket to address" << std::endl;
            close(serverSocket);
            return;
        }

        // Listen for incoming connections
        if (listen(serverSocket, SOMAXCONN) == -1) {
            std::cerr << "Failed to listen for connections" << std::endl;
            close(serverSocket);
            return;
        }

        std::cout << "Telnet server is running. Waiting for connections..." << std::endl;

        while (true) {
            // Accept incoming connection
            sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);
            int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
            if (clientSocket == -1) {
                std::cerr << "Failed to accept incoming connection" << std::endl;
                continue;
            }

            std::string clientIP = inet_ntoa(clientAddress.sin_addr);
            write(clientSocket, "\377\375\042\377\373\001", 6);
            std::cout << "New Telnet connection from " << clientIP << std::endl;

            // Create a new thread to handle the Telnet connection
            std::thread telnetThread(handleTelnetClient, clientSocket);
            telnetThread.detach(); // Detach the thread and let it run independently
        }

        close(serverSocket); // Close the server socket
    }
};

std::vector<std::string> UIHandler::radioStations = {
    "PR1",
    "Radio \"357\"",
    "Radio \"Disco Pruszkow\""
};
int UIHandler::selectedStationIndex = 0;
std::mutex UIHandler::selectionMutex;
std::vector<int> UIHandler::clientSockets;
