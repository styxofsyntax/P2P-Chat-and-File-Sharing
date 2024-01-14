#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <filesystem>
#include <numeric>
#include <thread>
#include <algorithm>
#include <chrono>
#include <fstream>

#include "cli.h"

using namespace std;

void Client::input_data()
{
    while (1)
    {
        cout << "Username: ";
        getline(cin, username);

        if (usernameAvail(username))
            break;

        cout << "Username already exists!\n";
    }

    while (1)
    {
        cout << "Enter a directory: ";
        getline(cin, dir);

        // Check if the entered directory is valid
        if (filesystem::is_directory(dir))
            break; // Exit the loop if a valid directory is entered
        else
            cout << "Invalid directory. Please try again.\n";
    }

    while (1)
    {
        cout << "Port: ";
        cin >> port;

        if (startChatServer())
            break;
    }
}

int Client::serverConnect()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("Socket creation failed : ");
        exit(-1);
    }

    struct sockaddr_in s_addr;
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(SERVER_PORT);
    inet_aton(SERVER_IP, &s_addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1)
    {
        perror("Connect failed on socket : ");
        exit(-1);
    }

    return fd;
}

Client::Client()
{
    input_data();
    chatSession = false;
    blockUI = false;
}

Client::Client(string username, string dir, int port)
{
    this->username = username;
    this->dir = dir;
    this->port = port;
    this->chatSession = false;
}

bool Client::serverRegister()
{
    int fd = serverConnect();

    if (!fetchFiles())
    {
        perror("File fetching failed : ");
        return false;
    }

    string data = "INIT," + username + ',' + dir + ',' + to_string(this->port) + ',' + filesToString();
    send(fd, data.c_str(), data.size(), 0);

    char buffer[1000];
    bzero(buffer, sizeof(buffer));

    if (recv(fd, buffer, 1000, 0) > 0)
    {
        vector<string> tokens = stringToTokens(buffer);

        if (strcmp(buffer, "OK") == 0)
            cout << ANSI_COLOR_GREEN "Registered with server!\n\n"
                 << ANSI_COLOR_RESET;
        else if (strcmp(buffer, "UP") == 0)
            cout << "Filenames updated on server!\n\n";
        else if (tokens.size() == 2 && tokens[0] == "ERR")
        {
            cout << tokens[1] << endl
                 << endl;
            close(fd);
            return false;
        }
        else
            cout << buffer << endl
                 << endl;
    }

    close(fd);
    return true;
}

void Client::serverExit()
{
    int fd = serverConnect();

    string data = "EXIT," + username;
    send(fd, data.c_str(), data.size(), 0);
    shutdown(fd, 2);
}

string Client::fetchPeerData(string p_username)
{
    int fd = serverConnect();

    char buffer[1000];
    bzero(buffer, sizeof(buffer));

    string data = "GET_P," + p_username;
    send(fd, data.c_str(), data.size(), 0);

    recv(fd, buffer, 1000, 0);
    close(fd);
    return buffer;
}

vector<string> Client::fetchUsernames()
{
    int fd = serverConnect();
    vector<string> tokens;
    char users[1000];
    bzero(users, sizeof(users));

    string data = "GET_U";
    send(fd, data.c_str(), data.size(), 0);

    if (recv(fd, users, 1000, 0) > 0)
    {
        tokens = stringToTokens(users);
        if (tokens[0] == "OK")
            tokens.erase(tokens.begin()); // remove "OK" from vector
        else
            cout << "Unexpected Error!\n\n";
    }
    close(fd);
    return tokens;
}

bool Client::usernameAvail(string name)
{
    if (name.find(',') != string::npos) // commas are not allowed in usernames as they are being used as delimeters
        return false;

    return !userExists(name);
}

bool Client::userExists(string name)
{
    vector<string> users = fetchUsernames();
    auto it = find(users.begin(), users.end(), name);
    return it != users.end();
}

void Client::updateFiles(const vector<string> &files)
{
    this->files = files;
}

string Client::filesToString()
{
    const char separator = ',';
    return accumulate(files.begin(), files.end(), string(),
                      [separator](const string &a, const string &b)
                      {
                          return a + (a.empty() ? "" : string(1, separator)) + b;
                      });
}

