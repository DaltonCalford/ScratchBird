# Clock Synchronization Specification

**Document Version:** 1.0  
**Date:** 2025-01-25  
**Status:** Draft Specification

---

## Overview

This document specifies the cluster-wide clock synchronization system that provides globally ordered timestamps for distributed MVCC. The system uses a heartbeat protocol to maintain bounded clock uncertainty across all nodes.

---

## Design Goals

1. **Bounded Uncertainty**: All nodes know maximum clock error (ε)
2. **Low Latency**: Minimal overhead for timestamp generation
3. **High Availability**: Automatic failover for clock masters
4. **Flexible Deployment**: Software sync (cloud) to hardware sync (on-prem)
5. **Observable**: Clear metrics for sync quality and degradation

---

## Clock Service Architecture

### Component Diagram

```
┌───────────────────────────────────────────────────┐
│  Clock Master Service (Primary)                   │
│  ┌─────────────────────────────────────────────┐  │
│  │  Time Source                                │  │
│  │  - AWS Time Sync (cloud) OR                │  │
│  │  - GPS Receiver (hardware) OR               │  │
│  │  - PTP Grandmaster (hardware) OR            │  │
│  │  - Chrony NTP (generic)                     │  │
│  └─────────────────────────────────────────────┘  │
│                                                    │
│  ┌─────────────────────────────────────────────┐  │
│  │  Heartbeat Generator                        │  │
│  │  - Frequency: 10-20 Hz (50-100ms interval) │  │
│  │  - Uncertainty measurement                  │  │
│  │  - Sequence numbering                       │  │
│  │  - Multicast/UDP broadcast                  │  │
│  └─────────────────────────────────────────────┘  │
│                                                    │
│  ┌─────────────────────────────────────────────┐  │
│  │  Cluster Membership Tracker                 │  │
│  │  - Node registration                        │  │
│  │  - Liveness monitoring                      │  │
│  │  - Load reporting                           │  │
│  └─────────────────────────────────────────────┘  │
└────────────────────┬──────────────────────────────┘
                     │
         Heartbeat Messages (UDP Multicast)
         Frequency: 50-100ms
                     │
    ┌────────────────┼────────────────┬────────────┐
    │                │                │            │
┌───▼───┐      ┌─────▼────┐    ┌─────▼────┐  ┌───▼────┐
│TX Eng │      │ TX Eng   │    │ Ingestion│  │  OLAP  │
│   1   │      │    2     │    │  Engine  │  │  Shard │
│       │      │          │    │          │  │        │
│ Local │      │  Local   │    │  Local   │  │ Local  │
│ Clock │      │  Clock   │    │  Clock   │  │ Clock  │
│ State │      │  State   │    │  State   │  │ State  │
└───────┘      └──────────┘    └──────────┘  └────────┘
```

---

## Heartbeat Protocol

### Message Format

```c
// Clock heartbeat message (UDP multicast)
struct ClockHeartbeat {
    // Protocol identification
    uint32_t magic;                      // 0xCL0CK01 (protocol version 1)
    
    // Sequence and timing
    uint64_t sequence_number;            // Monotonic, detect missed beats
    uint64_t master_timestamp_ns;        // Nanosecond precision
    
    // Uncertainty bounds
    uint32_t measured_uncertainty_ns;    // Measured sync quality
    uint32_t max_uncertainty_ns;         // Conservative worst-case
    
    // Clock source information
    uint8_t source_type;                 // ClockSource enum
    uint8_t stratum;                     // NTP stratum or equivalent
    uint32_t jitter_ns;                  // Short-term variation
    uint32_t wander_ns;                  // Long-term drift
    
    // Cluster coordination
    uint32_t cluster_epoch;              // Increments on membership change
    uint8_t master_id;                   // Primary=0, Backup=1
    uint8_t flags;                       // FAILOVER, DEGRADED, etc.
    
    // Checksum
    uint32_t crc32;                      // Message integrity
} __attribute__((packed));

// Total size: 56 bytes
```

