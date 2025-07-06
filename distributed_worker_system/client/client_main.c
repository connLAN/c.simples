/**
 * @file client_main.c
 * @brief Client main function for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "client.h"
#include "../common/logger.h"
#include "../common/common.h"
#include "../common/protocol.h"

/* Default configuration values */
#define DEFAULT_SERVER_IP      "127.0.0.1"
#define DEFAULT_SERVER_PORT    8080
#define DEFAULT_LOG_LEVEL     LOG_INFO

/* Global flag for signal handling */
static volatile int g_running = 1;

/* Signal handler */
static void signal_handler(int sig) {
    g_running = 0;
}

/* Print usage information */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help             Display this help message\n");
    printf("  -s, --server-ip IP     Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --server-port PORT Server port (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  -j, --job-type TYPE    Job type to submit (required)\n");
    printf("  -d, --data DATA        Input data for job (optional)\n");
    printf("  -f, --file FILE        File containing input data (optional)\n");
    printf("  -w, --wait             Wait for job completion\n");
    printf("  -t, --timeout SEC      Timeout in seconds when waiting (default: 30)\n");
    printf("  -l, --log-level LEVEL  Log level (0-5, default: %d)\n", DEFAULT_LOG_LEVEL);
    printf("                         0=TRACE, 1=DEBUG, 2=INFO, 3=WARNING, 4=ERROR, 5=FATAL\n");
    printf("  -o, --log-file FILE    Log file (default: stdout)\n");
}

/* Read file contents */
static int read_file(const char *filename, char **data, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file: %s", filename);
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        LOG_ERROR("Empty file: %s", filename);
        return -1;
    }
    
    *data = (char *)malloc(file_size);
    if (!*data) {
        fclose(file);
        LOG_ERROR("Failed to allocate memory for file data");
        return -1;
    }
    
    size_t bytes_read = fread(*data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(*data);
        LOG_ERROR("Failed to read file: %s", filename);
        return -1;
    }
    
    *size = bytes_read;
    return 0;
}

int main(int argc, char *argv[]) {
    /* Configuration with default values */
    ClientConfig config;
    memset(&config, 0, sizeof(ClientConfig));
    strncpy(config.server_ip, DEFAULT_SERVER_IP, sizeof(config.server_ip) - 1);
    config.server_port = DEFAULT_SERVER_PORT;
    config.reconnect_interval_seconds = 5;
    
    int job_type = -1;
    char *input_data = NULL;
    size_t input_size = 0;
    int wait_for_completion = 0;
    int timeout_seconds = 30;
    int log_level = DEFAULT_LOG_LEVEL;
    char log_file[256] = "";
    
    /* Define command line options */
    static struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"server-ip",   required_argument, 0, 's'},
        {"server-port", required_argument, 0, 'p'},
        {"job-type",    required_argument, 0, 'j'},
        {"data",        required_argument, 0, 'd'},
        {"file",        required_argument, 0, 'f'},
        {"wait",        no_argument,       0, 'w'},
        {"timeout",     required_argument, 0, 't'},
        {"log-level",   required_argument, 0, 'l'},
        {"log-file",    required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line arguments */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hs:p:j:d:f:wt:l:o:", 
                             long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
                
            case 's':
                strncpy(config.server_ip, optarg, sizeof(config.server_ip) - 1);
                break;
                
            case 'p':
                config.server_port = atoi(optarg);
                if (config.server_port <= 0 || config.server_port > 65535) {
                    fprintf(stderr, "Invalid server port number: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'j':
                job_type = atoi(optarg);
                if (job_type <= 0) {
                    fprintf(stderr, "Invalid job type: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'd':
                input_data = strdup(optarg);
                input_size = strlen(optarg);
                break;
                
            case 'f':
                if (read_file(optarg, &input_data, &input_size) < 0) {
                    return 1;
                }
                break;
                
            case 'w':
                wait_for_completion = 1;
                break;
                
            case 't':
                timeout_seconds = atoi(optarg);
                if (timeout_seconds <= 0) {
                    fprintf(stderr, "Invalid timeout value: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'l':
                log_level = atoi(optarg);
                if (log_level < 0 || log_level > 5) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'o':
                strncpy(log_file, optarg, sizeof(log_file) - 1);
                break;
                
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Validate required arguments */
    if (job_type < 0) {
        fprintf(stderr, "Job type is required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Initialize logger */
    if (strlen(log_file) > 0) {
        if (logger_init("client", log_file, log_level) != 0) {
            fprintf(stderr, "Failed to initialize logger with file: %s\n", log_file);
            return 1;
        }
    } else {
        logger_init("client", NULL, log_level);
    }
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize client */
    if (client_init(&config) != 0) {
        LOG_FATAL("Failed to initialize client");
        logger_close();
        return 1;
    }
    
    /* Connect to server */
    if (client_connect_to_server() != 0) {
        LOG_FATAL("Failed to connect to server");
        client_cleanup();
        logger_close();
        return 1;
    }
    
    /* Submit job */
    int job_id = client_submit_job(job_type, input_data, input_size);
    if (job_id < 0) {
        LOG_FATAL("Failed to submit job");
        client_cleanup();
        logger_close();
        return 1;
    }
    
    LOG_INFO("Job submitted successfully, job ID: %d", job_id);
    
    /* Free input data if allocated */
    if (input_data) {
        free(input_data);
    }
    
    /* Wait for completion if requested */
    if (wait_for_completion) {
        LOG_INFO("Waiting for job completion (timeout: %d seconds)...", timeout_seconds);
        
        int status = client_wait_for_job(job_id, timeout_seconds);
        if (status < 0) {
            LOG_ERROR("Error waiting for job completion");
            client_cleanup();
            logger_close();
            return 1;
        }
        
        switch (status) {
            case JOB_STATUS_COMPLETED:
                LOG_INFO("Job completed successfully");
                
                /* Get job result */
                char *result_data = NULL;
                size_t result_size = 0;
                
                if (client_get_job_result(job_id, &result_data, &result_size) == 0) {
                    if (result_size > 0) {
                        LOG_INFO("Job result (size: %zu):", result_size);
                        fwrite(result_data, 1, result_size, stdout);
                        printf("\n");
                    } else {
                        LOG_INFO("Job completed with empty result");
                    }
                    
                    if (result_data) {
                        free(result_data);
                    }
                } else {
                    LOG_ERROR("Failed to get job result");
                }
                break;
                
            case JOB_STATUS_FAILED:
                LOG_ERROR("Job failed");
                break;
                
            case JOB_STATUS_TIMEOUT:
                LOG_ERROR("Job timed out");
                break;
                
            default:
                LOG_ERROR("Unexpected job status: %d", status);
                break;
        }
    }
    
    /* Cleanup */
    client_cleanup();
    logger_close();
    
    return 0;
}
