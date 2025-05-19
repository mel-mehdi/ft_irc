IRC Server
Description
An Internet Relay Chat (IRC) server implementation in C++98, following the IRC protocol. This server allows multiple clients to connect, authenticate, join channels, and communicate with each other.

Features
Multiple client handling with non-blocking I/O operations
Client authentication using password
Channel creation and management
Private messaging between users
Channel operator privileges
Channel modes (invite-only, topic restrictions, password, user limit)
Commands: PASS, NICK, USER, JOIN, PART, PRIVMSG, KICK, INVITE, TOPIC, MODE, QUIT
Requirements
C++ compiler with C++98 support
Linux/Unix environment
Compilation
make
Usage
./ircserv <port> <password>
port: The port number on which the server will listen for incoming connections
password: The password required for clients to connect to the server
Connecting to the Server
You can connect to the server using any IRC client, such as:

irssi
HexChat
Weechat
mIRC
Example connection with irssi:

Supported Commands
PASS <password>: Set connection password
NICK <nickname>: Set or change nickname
USER <username> <hostname> <servername> :<realname>: Set user information
JOIN <channel> [password]: Join a channel
PART <channel> [message]: Leave a channel
PRIVMSG <target> :<message>: Send a message to a user or channel
KICK <channel> <user> [reason]: Remove a user from a channel
INVITE <nickname> <channel>: Invite a user to a channel
TOPIC <channel> [topic]: Set or view channel topic
MODE <channel> <flags> [parameters]: Change channel modes
+i: Set invite-only
+t: Set topic restriction to channel operators
+k <password>: Set channel password
+o <nickname>: Give channel operator privileges
+l <limit>: Set user limit
QUIT [message]: Disconnect from server
Implementation Notes
Uses poll() for handling I/O operations
Non-blocking sockets for better performance
Follows C++98 standard
No external libraries used
Authors
[Mahfoud El Mehdi]