### Clock Source Types

```c
enum ClockSource {
    CLOCK_SOURCE_GPS = 0,               // Stratum 0 (hardware)
    CLOCK_SOURCE_PTP_HARDWARE = 1,      // PTP with HW timestamping
    CLOCK_SOURCE_AWS_TIME_SYNC = 2,     // AWS specific (169.254.169.123)
    CLOCK_SOURCE_GCP_NTP = 3,           // GCP (metadata.google.internal)
    CLOCK_SOURCE_AZURE_NTP = 4,         // Azure (time.windows.com)
    CLOCK_SOURCE_PTP_SOFTWARE = 5,      // Software PTP
    CLOCK_SOURCE_CHRONY = 6,            // Chrony NTP client
    CLOCK_SOURCE_NTPD = 7,              // Standard NTP daemon
    CLOCK_SOURCE_LOCAL = 8,             // Fallback (not synced)
};

// Expected uncertainty by source type
const uint32_t SOURCE_TYPE_UNCERTAINTY[] = {
    [CLOCK_SOURCE_GPS] = 100,                    // 100ns
    [CLOCK_SOURCE_PTP_HARDWARE] = 500,           // 500ns
    [CLOCK_SOURCE_AWS_TIME_SYNC] = 1000000,      // 1ms
    [CLOCK_SOURCE_GCP_NTP] = 2000000,            // 2ms
    [CLOCK_SOURCE_AZURE_NTP] = 10000000,         // 10ms
    [CLOCK_SOURCE_PTP_SOFTWARE] = 1000000,       // 1ms
    [CLOCK_SOURCE_CHRONY] = 5000000,             // 5ms
    [CLOCK_SOURCE_NTPD] = 10000000,              // 10ms
    [CLOCK_SOURCE_LOCAL] = 1000000000,           // 1s (degraded)
};
```

### Heartbeat Flags

```c
#define HEARTBEAT_FLAG_FAILOVER     0x01    // Backup master took over
#define HEARTBEAT_FLAG_DEGRADED     0x02    // High uncertainty
#define HEARTBEAT_FLAG_GPS_LOCK     0x04    // GPS has satellite lock
#define HEARTBEAT_FLAG_RESYNC       0x08    // Clock jumped (NTP step)
#define HEARTBEAT_FLAG_MAINTENANCE  0x10    // Planned maintenance mode
```

---

## Clock Master Implementation

### Uncertainty Measurement

```c
// Continuously measure clock synchronization quality
void clock_master_measure_uncertainty() {
    const int NUM_SAMPLES = 100;
    int64_t offsets[NUM_SAMPLES];
    uint64_t sample_times[NUM_SAMPLES];
    
    // Collect samples
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sample_times[i] = read_monotonic_clock_ns();
        offsets[i] = measure_clock_offset_to_reference();
        
        // 100μs between samples (10ms total sampling window)
        nanosleep(100000);
    }
    
    // Calculate statistics
    Statistics stats = calculate_statistics(offsets, NUM_SAMPLES);
    
    // Uncertainty is 3σ (99.7% confidence)
    uint32_t statistical_uncertainty = stats.stddev * 3;
    
    // Add measurement noise and network latency variance
    uint32_t measurement_noise = stats.max_deviation / 2;
    uint32_t network_variance = estimate_network_variance();
    
    uint32_t measured_uncertainty = statistical_uncertainty +
                                    measurement_noise +
                                    network_variance;
    
    // Use higher of measured vs theoretical
    uint32_t theoretical = SOURCE_TYPE_UNCERTAINTY[clock_source_type];
    heartbeat.measured_uncertainty_ns = MAX(measured_uncertainty, theoretical);
    
    // Conservative max is 2x measured
    heartbeat.max_uncertainty_ns = measured_uncertainty * 2;
    
    // Log significant changes
    if (measured_uncertainty > previous_uncertainty * 1.5) {
        log_warn("Clock uncertainty increased: %u ns → %u ns",
                 previous_uncertainty, measured_uncertainty);
    }
}

// Estimate network latency variance
uint32_t estimate_network_variance() {
    // Ping all known cluster nodes
    uint64_t latencies[MAX_NODES];
    int count = 0;
    
    for (int i = 0; i < cluster_node_count; i++) {
        uint64_t rtt = ping_node(cluster_nodes[i]);
        if (rtt < REASONABLE_RTT_NS) {
            latencies[count++] = rtt;
        }
    }
    
    if (count == 0) return DEFAULT_NETWORK_VARIANCE;
    
    Statistics stats = calculate_statistics(latencies, count);
    return stats.stddev;  // Standard deviation of RTT
}
```

