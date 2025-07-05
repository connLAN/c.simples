/**
 * net_utils.h - Network Utilities
 * 
 * This file provides utility functions for network communication.
 */

#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stdint.h>
#include "comm_protocol.h"

/**
 * Create a server socket
 * 
 * @param port Port number to listen on
 * @param backlog Maximum length of the queue of pending connections
 * @return Socket file descriptor or -1 on error
 */
int create_server_socket(uint16_t port, int backlog);

/**
 * Create a client socket and connect to server
 * 
 * @param server_ip Server IP address
 * @param server_port Server port number
 * @return Socket file descriptor or -1 on error
 */
int create_client_socket(const char *server_ip, uint16_t server_port);

/**
 * Set socket timeout
 * 
 * @param sockfd Socket file descriptor
 * @param timeout_sec Timeout in seconds
 * @return 0 on success, -1 on error
 */
int set_socket_timeout(int sockfd, int timeout_sec);

/**
 * Send a message (header + payload)
 * 
 * @param sockfd Socket file descriptor
 * @param msg_type Message type
 * @param id Message ID (task ID or worker ID)
 * @param data Payload data (can be NULL if data_size is 0)
 * @param data_size Size of payload data
 * @return Number of bytes sent or -1 on error
 */
int send_message(int sockfd, uint8_t msg_type, uint32_t id, 
                const void *data, uint32_t data_size);

/**
 * Receive message header
 * 
 * @param sockfd Socket file descriptor
 * @param header Pointer to store the received header
 * @return 0 on success, -1 on error
 */
int recv_header(int sockfd, msg_header_t *header);

/**
 * Receive message payload
 * 
 * @param sockfd Socket file descriptor
 * @param buffer Buffer to store the received payload
 * @param size Size of the buffer
 * @return Number of bytes received or -1 on error
 */
int recv_payload(int sockfd, void *buffer, uint32_t size);

#endif /* NET_UTILS_H */