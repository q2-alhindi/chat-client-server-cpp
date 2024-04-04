#include <map>
// IOT socket api
#include <iot/socket.hpp>

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <chat.hpp>

#define USER_ALL "__ALL"
#define USER_END "END"

/**
 * @brief map of current online clients
*/
typedef std::map<std::string, sockaddr_in *> online_users;

void handle_list(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop);

/**
 * @brief Send a given message to all clients
 *
 * @param msg to send
 * @param username used if  not to send to that particular user
 * @param online_users current online users
 * @param sock socket for communicting with client 
 * @param send_to_username determines also to send to username
*/
void send_all(
    chat::chat_message& msg, std::string username, online_users& online_users, 
    uwe::socket& sock, bool send_to_username = true) {
    for (const auto user: online_users) {    
        if ((send_to_username && user.first.compare(username) == 0) || user.first.compare(username) != 0) { 
            int len = sock.sendto(
                reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
                (sockaddr*)user.second, sizeof(struct sockaddr_in));
        }
    }   
}

/**
 * @brief handle sending an error and incoming error messages
 * 
 * Note: there should not be any incoming errors messages!
 * 
 * @param err code for error
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_error(uint16_t err, struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    auto msg = chat::error_msg(err);
    int len = sock.sendto(
        reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
        (sockaddr*)&client_address, sizeof(struct sockaddr_in));
}

/**
 * @brief handle broadcast message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_broadcast(
    online_users& online_users, std::string username, std::string msg,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    
    DEBUG("Received broadcast\n");

    // Prepare the broadcast message outside the loop to avoid re-creating it
    auto m = chat::broadcast_msg(username, msg);

    // Iterate through the list of online users to send the message to each user
    for (const auto& [user_name, user_address] : online_users) {
        // Skip sending the message to the user who sent it
        if (client_address.sin_addr.s_addr == user_address->sin_addr.s_addr &&
            client_address.sin_port == user_address->sin_port) {
            DEBUG("Not sending message to self: %s\n", username.c_str());
            continue; // Skip to the next user
        }

        // Send the broadcast message
        int len = sock.sendto(
            reinterpret_cast<const char*>(&m), sizeof(chat::chat_message), 0,
            (sockaddr*)user_address, sizeof(struct sockaddr_in));

        // Check if sending was successful
        if (len < 0) {
            // Handle send error, e.g., by logging or taking corrective action
            DEBUG("Failed to send message to %s\n", user_name.c_str());
        }
    }
}


/**
 * @brief handle join messageß
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
// void handle_join(
//     online_users& online_users, std::string username, std::string, 
//     struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
//     DEBUG("Received join\n");

//     // first check user not already online
//     if (auto search = online_users.find(username); search != online_users.end()) {
//         handle_error(ERR_USER_ALREADY_ONLINE, client_address, sock, exit_loop);
//     }
//     else {
//         sockaddr_in *useraddress = new sockaddr_in;
//         *useraddress = client_address;

//         online_users[username] = useraddress;
//         // Send a "JACK" message to the client
//         auto jack_msg = chat::jack_msg();
//         int len = sock.sendto(
//             reinterpret_cast<const char*>(&jack_msg), sizeof(chat::chat_message), 0,
//             (sockaddr*)&client_address, sizeof(struct sockaddr_in));
//     DEBUG("Received join\n");
//         // Broadcast the join message to all other clients
//         handle_broadcast(online_users, username, "has joined the server", client_address, sock, exit_loop);

//         // Send a list message containing all online users to the client who just joined
//         handle_list(online_users, "__ALL", "", client_address, sock, exit_loop);
        
//     }
// }
void handle_join(

    online_users& users, std::string username, std::string,

    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {

    if (users.find(username) != users.end()) {
        handle_error(ERR_USER_ALREADY_ONLINE, client_address, sock, exit_loop);
    } else {
        auto* addr = new sockaddr_in(client_address);
        users[username] = addr;
        auto msg = chat::jack_msg();
        sock.sendto(reinterpret_cast<const char*>(&msg), sizeof(msg), 0, (sockaddr*)&client_address, sizeof(client_address));
        for (const auto& user : users) {
            if (user.first != username) {
                auto brdcst = chat::broadcast_msg("Server", username + " has joined the chat.");
                sock.sendto(reinterpret_cast<const char*>(&brdcst), sizeof(brdcst), 0, (sockaddr*)user.second, sizeof(sockaddr_in));
            }
        }
        handle_list(users, USER_ALL, "", client_address, sock, exit_loop);
    }
}

/*
 * @brief handle jack message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_jack(
    online_users& online_users, std::string username, std::string, 
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received jack\n");
    handle_error(ERR_UNEXPECTED_MSG, client_address, sock, exit_loop);
}

/**
 * @brief handle direct message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_directmessage(
    online_users& online_users, std::string recipient, std::string message,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received direct message to %s\n", recipient.c_str());

    // Find the recipient in the map of online users
    auto recipient_it = online_users.find(recipient);
    if (recipient_it != online_users.end()) {
        DEBUG("Found user for direct message\n");
        // Create the direct message
        auto d = chat::dm_msg(recipient, message);
        // Send the direct message
        int len = sock.sendto(
            reinterpret_cast<const char*>(&d), sizeof(chat::chat_message), 0,
            (sockaddr*)recipient_it->second, sizeof(struct sockaddr_in));

        if (len < 0) {
            DEBUG("Failed to send direct message to %s\n", recipient.c_str());
            // Optionally handle the send error (e.g., by logging or retrying)
        }
    } else {
        DEBUG("Recipient %s not found\n", recipient.c_str());
        // Optionally handle the case when the recipient is not found
    }
}




/**
 * @brief handle list message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_list(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received list\n");

    int username_size = MAX_USERNAME_LENGTH;
    int message_size  = MAX_MESSAGE_LENGTH;

    char username_data[MAX_USERNAME_LENGTH] = { '\0' };
    char * username_ptr = &username_data[0];
    char message_data[MAX_MESSAGE_LENGTH] = { '\0' };
    char * message_ptr = &message_data[0];

    bool using_username = true;
    bool full = false;

    for (const auto user: online_users) {
        if (using_username) {
            if (username_size - (user.first.length()+1) >= 0) {
                memcpy(username_ptr, user.first.c_str(), user.first.length());
                *(username_ptr+user.first.length()) = ':';
                username_ptr = username_ptr+user.first.length()+1;
                username_size = username_size - (user.first.length()+1);
                username_data[MAX_USERNAME_LENGTH - username_size] = '\0';
            }
            else {
                using_username = false;
            }
        }

        // otherwise we fill the message field
        if(!using_username) {
            if (message_size - (user.first.length()+1) >= 0) {
                memcpy(message_ptr, user.first.c_str(), user.first.length());
                *(message_ptr+user.first.length()) = ':';
                message_ptr = message_ptr+user.first.length()+1;
                message_size = message_size - (user.first.length()+1);
            }
            else {
                // we are full and we need to send packet and start again
                chat::chat_message msg{chat::LIST, '\0', '\0'};
                username_data[MAX_USERNAME_LENGTH - username_size] = '\0';
                memcpy(msg.username_, &username_data[0], MAX_USERNAME_LENGTH - username_size );
                message_data[MAX_MESSAGE_LENGTH - message_size] = '\0';
                memcpy(msg.message_, &message_data[0], MAX_MESSAGE_LENGTH - message_size );

                // 
                if (username.compare("__ALL") == 0) {
                    send_all(msg, "__ALL", online_users, sock);
                }
                else {
                    int len = sock.sendto(
                        reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
                        (sockaddr*)&client_address, sizeof(struct sockaddr_in));
                }

                username_size = MAX_USERNAME_LENGTH;
                message_size  = MAX_MESSAGE_LENGTH;

                username_ptr = &username_data[0];
                message_ptr = &message_data[0];

                using_username = false;
            }
        }
    }

    if (using_username) {
        if (username_size >= 4) { 
            // enough space to store end in username
            memcpy(&username_data[MAX_USERNAME_LENGTH - username_size], USER_END, strlen(USER_END) );
            username_size = username_size - (strlen(USER_END)+1);
        }
        else {
            username_size = username_size + 1; // this enables overwriting the last ':'
            using_username = false;
        }
    }
    
    if (!using_username) {

    }

    chat::chat_message msg{chat::LIST, '\0', '\0'};
    username_data[MAX_USERNAME_LENGTH - username_size] = '\0';
    DEBUG("username_data = %s\n", username_data);
    memcpy(msg.username_, &username_data[0], MAX_USERNAME_LENGTH - username_size );
    message_data[MAX_MESSAGE_LENGTH - message_size] = '\0';
    memcpy(msg.message_, &message_data[0], MAX_MESSAGE_LENGTH - message_size );

    if (username.compare("__ALL") == 0) {
        send_all(msg, "__ALL", online_users, sock);
    }
    else {
        int len = sock.sendto(
            reinterpret_cast<const char*>(&msg), sizeof(chat::chat_message), 0,
            (sockaddr*)&client_address, sizeof(struct sockaddr_in));
    }
}

/**
 * @brief handle leave message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_leave(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {

    DEBUG("Received leave\n");

    // Attempt to identify the username based on the client's socket address
    for (const auto& user : online_users) {
        if (strcmp(inet_ntoa(client_address.sin_addr), inet_ntoa(user.second->sin_addr)) == 0 &&
            client_address.sin_port == user.second->sin_port) {
            username = user.first;
            break; // Stop the search once the user is found
        }
    }
    
    if (username.empty()) {
        // This condition should not happen if the user was correctly identified
        DEBUG("Error: User not found.");
        handle_error(ERR_UNKNOWN_USERNAME, client_address, sock, exit_loop);
    } else {
        // Log the username of the user leaving
        DEBUG("%s is leaving the server\n", username.c_str());

        // Broadcast message to other users about this user leaving
        auto brdcast = chat::broadcast_msg("Server", username + " has left the chat.");
        send_all(brdcast, username, online_users, sock);

        // Clean up: Remove the user from the online_users map
        auto search = online_users.find(username);
        if (search != online_users.end()) {
            delete search->second; // Assuming dynamic allocation of sockaddr_in
            online_users.erase(search);
        }

        // Acknowledge the user's leave request
        auto ack_msg = chat::lack_msg();
        sock.sendto(reinterpret_cast<const char*>(&ack_msg), sizeof(chat::chat_message), 0,
                    (sockaddr*)&client_address, sizeof(struct sockaddr_in));
    }
}


/**
 * @brief handle lack message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_lack(
    online_users& online_users, std::string username, std::string,
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    DEBUG("Received lack\n");
    handle_error(ERR_UNEXPECTED_MSG, client_address, sock, exit_loop);
}

/**
 * @brief handle exit message
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_exit(
    online_users& online_users, std::string username, std::string, 
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
    
    DEBUG("Received exit\n");

    // Iterate over the online_users map to send an exit message to each user
    for (auto& user_entry : online_users) {
        std::string user_name = user_entry.first;
        sockaddr_in* user_addr = user_entry.second;

        // Create the exit message packet
        chat::chat_message exit_message = chat::exit_msg();

        // Send the exit message to the user
        int len = sock.sendto(
            reinterpret_cast<const char*>(&exit_message), sizeof(chat::chat_message), 0,
            (sockaddr*)user_addr, sizeof(struct sockaddr_in));

        if (len == sizeof(chat::chat_message)) {
            DEBUG("Exit message sent to %s\n", user_name.c_str());
        } else {
            DEBUG("Failed to send exit message to %s\n", user_name.c_str());
            // Handle the failure to send the message (e.g., error message or other actions)
        }

        // Clear up memory for the user
        delete user_addr;
    }

    // Clear the online_users map
    online_users.clear();

    // Set exit_loop to true to indicate that the event loop should terminate
    exit_loop = true;
}


/**
 * @brief
 * 
 * @param online_users map of usernames to their corresponding IP:PORT address
 * @param username part of chat protocol packet
 * @param msg part of chat protocol packet
 * @param client_address address of client to send message to
 * @param sock socket for communicting with client
 * @parm exit_loop set to true if event loop is to terminate
*/
void handle_error(
    online_users& online_users, std::string username, std::string, 
    struct sockaddr_in& client_address, uwe::socket& sock, bool& exit_loop) {
     DEBUG("Received error\n");
}