### Heartbeat Generation

```c
// Heartbeat generator thread
void* clock_master_heartbeat_thread(void* arg) {
    uint64_t sequence = 0;
    
    while (running) {
        // Measure current uncertainty
        clock_master_measure_uncertainty();
        
        // Build heartbeat message
        ClockHeartbeat hb = {0};
        hb.magic = 0xCL0CK01;
        hb.sequence_number = ++sequence;
        hb.master_timestamp_ns = read_reference_clock_ns();
        hb.measured_uncertainty_ns = measured_uncertainty;
        hb.max_uncertainty_ns = max_uncertainty;
        hb.source_type = clock_source_type;
        hb.stratum = get_stratum();
        hb.jitter_ns = calculate_jitter();
        hb.wander_ns = calculate_wander();
        hb.cluster_epoch = cluster_epoch;
        hb.master_id = is_primary ? 0 : 1;
        hb.flags = get_clock_flags();
        hb.crc32 = calculate_crc32(&hb, sizeof(hb) - 4);
        
        // Broadcast to cluster
        send_multicast_heartbeat(&hb);
        
        // 50ms interval (20 Hz)
        nanosleep(50000000);
    }
    
    return NULL;
}

// Send heartbeat via UDP multicast
void send_multicast_heartbeat(ClockHeartbeat* hb) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);  // 239.255.0.1
    addr.sin_port = htons(HEARTBEAT_PORT);  // 5555
    
    ssize_t sent = sendto(multicast_sock, hb, sizeof(*hb), 0,
                         (struct sockaddr*)&addr, sizeof(addr));
    
    if (sent != sizeof(*hb)) {
        log_error("Failed to send heartbeat: %s", strerror(errno));
        heartbeat_send_failures++;
    }
}
```

### Master Failover

```c
// Backup clock master monitors primary health
void* backup_clock_master_monitor(void* arg) {
    uint64_t last_primary_seq = 0;
    int missed_heartbeats = 0;
    
    while (running) {
        // Wait for heartbeat (with timeout)
        ClockHeartbeat hb;
        int received = receive_heartbeat_timeout(&hb, FAILOVER_TIMEOUT_MS);
        
        if (!received) {
            missed_heartbeats++;
            
            if (missed_heartbeats >= FAILOVER_THRESHOLD) {
                log_critical("Primary clock master unresponsive, taking over");
                
                // Promote to primary
                become_primary_clock_master();
                return NULL;
            }
        } else if (hb.master_id == 0) {
            // Primary is alive
            missed_heartbeats = 0;
            last_primary_seq = hb.sequence_number;
        }
        
        sleep_ms(100);
    }
    
    return NULL;
}

// Promote backup to primary
void become_primary_clock_master() {
    is_primary = true;
    cluster_epoch++;  // Increment epoch on failover
    
    log_info("Promoted to primary clock master (epoch %u)", cluster_epoch);
    
    // Start heartbeat generation
    pthread_create(&heartbeat_thread, NULL, 
                  clock_master_heartbeat_thread, NULL);
}
```

---