bool Client::fetchFiles()
{
    files.clear();

    try
    {
        for (const auto &entry : filesystem::directory_iterator(dir))
        {
            if (filesystem::is_regular_file(entry.path()))
                files.push_back(entry.path().filename());
        }
    }
    catch (const filesystem::filesystem_error &e)
    {
        cerr << "Error accessing the directory: " << e.what() << std::endl;
        return false;
    }
    return true;
}

string Client::fetchFilenamesFromServer()
{
    int fd = serverConnect();
    string data = "GET_AF";
    send(fd, data.c_str(), data.size(), 0);

    char buffer[1000];
    bzero(buffer, sizeof(buffer));

    recv(fd, buffer, 1000, 0);
    close(fd);
    return buffer;
}

string Client::fetchUserFilenamesFromServer(string username)
{
    int fd = serverConnect();
    string data = "GET_UF," + username;
    send(fd, data.c_str(), data.size(), 0);

    char buffer[1000];
    bzero(buffer, sizeof(buffer));

    recv(fd, buffer, 1000, 0);
    close(fd);
    return buffer;
}

vector<string> Client::getFiles()
{
    return files;
}

int Client::peerConnect(string p_ip, int p_port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("Socket creation failed : \n");
        return -1;
    }

    struct sockaddr_in p_addr;
    p_addr.sin_family = AF_INET;
    p_addr.sin_port = htons(p_port);
    inet_aton(p_ip.c_str(), &p_addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&p_addr, sizeof(p_addr)) == -1)
    {
        perror("Connect failed on socket : \n");
        close(fd);
        return -1;
    }
    return fd;
}

void Client::peerChat(string username)
{
    string data = fetchPeerData(username);
    vector<string> tokens = stringToTokens(data);

    if (tokens[0] == "ERR")
    {
        cout << "ERROR: " << tokens[1] << endl;
        return;
    }

    string p_ip = tokens[1];
    int p_port = stoi(tokens[2]);

    int fd = peerConnect(p_ip, p_port);

    if (fd < 0)
        return;

    chatSession = true;

    thread sendPeerThread([this, fd]()
                          { this->sendChatToPeer(fd); });
    thread recvThread([this, fd]()
                      { this->recvChatFromPeer(fd); });

    cout << ANSI_COLOR_GREEN << "Chat session opened with " << username << '\n'
         << ANSI_COLOR_RESET;

    string flag = "CHAT," + this->username;
    send(fd, flag.c_str(), flag.size(), 0);

    recvThread.join();
    sendPeerThread.join();
    close(fd);

    cout << ANSI_COLOR_YELLOW << "Chat session closed!\n\n"
         << ANSI_COLOR_RESET;
}

bool Client::startChatServer()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("Socket creation failed : ");
        return false;
    }

    struct sockaddr_in c_addr;
    c_addr.sin_addr.s_addr = INADDR_ANY;
    c_addr.sin_family = AF_INET;
    c_addr.sin_port = htons(port);

    cout << "listening on port: " << port << '\n';

    if (bind(fd, (struct sockaddr *)(&c_addr), sizeof(c_addr)) == -1)
    {
        perror("Bind failed on socket : ");
        return false;
    }

    int backlog = 4;
    if (listen(fd, backlog) == -1)
    {
        perror("listen failed on socket : ");
        return false;
    }

    cout << "Chat-Server started!\n\n";

    thread acceptThread([this, fd]()
                        { this->invokeAccept(fd); });
    acceptThread.detach();

    return true;
}

