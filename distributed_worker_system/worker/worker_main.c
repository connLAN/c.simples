/**
 * @file worker_main.c
 * @brief Worker main function for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "worker.h"
#include "../common/logger.h"
#include "../common/common.h"

/* Default configuration values */
#define DEFAULT_SERVER_IP              "127.0.0.1"
#define DEFAULT_SERVER_PORT            8080
#define DEFAULT_WORKER_IP              "127.0.0.1"
#define DEFAULT_WORKER_PORT            0
#define DEFAULT_MAX_CONCURRENT_JOBS    4
#define DEFAULT_RECONNECT_INTERVAL     5
#define DEFAULT_HEARTBEAT_INTERVAL     30
#define DEFAULT_LOG_LEVEL              LOG_LEVEL_INFO

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
    printf("  -h, --help                   Display this help message\n");
    printf("  -s, --server-ip IP           Server IP address (default: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --server-port PORT       Server port (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  -i, --worker-ip IP           Worker IP address (default: %s)\n", DEFAULT_WORKER_IP);
    printf("  -o, --worker-port PORT       Worker port (default: %d)\n", DEFAULT_WORKER_PORT);
    printf("  -j, --max-jobs NUM           Maximum concurrent jobs (default: %d)\n", DEFAULT_MAX_CONCURRENT_JOBS);
    printf("  -r, --reconnect-interval SEC Reconnect interval in seconds (default: %d)\n", DEFAULT_RECONNECT_INTERVAL);
    printf("  -b, --heartbeat-interval SEC Heartbeat interval in seconds (default: %d)\n", DEFAULT_HEARTBEAT_INTERVAL);
    printf("  -t, --job-types TYPES        Comma-separated list of supported job types\n");
    printf("                               (default: all types)\n");
    printf("  -l, --log-level LEVEL        Log level (0-5, default: %d)\n", DEFAULT_LOG_LEVEL);
    printf("                               0=TRACE, 1=DEBUG, 2=INFO, 3=WARNING, 4=ERROR, 5=FATAL\n");
    printf("  -f, --log-file FILE          Log file (default: stdout)\n");
}

int main(int argc, char *argv[]) {
    /* Configuration with default values */
    WorkerConfig config;
    memset(&config, 0, sizeof(WorkerConfig));
    strncpy(config.server_ip, DEFAULT_SERVER_IP, sizeof(config.server_ip) - 1);
    config.server_port = DEFAULT_SERVER_PORT;
    strncpy(config.worker_ip, DEFAULT_WORKER_IP, sizeof(config.worker_ip) - 1);
    config.worker_port = DEFAULT_WORKER_PORT;
    config.max_concurrent_jobs = DEFAULT_MAX_CONCURRENT_JOBS;
    config.reconnect_interval_seconds = DEFAULT_RECONNECT_INTERVAL;
    config.heartbeat_interval_seconds = DEFAULT_HEARTBEAT_INTERVAL;
    
    /* Default job types (all types) */
    config.job_types[0] = JOB_TYPE_ECHO;
    config.job_types[1] = JOB_TYPE_REVERSE;
    config.job_types[2] = JOB_TYPE_UPPERCASE;
    config.num_job_types = 3;
    
    int log_level = DEFAULT_LOG_LEVEL;
    char log_file[256] = "";
    
    /* Define command line options */
    static struct option long_options[] = {
        {"help",                no_argument,       0, 'h'},
        {"server-ip",           required_argument, 0, 's'},
        {"server-port",         required_argument, 0, 'p'},
        {"worker-ip",           required_argument, 0, 'i'},
        {"worker-port",         required_argument, 0, 'o'},
        {"max-jobs",            required_argument, 0, 'j'},
        {"reconnect-interval",  required_argument, 0, 'r'},
        {"heartbeat-interval",  required_argument, 0, 'b'},
        {"job-types",           required_argument, 0, 't'},
        {"log-level",           required_argument, 0, 'l'},
        {"log-file",            required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line arguments */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hs:p:i:o:j:r:b:t:l:f:", 
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
                
            case 'i':
                strncpy(config.worker_ip, optarg, sizeof(config.worker_ip) - 1);
                break;
                
            case 'o':
                config.worker_port = atoi(optarg);
                if (config.worker_port < 0 || config.worker_port > 65535) {
                    fprintf(stderr, "Invalid worker port number: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'j':
                config.max_concurrent_jobs = atoi(optarg);
                if (config.max_concurrent_jobs <= 0) {
                    fprintf(stderr, "Invalid max concurrent jobs: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'r':
                config.reconnect_interval_seconds = atoi(optarg);
                if (config.reconnect_interval_seconds <= 0) {
                    fprintf(stderr, "Invalid reconnect interval: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 'b':
                config.heartbeat_interval_seconds = atoi(optarg);
                if (config.heartbeat_interval_seconds <= 0) {
                    fprintf(stderr, "Invalid heartbeat interval: %s\n", optarg);
                    return 1;
                }
                break;
                
            case 't': {
                /* Parse comma-separated job types */
                char *token;
                char *str = strdup(optarg);
                char *saveptr;
                int count = 0;
                
                token = strtok_r(str, ",", &saveptr);
                while (token && count < MAX_JOB_TYPES) {
                    int job_type = atoi(token);
                    if (job_type <= 0) {
                        fprintf(stderr, "Invalid job type: %s\n", token);
                        free(str);
                        return 1;
                    }
                    
                    config.job_types[count++] = job_type;
                    token = strtok_r(NULL, ",", &saveptr);
                }
                
                config.num_job_types = count;
                free(str);
                break;
            }
                
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
    
    /* Initialize worker */
    if (worker_init(&config) != 0) {
        log_fatal("Failed to initialize worker");
        log_cleanup();
        return 1;
    }
    
    /* Start worker */
    if (worker_start() != 0) {
        log_fatal("Failed to start worker");
        log_cleanup();
        return 1;
    }
    
    log_info("Worker started, connecting to server %s:%d", 
             config.server_ip, config.server_port);
    
    /* Main loop */
    while (g_running) {
        /* Print statistics periodically */
        static int stats_counter = 0;
        if (++stats_counter >= 10) { /* Every 10 seconds */
            stats_counter = 0;
            
            int jobs_processed, jobs_failed;
            double avg_processing_time_ms;
            
            if (worker_get_stats(&jobs_processed, &jobs_failed, &avg_processing_time_ms) == 0) {
                log_info("Stats: jobs(processed=%d, failed=%d), avg_time=%.2f ms",
                         jobs_processed, jobs_failed, avg_processing_time_ms);
            }
        }
        
        /* Sleep for a while */
        sleep(1);
    }
    
    /* Stop worker */
    log_info("Stopping worker...");
    worker_stop();
    
    /* Cleanup */
    log_cleanup();
    
    return 0;
}