## Clock Client Implementation

### Local Clock State

```c
// Each node maintains local clock state
struct LocalClockState {
    // Last sync information
    uint64_t last_sync_timestamp_ns;      // Master time at last sync
    uint64_t last_sync_local_time_ns;     // Local time at last sync
    int64_t clock_offset_ns;              // Adjustment to apply
    
    // Uncertainty tracking
    uint32_t uncertainty_bound_ns;         // Current uncertainty
    uint32_t min_uncertainty_ns;           // Best achieved
    uint32_t max_uncertainty_ns;           // Worst seen
    
    // Heartbeat tracking
    uint64_t last_heartbeat_seq;           // Last received sequence
    uint64_t missed_heartbeat_count;       // Missed beats
    uint64_t total_heartbeats_received;    // Statistics
    
    // Synchronization state
    bool synchronized;                     // Are we synced?
    uint64_t last_sync_time_ms;           // When did we last sync?
    ClockSource source_type;               // What's our time source?
    
    // Drift tracking
    int64_t drift_rate_ppb;               // Parts per billion
    uint64_t drift_measurement_start;      // For calculating drift
};

// Global clock state
LocalClockState clock_state = {0};
```

### Heartbeat Reception and Processing

```c
// Thread to receive and process heartbeats
void* clock_client_receiver_thread(void* arg) {
    int sock = create_multicast_receiver(MULTICAST_GROUP, HEARTBEAT_PORT);
    
    while (running) {
        ClockHeartbeat hb;
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        
        ssize_t received = recvfrom(sock, &hb, sizeof(hb), 0,
                                    (struct sockaddr*)&from, &from_len);
        
        if (received != sizeof(hb)) {
            log_warn("Incomplete heartbeat received: %zd bytes", received);
            continue;
        }
        
        // Validate message
        if (hb.magic != 0xCL0CK01) {
            log_warn("Invalid heartbeat magic: 0x%x", hb.magic);
            continue;
        }
        
        uint32_t crc = calculate_crc32(&hb, sizeof(hb) - 4);
        if (crc != hb.crc32) {
            log_warn("Heartbeat CRC mismatch");
            continue;
        }
        
        // Record receive timestamp ASAP
        uint64_t receive_time_local = read_local_clock_ns();
        
        // Process heartbeat
        process_clock_heartbeat(&hb, receive_time_local);
    }
    
    return NULL;
}

// Process received heartbeat
void process_clock_heartbeat(ClockHeartbeat* hb, uint64_t receive_time_local) {
    // Detect missed heartbeats
    uint64_t expected_seq = clock_state.last_heartbeat_seq + 1;
    if (hb->sequence_number > expected_seq) {
        uint64_t missed = hb->sequence_number - expected_seq;
        clock_state.missed_heartbeat_count += missed;
        log_warn("Missed %lu heartbeats", missed);
    }
    
    // Detect clock master failover
    if (hb->flags & HEARTBEAT_FLAG_FAILOVER) {
        log_info("Clock master failover detected (epoch %u)", hb->cluster_epoch);
    }
    
    // Detect clock jump/resync
    if (hb->flags & HEARTBEAT_FLAG_RESYNC) {
        log_warn("Clock master reports resync (time may have jumped)");
        // Increase uncertainty temporarily
        clock_state.uncertainty_bound_ns *= 2;
    }
    
    // Calculate network latency (approximate)
    // In production, use timestamping on both ends
    uint64_t estimated_one_way_latency = network_rtt_estimate / 2;
    
    // Adjust master timestamp for network delay
    uint64_t adjusted_master_time = hb->master_timestamp_ns + 
                                    estimated_one_way_latency;
    
    // Calculate offset
    int64_t offset = (int64_t)adjusted_master_time - 
                     (int64_t)receive_time_local;
    
    // Apply smoothing (Exponential Moving Average)
    // New offset = 0.9 * old + 0.1 * measured
    const double ALPHA = 0.1;
    clock_state.clock_offset_ns = (int64_t)(
        (1.0 - ALPHA) * clock_state.clock_offset_ns +
        ALPHA * offset
    );
    
    // Update uncertainty
    uint64_t time_since_last_sync = receive_time_local - 
                                    clock_state.last_sync_local_time_ns;
    
    // Uncertainty grows with drift over time
    uint32_t drift_uncertainty = (time_since_last_sync * 
                                 clock_state.drift_rate_ppb) / 1000000000;
    
    clock_state.uncertainty_bound_ns = hb->measured_uncertainty_ns +
                                       drift_uncertainty +
                                       estimated_network_variance;
    
    // Update state
    clock_state.last_sync_timestamp_ns = adjusted_master_time;
    clock_state.last_sync_local_time_ns = receive_time_local;
    clock_state.last_heartbeat_seq = hb->sequence_number;
    clock_state.total_heartbeats_received++;
    clock_state.synchronized = true;
    clock_state.source_type = hb->source_type;
    
    // Track best and worst uncertainty
    if (hb->measured_uncertainty_ns < clock_state.min_uncertainty_ns ||
        clock_state.min_uncertainty_ns == 0) {
        clock_state.min_uncertainty_ns = hb->measured_uncertainty_ns;
    }
    if (hb->measured_uncertainty_ns > clock_state.max_uncertainty_ns) {
        clock_state.max_uncertainty_ns = hb->measured_uncertainty_ns;
    }
}
```