void Client::invokeAccept(int fd)
{
    struct sockaddr_in p_addr;
    socklen_t paddr_len = sizeof(p_addr);

    while (1)
    {
        int connfd = accept(fd, (struct sockaddr *)&p_addr, &paddr_len);

        if (connfd < 0)
        {
            perror("accept failed on socket : ");
        }
        else
        {
            cout << "Connection: {ip: " << inet_ntoa(p_addr.sin_addr) << " port: " << ntohs(p_addr.sin_port) << "}\n";

            char buffer[1000];

            bzero(buffer, sizeof(buffer));

            if (recv(connfd, buffer, 1000, 0) > 0)
            {
                vector<string> tokens = stringToTokens(buffer);

                if (tokens[0] == "CHAT")
                {
                    cout << ANSI_COLOR_CYAN << "Enter 9 to accept the chat request! (5 secs)\n"
                         << ANSI_COLOR_RESET;
                    chatSession = true;
                    auto startTime = std::chrono::steady_clock::now();
                    do
                    {
                        auto currentTime = std::chrono::steady_clock::now();
                        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

                        if (elapsedTime >= 5)
                        {
                            std::cout << ANSI_COLOR_RED << "Chat request denied.\n\n"
                                      << ANSI_COLOR_RESET;
                            shutdown(connfd, 2);
                            break;
                        }

                        // Sleep for a short duration to avoid busy waiting
                        this_thread::sleep_for(std::chrono::milliseconds(100));
                    } while (!blockUI); // check if user toggles off the blockUI to indicate accept

                    if (!blockUI) // if still off then decline
                    {
                        chatSession = false;
                        continue; // declines the chat request
                    }

                    cout << ANSI_COLOR_GREEN << "Chat session opened with " << tokens[1] << '\n'
                         << ANSI_COLOR_RESET;

                    thread recvChatThread = thread([this, connfd]()
                                                   { this->recvChatFromPeer(connfd); });
                    thread sendChatThread = thread([this, connfd]()
                                                   { this->sendChatToPeer(connfd); });
                    recvChatThread.join();
                    sendChatThread.join();
                    try
                    {
                        close(connfd);
                    }
                    catch (exception e)
                    {
                    }
                    cout << ANSI_COLOR_YELLOW << "Chat session closed!\n\n"
                         << ANSI_COLOR_RESET;
                    blockUI = false;
                }
                else if (tokens[0] == "FILE")
                {
                    thread sendFileThread([this, connfd, tokens]()
                                          { this->sendFileToPeer(connfd, tokens[1]); });
                    sendFileThread.detach();
                }
            }
        }
    }
}

void Client::recvFileFromPeer(string username, string filename)
{
    string data = fetchPeerData(username);
    vector<string> tokens = stringToTokens(data);

    if (tokens[0] == "ERR")
    {
        cout << "ERROR: " << tokens[1] << endl;
        return;
    }

    string p_ip = tokens[1];
    int p_port = stoi(tokens[2]);

    int fd = peerConnect(p_ip, p_port);

    if (fd < 0)
        return;

    char buffer[1000];
    ofstream outputFile(dir + '/' + filename, ios::binary);

    string flag = "FILE," + filename;
    send(fd, flag.c_str(), flag.size(), 0);

    ssize_t bytesRead;
    while ((bytesRead = recv(fd, buffer, 1000, 0)) > 0)
    {
        outputFile.write(buffer, bytesRead);
    }

    close(fd);
    outputFile.close();
    cout << "File received successfully.\n\n";
}

void Client::sendFileToPeer(int connfd, string filename)
{
    ifstream inputFile(dir + '/' + filename, ios::binary);
    if (!inputFile.is_open())
    {
        cout << "Error opening file\n\n";
        shutdown(connfd, 2);
        return;
    }

    char buffer[1000];
    ssize_t bytesRead;
    while ((bytesRead = inputFile.readsome(buffer, 1000)) > 0)
    {
        send(connfd, buffer, bytesRead, 0);
    }

    close(connfd);
    inputFile.close();

    cout << "File sent successfully.\n\n";
}

void Client::recvChatFromPeer(int connfd)
{
    char buffer[1000];

    while (chatSession)
    {
        bzero(buffer, sizeof(buffer));
        if (recv(connfd, buffer, 1000, 0) > 0)
        {
            cout << "Recieved: " << buffer << '\n';
        }
        else
        {
            if (chatSession)
                cout << ANSI_COLOR_RED << "Peer disconnected. Enter anything to exit!\n"
                     << ANSI_COLOR_RESET;
            chatSession = false;
            break;
        }
    }
}

void Client::sendChatToPeer(int connfd)
{
    char buffer[1000];
    while (chatSession)
    {
        cin.getline(buffer, 1000);

        if (strcmp(buffer, "!exit") == 0)
        {
            cout << "exiting...\n";
            chatSession = false;
            break;
        }

        if (send(connfd, buffer, strlen(buffer), 0) < 0)
            break;
    }
    shutdown(connfd, 2);
}

vector<string> stringToTokens(string str)
{
    istringstream ss(str);
    string token;
    vector<string> tokens;
    while (getline(ss, token, ','))
    {
        tokens.push_back(token);
    }
    return tokens;
}

void printVector(const vector<string> &v)
{
    for (const auto &element : v)
        cout << element << ", ";

    cout << '\n';
}