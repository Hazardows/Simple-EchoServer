#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

struct ClientConfig {
    // predefined configs
    string server_ip = "127.0.0.1";
    u_short port = 8080;
};

class Client {
private:
    SOCKET client_socket;
    atomic<bool> running;
    thread receive_thread;

public:
    Client() : client_socket(INVALID_SOCKET), running(false) {}
    
    ~Client() {
        disconnect();
    }
    
    bool connect_to_server(const ClientConfig config) {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            cerr << "WSAStartup fallido" << endl;
            return false;
        }
        
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket == INVALID_SOCKET) {
            cerr << "Error al crear el socket" << endl;
            WSACleanup();
            return false;
        }
        
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config.port);
        inet_pton(AF_INET, config.server_ip.c_str(), &server_addr.sin_addr);
        
        if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            cerr << "Error al conectar: " << WSAGetLastError() << endl;
            closesocket(client_socket);
            WSACleanup();
            return false;
        }
        
        // configure socket as non-blocking for receiving
        u_long mode = 1;
        ioctlsocket(client_socket, FIONBIO, &mode);
        
        running = true;
        cout << "Conectado al servidor " << config.server_ip << ":" << config.port << endl;
        
        // start thread to receive messages
        receive_thread = thread(&Client::receive_messages, this);
        
        return true;
    }
    
    void send_messages() {
        string message;
        cout << "Escribe mensajes (escribe 'quit' para salir):" << endl;
        
        while (running) {
            cout << "> ";
            getline(cin, message);
            
            if (message == "quit") {
                running = false;
                break;
            }
            
            if (!message.empty()) {
                int result = send(client_socket, message.c_str(), message.length(), 0);
                if (result == SOCKET_ERROR) {
                    cerr << "Error enviando mensaje" << endl;
                    break;
                }
            }
        }
    }
    
    void disconnect() {
        running = false;
        
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
        
        if (client_socket != INVALID_SOCKET) {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }
        
        WSACleanup();
        cout << "Desconectado del servidor" << endl;
    }

private:
    void receive_messages() {
        char buffer[1024];
        
        while (running) {
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    this_thread::sleep_for(chrono::milliseconds(16));
                    continue;
                }
                else {
                    cerr << "Error recibiendo mensaje: " << error << endl;
                    break;
                }
            }
            
            if (bytes_received == 0) {
                cout << "Servidor desconectado" << endl;
                running = false;
                break;
            }
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                cout << "Recibido: " << buffer << endl;
                
                cout << "> ";
            }
        }
    }
};

void loadConfig(string filename, ClientConfig& config) {
    ifstream cin(filename);

    string input;
    int line = 1;

    config.server_ip = "127.0.0.1";
    config.port = 8080;

    while (cin >> input) {
        // ignore this line if it is a comment.
        if (input[0] == '#') {
            getline(cin, input);
            goto next;
        }

        // check for settings and errors
        if (input == "port:") {
            u_short port;
            cin >> port;
            
            // todo check if this is a valid port
            config.port = port;

            goto next;
        }
        else if (input == "server_ip:") {
            string val;
            cin >> val;

            // todo: check if this is a valid server ip
            config.server_ip = val;

            goto next;
        }


        error:
        cout << "Error parsing file at line: " << line << endl;
        
        next:
        line++;
    }
}

int main() {
    ClientConfig config;
    loadConfig("client.cfg", config);

    Client client;
    if (client.connect_to_server(config)) {
        client.send_messages();
    }
    
    return 0;
}