### Clock Time Query

```c
// Get current cluster time with uncertainty
struct ClusterTime {
    uint64_t timestamp_ns;
    uint32_t uncertainty_ns;
};

ClusterTime get_cluster_time() {
    if (!clock_state.synchronized) {
        log_error("Clock not synchronized!");
        return fallback_time();
    }
    
    // Read local monotonic clock
    uint64_t local_now = read_monotonic_clock_ns();
    
    // Calculate elapsed time since last sync
    uint64_t elapsed_since_sync = local_now - clock_state.last_sync_local_time_ns;
    
    // Calculate cluster time
    ClusterTime ct;
    ct.timestamp_ns = clock_state.last_sync_timestamp_ns +
                      elapsed_since_sync +
                      clock_state.clock_offset_ns;
    
    // Uncertainty grows with time since last sync
    uint32_t drift_uncertainty = (elapsed_since_sync *
                                 clock_state.drift_rate_ppb) / 1000000000;
    
    ct.uncertainty_ns = clock_state.uncertainty_bound_ns + drift_uncertainty;
    
    // Check if uncertainty is too high
    if (ct.uncertainty_ns > MAX_ACCEPTABLE_UNCERTAINTY_NS) {
        log_warn("Clock uncertainty too high: %u ns", ct.uncertainty_ns);
    }
    
    return ct;
}

// Fallback when clock sync is lost
ClusterTime fallback_time() {
    ClusterTime ct;
    ct.timestamp_ns = read_local_clock_ns();
    ct.uncertainty_ns = 1000000000;  // 1 second (very conservative)
    return ct;
}
```

### Clock Synchronization Loss Detection

