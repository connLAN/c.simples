/**
 * @file protocol.c
 * @brief Implementation of protocol functions for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

int send_message(int sockfd, const Message *msg) {
    if (sockfd < 0 || !msg) {
        return -1;
    }

    // Send the entire message structure
    ssize_t bytes_sent = send(sockfd, msg, sizeof(Message), 0);
    if (bytes_sent != sizeof(Message)) {
        perror("Failed to send message");
        return -1;
    }

    return 0;
}

int receive_message(int sockfd, Message *msg) {
    if (sockfd < 0 || !msg) {
        return -1;
    }

    // Clear the message buffer
    memset(msg, 0, sizeof(Message));

    // Receive the entire message structure
    ssize_t bytes_received = recv(sockfd, msg, sizeof(Message), 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            // Connection closed
            fprintf(stderr, "Connection closed by peer\n");
        } else {
            perror("Failed to receive message");
        }
        return -1;
    }

    if (bytes_received != sizeof(Message)) {
        fprintf(stderr, "Incomplete message received: %zd bytes out of %zu\n", 
                bytes_received, sizeof(Message));
        return -1;
    }

    return 0;
}

const char *job_status_to_string(JobStatus status) {
    switch (status) {
        case JOB_STATUS_PENDING:
            return "Pending";
        case JOB_STATUS_ASSIGNED:
            return "Assigned";
        case JOB_STATUS_COMPLETED:
            return "Completed";
        case JOB_STATUS_FAILED:
            return "Failed";
        default:
            return "Unknown";
    }
}