/**
 * @file common.h
 * @brief Common definitions and utilities for distributed worker system
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Error codes */
#define ERR_SUCCESS          0
#define ERR_INVALID_ARGS    -1
#define ERR_SOCKET_CREATE   -2
#define ERR_SOCKET_BIND     -3
#define ERR_SOCKET_LISTEN   -4
#define ERR_SOCKET_ACCEPT   -5
#define ERR_SOCKET_CONNECT  -6
#define ERR_SEND_MESSAGE    -7
#define ERR_RECV_MESSAGE    -8
#define ERR_INVALID_MESSAGE -9
#define ERR_JOB_NOT_FOUND   -10
#define ERR_WORKER_NOT_FOUND -11
#define ERR_OUT_OF_MEMORY   -12

/* Job types */
#define JOB_TYPE_ECHO        1  /* Echo the input data */
#define JOB_TYPE_REVERSE     2  /* Reverse the input string */
#define JOB_TYPE_UPPERCASE   3  /* Convert input string to uppercase */
#define JOB_TYPE_LOWERCASE   4  /* Convert input string to lowercase */
#define JOB_TYPE_COUNT_CHARS 5  /* Count characters in input string */
#define JOB_TYPE_CUSTOM      99 /* Custom job type */

/**
 * @brief Get current timestamp in milliseconds
 * 
 * @return long long Current timestamp in milliseconds
 */
long long get_timestamp_ms();

/**
 * @brief Get formatted timestamp string
 * 
 * @param buffer Buffer to store timestamp string
 * @param size Size of buffer
 * @return char* Pointer to buffer
 */
char *get_timestamp_str(char *buffer, size_t size);

/**
 * @brief Get job type name
 * 
 * @param job_type Job type
 * @return const char* Job type name
 */
const char *get_job_type_name(int job_type);

/**
 * @brief Generate a unique ID
 * 
 * @return int Unique ID
 */
int generate_unique_id();

#endif /* COMMON_H */