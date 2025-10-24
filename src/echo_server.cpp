#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

struct ServerConfig {
    // predefined configs
    u_short port = 8080;
    bool output_messages = false;
    u_long max_clients = 100;
    u_long buffer_size = 1024;
};

class EchoServer {
private:
    SOCKET server_socket;
    WSADATA wsa_data;
    atomic<bool> running;
    vector<SOCKET> client_sockets;
    ServerConfig server_config;

public:
    EchoServer() : server_socket(INVALID_SOCKET), running(false) {}

    ~EchoServer() {
        stop();
    }

    bool start(const ServerConfig config) {
        // save configuration
        server_config = config;

        // initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            cerr << "Error inicializando Winsock: " << WSAGetLastError() << endl;
            return false;
        }

        // create socket
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET) {
            cerr << "Error creando socket: " << WSAGetLastError() << endl;
            WSACleanup();
            return false;
        }

        // configure socket as non-blocking
        u_long mode = 1;
        if (ioctlsocket(server_socket, FIONBIO, &mode) == SOCKET_ERROR) {
            cerr << "Error configurando no bloqueante: " << WSAGetLastError() << endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        // configure server address
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(server_config.port);

        // bind
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            cerr << "Error en bind: " << WSAGetLastError() << endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        // listen
        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
            cerr << "Error en listen: " << WSAGetLastError() << endl;
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        running = true;
        cout << "Echo Server No Bloqueante iniciado en puerto " << server_config.port << endl;
        cout << "Esperando conexiones..." << endl;

        return true;
    }

    void run() {
        while (running) {
            // accept new connections
            accept_new_connections();
            
            // process existing customers
            process_clients();
            
            // small pause to avoid saturating the CPU
            Sleep(16);
        }
    }

    void stop() {
        running = false;
        
        // close all client sockets
        for (SOCKET client : client_sockets) {
            closesocket(client);
        }
        client_sockets.clear();
        
        // close server socket
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }
        
        WSACleanup();
        cout << "Servidor detenido" << endl;
    }

private:
    void accept_new_connections() {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket;

        while (
            server_config.max_clients != client_sockets.size() &&
            (client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_addr_len)) != INVALID_SOCKET
        ) {
            // configure new client as non-blocking
            u_long mode = 1;
            ioctlsocket(client_socket, FIONBIO, &mode);
            
            client_sockets.push_back(client_socket);
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            
            cout << "Nuevo cliente: " << client_ip << ":" << ntohs(client_addr.sin_port);
            cout << " [Total: " << client_sockets.size() << "]" << endl;
        }

        if (server_config.max_clients != client_sockets.size()) {
            // rechazar nuevas conexiones
        }
        
        // expected error when there are no more pending connections
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != 0) {
            cerr << "Error en accept: " << error << endl;
        }
    }

    void process_clients() {        
        vector<SOCKET> clients_to_remove;
        
        // first pass: receive data and detect disconnected clients
        for (SOCKET client_socket : client_sockets) {
            char buffer[server_config.buffer_size];
            int bytes_received;
            
            // receive data
            bytes_received = recv(client_socket, buffer, server_config.buffer_size - 1, 0);
            
            if (bytes_received == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    // no data available, continue with next client
                    continue;
                }
                else {
                    // actual error, mark for removal
                    cerr << "Error en recv: " << error << endl;
                    clients_to_remove.push_back(client_socket);
                    continue;
                }
            }
            
            if (bytes_received == 0) {
                // client disconected
                cout << "Cliente desconectado" << endl;
                clients_to_remove.push_back(client_socket);
                continue;
            }
            
            // process received message
            if (server_config.output_messages) {
                string msg(buffer, bytes_received);
                msg[bytes_received] = '\0';
                cout << "Mensaje: " << msg << endl;
            }
            
            // send echo to ALL clients
            broadcast_message(buffer, bytes_received, client_socket);
        }
        
        // second pass: remove disconnected clients
        if (!clients_to_remove.empty()) {
            remove_disconnected_clients(clients_to_remove);
        }
    }
    
    void broadcast_message(const char* message, int message_length, SOCKET sender_socket) {
        vector<SOCKET> clients_to_remove;
        
        for (SOCKET client : client_sockets) {
            // do not send the message back to the sender
            if (client == sender_socket) {
                continue;
            }
            
            int bytes_sent = send(client, message, message_length, 0);
            if (bytes_sent == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    cerr << "Error en broadcast: " << error << endl;
                    clients_to_remove.push_back(client);
                }
            }
        }
        
        // remove clients that failed to send
        if (!clients_to_remove.empty()) {
            remove_disconnected_clients(clients_to_remove);
        }
    }
    
    void remove_disconnected_clients(const vector<SOCKET>& clients_to_remove) {
        for (SOCKET client : clients_to_remove) {
            auto it = find(client_sockets.begin(), client_sockets.end(), client);
            if (it != client_sockets.end()) {
                closesocket(client);
                client_sockets.erase(it);
                cout << "Cliente removido. Total: " << client_sockets.size() << endl;
            }
        }
    }
};

void loadConfig(string filename, ServerConfig& config) {
    ifstream cin(filename);

    string input;
    int line = 1;

    config.port = 8080;
    config.output_messages = false;
    config.max_clients = 100;
    config.buffer_size = 1024;

    while (getline(cin, input)) {
        istringstream line_stream(input);
        string key;

        // skip empty lines
        if (input.empty()) {
            goto skip;
        }
        
        // read the first word from the line
        if (!(line_stream >> key)) {
            // skip lines with only whitespace
            goto skip;
        }

        // ignore this line if it is a comment.
        if (key[0] == '#') {
            goto skip;
        }

        // check for settings and errors
        if (key == "port:") {
            u_short port;
            if (!(line_stream >> port)) {
                goto error;
            }
            
            // todo check if this is a valid port
            config.port = port;
            goto skip;
        }
        else if (key == "output_messages:") {
            string val;
            if (!(line_stream >> val)) {
                goto error;
            }

            if (val == "true" || val == "1") {
                config.output_messages = true;
            }
            else if (val == "false" || val == "0") {
                config.output_messages = false;
            }
            else {
                goto error;
            }

            goto skip;
        }
        else if (key == "max_clients:") {
            u_long val;
            if (!(line_stream >> val)) {
                goto error;
            }

            if (val == 0) {
                val = 1;
            }

            config.max_clients = val;
            goto skip;
        }
        else if (key == "buffer_size:") {
            u_long val;
            if (!(line_stream >> val)) {
                goto error;
            }

            if (val == 0) {
                val = 64;
            }

            config.buffer_size = val;
            goto skip;
        }
        else {
            goto error;
        }

        error:
        cerr << "Error de lectura en la linea: " << line << endl;
        
        skip:
        line++;
    }
}

int main() {
    ServerConfig config;
    loadConfig("server.cfg", config);
    
    EchoServer server;
    if (server.start(config)) {
        cout << "Servidor iniciado. Presiona Ctrl+C para detener." << endl;
        server.run();
    }
    else {
        cerr << "No se pudo iniciar el servidor" << endl;
        return 1;
    }

    return 0;
}