```c
// Monitor thread to detect loss of sync
void* clock_sync_monitor_thread(void* arg) {
    while (running) {
        uint64_t now = get_monotonic_time_ms();
        uint64_t time_since_sync = now - clock_state.last_sync_time_ms;
        
        // Check if we haven't received a heartbeat recently
        if (time_since_sync > SYNC_TIMEOUT_MS) {
            log_critical("Lost clock synchronization! Last sync %lu ms ago",
                        time_since_sync);
            
            clock_state.synchronized = false;
            
            // Handle based on consistency mode
            handle_clock_sync_loss();
        }
        
        // Check if uncertainty is too high
        ClusterTime ct = get_cluster_time();
        if (ct.uncertainty_ns > CRITICAL_UNCERTAINTY_NS) {
            log_error("Clock uncertainty critical: %u ns", ct.uncertainty_ns);
            
            // Alert monitoring system
            send_alert(ALERT_HIGH_UNCERTAINTY, ct.uncertainty_ns);
        }
        
        sleep_ms(1000);  // Check every second
    }
    
    return NULL;
}

// Handle loss of clock synchronization
void handle_clock_sync_loss() {
    if (STRICT_CONSISTENCY_MODE) {
        log_critical("Pausing writes due to clock sync loss");
        
        // Stop accepting new transactions
        set_node_state(NODE_STATE_DEGRADED);
        
        // Wait for resynchronization
        while (!clock_state.synchronized) {
            sleep_ms(100);
        }
        
        log_info("Clock resynchronized, resuming operations");
        set_node_state(NODE_STATE_ACTIVE);
        
    } else {
        log_warn("Operating in degraded mode with local clock");
        
        // Continue with local clock (high uncertainty)
        // Mark transactions as potentially out-of-order
    }
}
```

---

## Cloud Provider Integration

### AWS Time Sync Service

```c
// Initialize AWS Time Sync
void init_aws_time_sync() {
    const char* chrony_conf = 
        "# AWS Time Sync Service\n"
        "server 169.254.169.123 prefer iburst minpoll 4 maxpoll 4\n"
        "\n"
        "# Make step adjustments quickly\n"
        "makestep 0.1 3\n"
        "\n"
        "# Sync system RTC\n"
        "rtcsync\n"
        "\n"
        "# Logging\n"
        "logdir /var/log/chrony\n"
        "log measurements statistics tracking\n";
    
    write_file("/etc/chrony/chrony.conf", chrony_conf);
    
    // Restart chrony
    system("systemctl restart chronyd");
    
    // Wait for initial sync
    sleep(5);
    
    // Verify sync
    if (!verify_chrony_sync()) {
        log_error("Failed to sync with AWS Time Sync");
        return;
    }
    
    clock_source_type = CLOCK_SOURCE_AWS_TIME_SYNC;
    log_info("Initialized AWS Time Sync");
}

// Verify chrony is synced
bool verify_chrony_sync() {
    // Execute: chronyc tracking
    FILE* fp = popen("chronyc tracking", "r");
    if (!fp) return false;
    
    char line[256];
    bool synced = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // Look for "Leap status     : Normal"
        if (strstr(line, "Leap status") && strstr(line, "Normal")) {
            synced = true;
        }
    }
    
    pclose(fp);
    return synced;
}
```

### GCP NTP Integration

```c
void init_gcp_ntp() {
    const char* chrony_conf =
        "# GCP NTP Service\n"
        "server metadata.google.internal prefer iburst minpoll 4 maxpoll 4\n"
        "makestep 0.1 3\n"
        "rtcsync\n";
    
    write_file("/etc/chrony/chrony.conf", chrony_conf);
    system("systemctl restart chronyd");
    sleep(5);
    
    clock_source_type = CLOCK_SOURCE_GCP_NTP;
    log_info("Initialized GCP NTP");
}
```

### Azure NTP Integration

```c
void init_azure_ntp() {
    const char* chrony_conf =
        "# Azure NTP Service\n"
        "server time.windows.com prefer iburst\n"
        "makestep 0.1 3\n"
        "rtcsync\n";
    
    write_file("/etc/chrony/chrony.conf", chrony_conf);
    system("systemctl restart chronyd");
    sleep(5);
    
    clock_source_type = CLOCK_SOURCE_AZURE_NTP;
    log_info("Initialized Azure NTP");
}
```

### Auto-Detection

