#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <fcntl.h>

#include "RadioStation.hpp"

#define CLEAR_SCREEN "\033[2J\033[H"

class UIHandler {
public:
    static std::vector<RadioStation> radioStations;
    static int selectedStationIndex;
    static std::mutex selectionMutex;
    static std::vector<int> clientSockets;
    static int pipefd[2];


    UIHandler() {
        enableRawMode();
    }

    ~UIHandler() {
        disableRawMode();
    }

    static void displayRadioUI(int clientSocket) {
        std::string uiOutput = generateTelnetUIOutput();
        send(clientSocket, uiOutput.c_str(), uiOutput.length(), 0);
    }

    static void moveSelectionUp(int clientSocket) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        if (selectedStationIndex > 0) {
            --selectedStationIndex;
            notifySelectedStationChanged(selectedStationIndex);
            broadcastUIUpdate();
        }
    }

    static void moveSelectionDown(int clientSocket) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        if (selectedStationIndex < radioStations.size() - 1) {
            ++selectedStationIndex;
            notifySelectedStationChanged(selectedStationIndex);
            broadcastUIUpdate();
        }
    }

    static char readKey() {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1)
            std::cerr << "Failed to read input character" << std::endl;
        return c;
    }

    static void enableRawMode() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    static void disableRawMode() {
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
            output += radioStations[i].name + "\r\n";
        }

        output += "------------------------------------------------------------------------\r\n";

        return output;
    }

    static bool doesRadioStationExist(RadioStation& radioStation) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        bool res = std::find(radioStations.begin(), radioStations.end(), radioStation) != radioStations.end();
        if (res) {
            auto it = std::find(radioStations.begin(), radioStations.end(), radioStation);
            it->last_received_lookup_seconds = time(NULL);
        }
        return res;
    }

    static RadioStation getRadioStation(int index) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        if (index >= 0 && index < radioStations.size()) {
            return radioStations[index];
        } else {
            return RadioStation();
        }
    }

    static std::string trim(const std::string& str) {
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
                break;
            } else if (bytesRead == 0) {
                break;
            }

            command += buffer;

            command = trim(command);
            if (!command.empty()) {
                if (command == "exit") {
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

    static void notifySelectedStationChanged(int selectedStationIndex) {
        // std::string message = "selected " + std::to_string(selectedStationIndex);
        if (write(pipefd[1], &selectedStationIndex, sizeof(selectedStationIndex)) == -1) {
            exit(EXIT_FAILURE);
        }
    }

    static void broadcastUIUpdate() {
        std::string uiOutput = generateTelnetUIOutput();

        // std::lock_guard<std::mutex> lock(selectionMutex);
        for (int clientSocket : clientSockets) {
            send(clientSocket, uiOutput.c_str(), uiOutput.length(), 0);
        }
        notifySelectedStationChanged(selectedStationIndex);
    }

    static void runTelnetServer(uint32_t port) {
        pipe(UIHandler::pipefd);
        // Create a socket for the server
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            return;
        }

        // Set up the server address
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(port); // Telnet port

        // Bind the socket to the server address
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
            close(serverSocket);
            return;
        }

        // Listen for incoming connections
        if (listen(serverSocket, SOMAXCONN) == -1) {
            close(serverSocket);
            return;
        }

        while (true) {
            // Accept incoming connection
            sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);
            int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
            if (clientSocket == -1) {
                continue;
            }

            std::string clientIP = inet_ntoa(clientAddress.sin_addr);
            write(clientSocket, "\377\375\042\377\373\001", 6);

            // Create a new thread to handle the Telnet connection
            std::thread telnetThread(handleTelnetClient, clientSocket);
            telnetThread.detach(); // Detach the thread and let it run independently
        }

        close(pipefd[0]);
        close(pipefd[1]);
        close(serverSocket); // Close the server socket
    }

    static void addRadioStation(const RadioStation& radioStation) {
        std::lock_guard<std::mutex> lock(selectionMutex);
        radioStations.push_back(radioStation);
        RadioStation current = radioStations[selectedStationIndex];
        std::sort(radioStations.begin(), radioStations.end(), RadioStation::compareByName);
        selectedStationIndex = std::find(radioStations.begin(), radioStations.end(), current) - radioStations.begin();
        broadcastUIUpdate();
    }

    static void removeInactiveRadioStations() {
        auto currentTime = std::chrono::system_clock::now();

        for (auto it = radioStations.begin(); it != radioStations.end();) {
            auto lastLookupTime = std::chrono::time_point<std::chrono::system_clock>(std::chrono::seconds(it->last_received_lookup_seconds));
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastLookupTime).count();

            if (elapsedTime >= 20) {
                std::lock_guard<std::mutex> lock(selectionMutex);
                it = radioStations.erase(it);
                selectedStationIndex = 0;
                RadioStation current = radioStations[selectedStationIndex];
                std::sort(radioStations.begin(), radioStations.end(), RadioStation::compareByName);
                selectedStationIndex = std::find(radioStations.begin(), radioStations.end(), current) - radioStations.begin();
                broadcastUIUpdate();
            } else {
                ++it;
            }
        }
    }

};

std::vector<RadioStation> UIHandler::radioStations = {};
int UIHandler::selectedStationIndex = 0;
std::mutex UIHandler::selectionMutex;
std::vector<int> UIHandler::clientSockets;
int UIHandler::pipefd[2];