/**
 * @brief function table, mapping command type to handler.
*/
void (*handle_messages[9])(online_users&, std::string, std::string, struct sockaddr_in&, uwe::socket&, bool& exit_loop) = {
    handle_join, handle_jack, handle_broadcast, handle_directmessage,
    handle_list, handle_leave, handle_lack, handle_exit, handle_error
};

/**
 * @brief server for chat protocol
*/
void server() {
    // keep track of online users
    online_users online_users;

    // port to start the server on

	// socket address used for the server
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// htons: host to network short: transforms a value in host byte
	// ordering format to a short value in network byte ordering format
	server_address.sin_port = htons(SERVER_PORT);

	// htons: host to network long: same as htons but to long
	// server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	// creates binary representation of server name and stores it as sin_addr
	inet_pton(AF_INET, uwe::get_ipaddr().c_str(), &server_address.sin_addr);

    // create a UDP socket
	uwe::socket sock{AF_INET, SOCK_DGRAM, 0};

	sock.bind((struct sockaddr *)&server_address, sizeof(server_address));

	// socket address used to store client address
	struct sockaddr_in client_address;
	size_t client_address_len = 0;

	char buffer[sizeof(chat::chat_message)];
    DEBUG("Entering server loop\n");
    bool exit_loop = false;
	for (;!exit_loop;) {
        int len = sock.recvfrom(
			buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, &client_address_len);

      
        // DEBUG("Received message:\n");
        if (len == sizeof(chat::chat_message)) {
            // handle incoming packet
            chat::chat_message * message = reinterpret_cast<chat::chat_message*>(buffer);
            auto type = static_cast<chat::chat_type>(message->type_);
            std::string username{(const char*)&message->username_[0]};
            std::string msg{(const char*)&message->message_[0]};

            if (is_valid_type(type)) {
                DEBUG("handling msg type %d\n", type);
                // valid type, so dispatch message handler
                handle_messages[type](online_users, username, msg, client_address, sock, exit_loop);
            }
        }
        else {
            DEBUG("Unexpected packet length\n");
        }
    }
}

/**
 * @brief entry point for chat server application
*/
int main(void) { 
    // Set server IP address
    uwe::set_ipaddr("192.168.1.7");
    server();

    return 0;
}