```c
// Auto-detect cloud environment and configure time sync
void init_cloud_time_sync() {
    if (detect_aws()) {
        init_aws_time_sync();
    } else if (detect_gcp()) {
        init_gcp_ntp();
    } else if (detect_azure()) {
        init_azure_ntp();
    } else {
        init_generic_ntp();
    }
}

bool detect_aws() {
    // Check for AWS metadata service
    int ret = system("curl -s -m 1 http://169.254.169.254/latest/meta-data/ > /dev/null 2>&1");
    return (ret == 0);
}

bool detect_gcp() {
    // Check for GCP metadata service
    int ret = system("curl -s -m 1 -H 'Metadata-Flavor: Google' http://metadata.google.internal/ > /dev/null 2>&1");
    return (ret == 0);
}

bool detect_azure() {
    // Check for Azure metadata service
    int ret = system("curl -s -m 1 -H 'Metadata: true' http://169.254.169.254/metadata/instance?api-version=2021-02-01 > /dev/null 2>&1");
    return (ret == 0);
}
```

---

## Hardware Clock Sources (Future)

### GPS Receiver Integration

```c
// Initialize GPS receiver (e.g., u-blox NEO-M8T)
void init_gps_clock_source() {
    // Open GPS serial device
    int gps_fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
    if (gps_fd < 0) {
        log_error("Failed to open GPS device");
        return;
    }
    
    // Configure serial port (9600 baud, 8N1)
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    tty.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    
    tcsetattr(gps_fd, TCSANOW, &tty);
    
    // Start GPS reading thread
    pthread_create(&gps_thread, NULL, gps_reader_thread, &gps_fd);
    
    clock_source_type = CLOCK_SOURCE_GPS;
    log_info("Initialized GPS clock source");
}

// GPS reader thread
void* gps_reader_thread(void* arg) {
    int fd = *(int*)arg;
    char buffer[1024];
    
    while (running) {
        // Read NMEA sentences
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            
            // Parse NMEA sentences
            if (strstr(buffer, "$GPRMC")) {
                parse_gprmc(buffer);
            } else if (strstr(buffer, "$GPGGA")) {
                parse_gpgga(buffer);
            }
        }
    }
    
    return NULL;
}
```

### PTP (Precision Time Protocol)

```c
// Initialize PTP with hardware timestamping
void init_ptp_clock_source() {
    // Requires linuxptp package: ptp4l and phc2sys
    
    // Start PTP daemon
    system("ptp4l -i eth0 -s -H &");
    
    // Synchronize system clock to PTP hardware clock
    system("phc2sys -a -r &");
    
    sleep(10);  // Wait for initial sync
    
    clock_source_type = CLOCK_SOURCE_PTP_HARDWARE;
    log_info("Initialized PTP clock source");
}
```

---

## Uncertainty Budget

### Target Uncertainty by Deployment

| Deployment | Clock Source | Target ε | Acceptable Range |
|------------|--------------|----------|------------------|
| AWS | Time Sync Service | 1-2ms | 0.5-5ms |
| GCP | NTP (metadata) | 2-5ms | 1-10ms |
| Azure | NTP (time.windows.com) | 5-10ms | 2-20ms |
| Generic Cloud | NTP (pool.ntp.org) | 10-50ms | 5-100ms |
| On-Prem + GPS | GPS Receiver | 100-500ns | 50ns-1ms |
| On-Prem + PTP | PTP Grandmaster | 10-100ns | 10ns-1ms |

### Uncertainty Breakdown

```
Total Uncertainty = Base Sync + Network Variance + Drift + Safety Margin

AWS Example:
  Base Sync:         1ms (AWS Time Sync quality)
  Network Variance:  0.5ms (local network jitter)
  Drift:            0.1ms (since last heartbeat)
  Safety Margin:    0.4ms (conservative padding)
  ────────────────────────
  Total:            2ms

GPS Example:
  Base Sync:         100ns (GPS accuracy)
  Network Variance:  200ns (local network jitter)
  Drift:            50ns (since last heartbeat)
  Safety Margin:    150ns (conservative padding)
  ────────────────────────
  Total:            500ns
```

---

