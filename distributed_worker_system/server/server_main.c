/**
 * @file server_main.c
 * @brief Server main function for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "server.h"
#include "../common/logger.h"
#include "../common/common.h"

/* Default configuration values */
#define DEFAULT_IP_ADDRESS     ""
#define DEFAULT_PORT           8080
#define DEFAULT_MAX_CLIENTS    100
#define DEFAULT_MAX_WORKERS    50
#define DEFAULT_MAX_JOBS       1000
#define DEFAULT_JOB_TIMEOUT    300
#define DEFAULT_LOG_LEVEL      LOG_LEVEL_INFO

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
    printf("  -h, --help                 Display this help message\n");
    printf("  -a, --address IP           Server IP address (default: all interfaces)\n");
    printf("  -p, --port PORT            Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -c, --max-clients NUM      Maximum number of clients (default: %d)\n", DEFAULT_MAX_CLIENTS);
    printf("  -w, --max-workers NUM      Maximum number of workers (default: %d)\n", DEFAULT_MAX_WORKERS);
    printf("  -j, --max-jobs NUM         Maximum number of jobs (default: %d)\n", DEFAULT_MAX_JOBS);
    printf("  -t, --job-timeout SECONDS  Job timeout in seconds (default: %d)\n", DEFAULT_JOB_TIMEOUT);
    printf("  -l, --log-level LEVEL      Log level (0-5, default: %d)\n", DEFAULT_LOG_LEVEL);
    printf("                             0=TRACE, 1=DEBUG, 2=INFO, 3=WARNING, 4=ERROR, 5=FATAL\n");
    printf("  -f, --log-file FILE        Log file (default: stdout)\n");
}

int main(int argc, char *argv[]) {
    /* Configuration with default values */
    ServerConfig config;
    memset(&config, 0, sizeof(ServerConfig));
    strncpy(config.ip_address, DEFAULT_IP_ADDRESS, sizeof(config.ip_address) - 1);
    config.port = DEFAULT_PORT;
    config.max_clients = DEFAULT_MAX_CLIENTS;
    config.max_workers = DEFAULT_MAX_WORKERS;
    config.max_jobs = DEFAULT_MAX_JOBS;
    config.job_timeout_seconds = DEFAULT_JOB_TIMEOUT;
    
    int log_level = DEFAULT_LOG_LEVEL;
    char log_file[256] = "";
    
    /* Define command line options */
    static struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"address",     required_argument, 0, 'a'},
        {"port",        required_argument, 0, 'p'},
        {"max-clients", required_argument, 0, 'c'},
        {"max-workers", required_argument, 0, 'w'},
        {"max-jobs",    required_argument, 0, 'j'},
        {"job-timeout", required_argument, 0, 't'},
        {"log-level",   required_argument, 0, 'l'},
        {"log-file",    required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line arguments */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "ha:p:c:w:j:t:l:f:", 
                             long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
                
            case 'a':
                strncpy(config.ip_address, optarg, sizeof(config.ip_address) - 1);
                break;
                
            case 'p':
                config.port = atoi(optarg);
                if (config.port <= 0 || config.port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'c':
                config.max_clients = atoi(optarg);
                if (config.max_clients <= 0) {
                    fprintf(stderr, "Invalid max clients: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'w':
                config.max_workers = atoi(optarg);
                if (config.max_workers <= 0) {
                    fprintf(stderr, "Invalid max workers: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'j':
                config.max_jobs = atoi(optarg);
                if (config.max_jobs <= 0) {
                    fprintf(stderr, "Invalid max jobs: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 't':
                config.job_timeout_seconds = atoi(optarg);
                if (config.job_timeout_seconds <= 0) {
                    fprintf(stderr, "Invalid job timeout: %s\n", optarg);
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
                
            case 'f':
                strncpy(log_file, optarg, sizeof(log_file) - 1);
                break;
                
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Initialize logger */
    if (strlen(log_file) > 0) {
        if (log_init(log_level, log_file) != 0) {
            fprintf(stderr, "Failed to initialize logger with file: %s\n", log_file);
            return 1;
        }
    } else {
        log_init(log_level, NULL);
    }
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize server */
    if (server_init(&config) != 0) {
        log_fatal("Failed to initialize server");
        log_cleanup();
        return 1;
    }
    
    /* Start server */
    if (server_start() != 0) {
        log_fatal("Failed to start server");
        log_cleanup();
        return 1;
    }
    
    log_info("Server started on %s:%d", 
             strlen(config.ip_address) > 0 ? config.ip_address : "0.0.0.0", 
             config.port);
    
    /* Main loop */
    while (g_running) {
        /* Check for timed out jobs */
        int timeout_count = job_handler_check_timeouts();
        if (timeout_count > 0) {
            log_info("Handled %d timed out jobs", timeout_count);
        }
        
        /* Check for inactive workers */
        int inactive_count = worker_manager_check_inactive(60); /* 60 seconds timeout */
        if (inactive_count > 0) {
            log_info("Marked %d workers as inactive", inactive_count);
        }
        
        /* Print statistics periodically */
        static int stats_counter = 0;
        if (++stats_counter >= 10) { /* Every 10 seconds */
            stats_counter = 0;
            
            int active_clients, active_workers;
            int pending_jobs, running_jobs, completed_jobs, failed_jobs;
            
            if (server_get_stats(&active_clients, &active_workers, 
                                &pending_jobs, &running_jobs, 
                                &completed_jobs, &failed_jobs) == 0) {
                log_info("Stats: workers=%d, jobs(pending=%d, running=%d, completed=%d, failed=%d)",
                         active_workers, pending_jobs, running_jobs, completed_jobs, failed_jobs);
            }
        }
        
        /* Sleep for a while */
        sleep(1);
    }
    
    /* Stop server */
    log_info("Stopping server...");
    server_stop();
    
    /* Cleanup */
    log_cleanup();
    
    return 0;
}