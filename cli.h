#ifndef CLIENT_H
#define CLIENT_H

#include <vector>
#include <string>
#include <atomic>
#include <thread>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

void printVector(const std::vector<std::string> &);
std::vector<std::string> stringToTokens(std::string);

#define MIN_PORT 1
#define MAX_PORT 65535
#define SERVER_PORT 80
#define SERVER_IP "127.0.0.1"

class Client
{
private:
    std::string dir;
    std::vector<std::string> files;

    void input_data();
    int serverConnect();

public:
    std::string username;
    int port;
    std::atomic<bool> chatSession;
    std::atomic<bool> blockUI;

    Client();
    Client(std::string username, std::string dir, int port);
    bool serverRegister();
    void serverExit();
    std::string fetchPeerData(std::string p_username);
    std::vector<std::string> fetchUsernames();
    bool usernameAvail(std::string username);
    bool userExists(std::string name);
    void updateFiles(const std::vector<std::string> &files);
    std::string filesToString();
    bool fetchFiles();
    std::string fetchFilenamesFromServer();
    std::string fetchUserFilenamesFromServer(std::string username);
    std::vector<std::string> getFiles();
    int peerConnect(std::string p_ip, int p_port);
    void peerChat(std::string username);
    bool startChatServer();
    void invokeAccept(int fd);
    void sendFileToPeer(int connfd, std::string filename);
    void recvFileFromPeer(std::string username, std::string filename);
    void recvChatFromPeer(int connfd);
    void sendChatToPeer(int connfd);
};

#endif