## Monitoring and Metrics

### Key Metrics

```sql
-- Clock synchronization status
SELECT 
    server_id,
    source_type,
    uncertainty_ns,
    last_sync_ms_ago,
    missed_heartbeats,
    drift_rate_ppb
FROM clock_sync_metrics
ORDER BY uncertainty_ns DESC;

-- Uncertainty distribution
SELECT 
    bucket_ms,
    COUNT(*) as server_count
FROM (
    SELECT 
        server_id,
        FLOOR(uncertainty_ns / 1000000) as bucket_ms
    FROM clock_sync_metrics
)
GROUP BY bucket_ms
ORDER BY bucket_ms;

-- Clock master health
SELECT 
    master_id,
    uptime_seconds,
    heartbeats_sent,
    source_type,
    avg_uncertainty_ns,
    failover_count
FROM clock_master_metrics;
```

### Alert Thresholds

```yaml
alerts:
  - name: high_clock_uncertainty
    condition: uncertainty_ns > 50000000  # 50ms
    severity: warning
    
  - name: critical_clock_uncertainty
    condition: uncertainty_ns > 100000000  # 100ms
    severity: critical
    
  - name: clock_sync_loss
    condition: time_since_sync_ms > 5000
    severity: critical
    
  - name: high_missed_heartbeats
    condition: missed_heartbeats > 10
    severity: warning
    
  - name: clock_master_failover
    condition: cluster_epoch changed
    severity: info
```

---

## Configuration

### Clock Service Configuration

```yaml
clock_service:
  # Time source (auto-detected if not specified)
  source: auto  # auto, aws, gcp, azure, gps, ptp, chrony
  
  # Heartbeat settings
  heartbeat:
    frequency_hz: 20  # 50ms interval
    port: 5555
    multicast_group: 239.255.0.1
    
  # Failover settings
  failover:
    timeout_ms: 500
    threshold_missed_beats: 10
    
  # Uncertainty limits
  uncertainty:
    max_acceptable_ns: 50000000  # 50ms
    critical_threshold_ns: 100000000  # 100ms
    measurement_samples: 100
    
  # High availability
  ha:
    enabled: true
    primary_ip: 10.0.1.10
    backup_ip: 10.0.1.11
```

### Node Configuration

```yaml
clock_client:
  # Multicast settings
  multicast_group: 239.255.0.1
  heartbeat_port: 5555
  
  # Sync settings
  sync:
    timeout_ms: 5000  # Declare desync after 5s
    strict_mode: false  # Pause writes on desync?
    
  # Monitoring
  monitoring:
    log_uncertainty: true
    alert_on_degraded: true
```

---

## Implementation Checklist

### Phase 1: Software Clock Sync (Cloud)
- [ ] Implement heartbeat protocol (UDP multicast)
- [ ] Build clock master service
- [ ] Implement clock client (receiver + state management)
- [ ] Add AWS Time Sync integration
- [ ] Add GCP NTP integration
- [ ] Add Azure NTP integration
- [ ] Implement uncertainty measurement
- [ ] Add monitoring metrics
- [ ] Test failover behavior

### Phase 2: High Availability
- [ ] Implement backup clock master
- [ ] Add failover detection and promotion
- [ ] Test master failover scenarios
- [ ] Add cluster epoch tracking
- [ ] Document HA procedures

### Phase 3: Hardware Clock Support (On-Prem)
- [ ] Add GPS receiver integration
- [ ] Add PTP integration
- [ ] Implement hardware timestamp reading
- [ ] Test hardware clock accuracy
- [ ] Document hardware setup

### Phase 4: Operations
- [ ] Create monitoring dashboards
- [ ] Define alerting thresholds
- [ ] Write runbook for clock issues
- [ ] Performance testing
- [ ] Production deployment

---

**Document Status**: Draft for Review  
**Next Review Date**: TBD  
**Approval Required**: Infrastructure Team
