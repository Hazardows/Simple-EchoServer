# Simple EchoServer
A lightweight, multi-client echo server implementation in C++ for Windows.

## Features
 - Multi-client support: Handles multiple simultaneous connections.
 - Non-blocking I/O: Efficient connection management using non-blocking sockets.
 - Real-time echo: Immediately returns received messages to all connected clients.
 - Cross-client broadcasting: Messages from one client are broadcast to all others.
 - Configurable: Customizable settings via server.cfg file.

## Configurations
### Server Configuration
Create server.cfg in the same directory as echo_server.exe to customize server settings. If no configuration file is found, default values will be used.

### Client Configuration
Create test_client.cfg in the same directory as test_client.exe to customize client behavior. If no configuration file is found, default values will be used.

### Sample Configurations
Check the sample_configs/ directory for example configuration files that you can copy and modify:
- server.cfg - Example server configuration
- test_client.cfg - Example client configuration

## Compilation
To compile both the echo server and test client, run the build.bat script.
This will: 
 - Compile the sources using the Clang compiler with static linking.
 - Create the executables in the bin/ directory.
Note: Sample configuration files are not automatically copied to the bin/ directory.
