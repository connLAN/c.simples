#include "net_traffic_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Declare extern variables from net_traffic_analyzer.c
extern ConnectionRecord* connections;
extern size_t connection_count;
extern IPStats* ip_stats;
extern size_t ip_stats_count;

int main() {
    // Initialize test connections
    connection_count = 5;
    connections = malloc(connection_count * sizeof(ConnectionRecord));
    
    time_t now = time(NULL);
    
    // Create test data (5 connections from 3 IPs over 2 days)
    for (int i = 0; i < 5; i++) {
        connections[i].timestamp = now - (i % 2) * 86400; // Alternate days
        snprintf(connections[i].ip, 16, "192.168.1.%d", (i % 3) + 1);
        connections[i].bytes = 1000 * (i + 1);
    }

    // Initialize IP stats (mark one IP as suspicious)
    ip_stats_count = 1;
    ip_stats = malloc(ip_stats_count * sizeof(IPStats));
    strcpy(ip_stats[0].ip, "192.168.1.2");
    ip_stats[0].is_suspicious = 1;

    // Test daily report
    size_t count;
    TrafficReport* reports = generate_daily_report(now, &count);
    
    if (reports) {
        printf("Daily Report:\n");
        printf("Date\t\tTotal\tConnections\tUnique IPs\tSuspicious IPs\n");
        for (size_t i = 0; i < count; i++) {
            if (reports[i].total_bytes > 0) {
                printf("%s\t%lu\t%u\t\t%u\t\t%u\n",
                       reports[i].period,
                       reports[i].total_bytes,
                       reports[i].total_connections,
                       reports[i].unique_ips,
                       reports[i].suspicious_ips);
            }
        }
        free(reports);
    }

    free(connections);
    free(ip_stats);
    return 0;
}