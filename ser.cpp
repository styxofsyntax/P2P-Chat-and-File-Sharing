#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <sstream>
#include "ser.h"

using namespace std;

Server::Server(int port)
{
    this->port = port;
}

void Server::start()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("Socket creation failed : ");
        exit(-1);
    }

    struct sockaddr_in s_addr;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(80);

    if (bind(fd, (struct sockaddr *)(&s_addr), sizeof(s_addr)) == -1)
    {
        perror("Bind failed on socket : ");
        exit(-1);
    }

    int backlog = 4;
    if (listen(fd, backlog) == -1)
    {
        perror("listen failed on socket : ");
        exit(-1);
    }

    cout << "Server started!\n\n";

    thread acceptThread([this, fd]()
                        { this->invokeAccept(fd); });
    acceptThread.detach();
}

void Server::invokeAccept(int fd)
{
    struct sockaddr_in c_addr;
    socklen_t cliaddr_len = sizeof(c_addr);

    while (1)
    {
        int connfd = accept(fd, (struct sockaddr *)&c_addr, &cliaddr_len);
        if (connfd < 0)
        {
            perror("accept failed on socket : ");
            exit(-1);
        }
        else
        {
            client_ctx c_ctx;
            c_ctx.connfd = connfd;
            c_ctx.c_addr = c_addr;

            cout << ANSI_COLOR_GREEN << "Connection: {ip: " << inet_ntoa(c_addr.sin_addr)
                 << " port: " << ntohs(c_addr.sin_port) << "}"
                 << ANSI_COLOR_RESET << '\n';

            thread recvThread([this, c_ctx]()
                              { this->recvFromClient(c_ctx); });
            recvThread.detach();
        }
    }
}

void Server::recvFromClient(client_ctx c_ctx)
{
    struct sockaddr_in c_addr = c_ctx.c_addr;
    int connfd = c_ctx.connfd;

    char buffer[1000];

    while (1)
    {
        bzero(buffer, sizeof(buffer));
        if (recv(connfd, buffer, 1000, 0) > 0)
        {
            cout << "\n---Received--- \nSource: {ip: " << inet_ntoa(c_addr.sin_addr) << " port: " << ntohs(c_addr.sin_port) << "}\n";

            istringstream ss(buffer);
            string token;
            vector<string> tokens;
            while (getline(ss, token, ','))
            {
                tokens.push_back(token);
            }

            // Check if the message type is INIT
            cout << "Request: " << tokens[0] << '\n';
            if (tokens.size() >= 4 && tokens[0] == "INIT")
            {
                // Extract the values
                peer_data pdata;
                string data, username = tokens[1];
                pdata.dir = tokens[2];
                pdata.port = stoi(tokens[3]);

                // Store files in a vector
                pdata.files = vector<string>(tokens.begin() + 4, tokens.end());
                pdata.ip = inet_ntoa(c_addr.sin_addr);

                // If username is not present in database
                if (peers.find(username) == peers.end())
                    data = "OK"; // user registered
                else
                    data = "UP"; // data updated
                peers[username] = pdata;

                send(connfd, data.c_str(), data.size(), 0);
                printPeers();

                continue;
            }
            else if (tokens.size() == 2 && tokens[0] == "EXIT")
            {
                string exitUsername = tokens[1];
                auto it = peers.find(exitUsername);
                if (it != peers.end())
                {
                    peers.erase(it);
                    cout << "Username " << exitUsername << " removed.\n\n";
                }
                continue;
            }

            string data;
            if (tokens.size() == 1 && tokens[0] == "GET_U")
                data = getAllUsernames();
            else if (tokens.size() == 2 && tokens[0] == "GET_P")
                data = getPeerData(tokens[1]);
            else if (tokens.size() == 1 && tokens[0] == "GET_AF")
                data = getAllFilenames();
            else if (tokens.size() == 2 && tokens[0] == "GET_UF")
                data = getUserFilenames(tokens[1]);
            else
                data = "ERR,Invalid Request!";

            cout << "Sent Data: " << data << "\n\n";
            send(connfd, data.c_str(), data.size(), 0);
        }
        else
        {
            cout << ANSI_COLOR_RED << "Disconnected: {ip: " << inet_ntoa(c_addr.sin_addr)
                 << " port: " << ntohs(c_addr.sin_port) << "}"
                 << ANSI_COLOR_RESET << "\n\n";
            close(connfd);
            break;
        }
    }
}

void Server::printPeers()
{
    for (const auto &entry : peers)
    {
        const std::string &username = entry.first;
        const peer_data &pdata = entry.second;
        cout << "Data: {\n\tUsername: " << username << endl;
        cout << "\tDirectory: " << pdata.dir << endl;
        cout << "\tPort: " << pdata.port << endl;
        cout << "\tFiles: ";
        for (const auto &file : pdata.files)
        {
            cout << file << ", ";
        }
        cout << "\n}\n";
        std::cout << std::endl;
    }
}

string Server::registerUser(string username, peer_data pdata)
{
    if (peers.find(username) == peers.end())
        return "ERR,User not found!";
    peers.insert(make_pair(username, pdata));
    return "OK";
}
string Server::getPeerData(string username)
{
    if (peers.find(username) == peers.end())
        return "ERR,User not found!";

    peer_data pdata = peers[username];
    return "OK," + pdata.ip + "," + to_string(pdata.port);
}

string Server::getAllUsernames()
{
    string result;

    for (const auto &pair : peers)
    {
        result += pair.first + ",";
    }

    // Remove the trailing comma and space if there are any usernames
    if (!result.empty())
        result.pop_back(); // Remove the last comma

    return "OK," + result;
}

string Server::getAllFilenames()
{
    string result;

    for (const auto &pair : peers)
    {
        result += pair.first + ": " + vectorToString(pair.second.files) + '\n';
    }

    if (!result.empty())
    {
        result.pop_back();
    }

    return result;
}

string Server::getUserFilenames(string username)
{
    if (peers.find(username) == peers.end())
        return "ERR,User not found!";
    return vectorToString(peers[username].files);
}

int main()
{
    Server s1(80);
    s1.start();
    while (1)
    {
    }
    return 0;
}

string vectorToString(const vector<string> &files)
{
    string result = "";
    for (const auto &element : files)
        result += element + ",";

    return result;
}