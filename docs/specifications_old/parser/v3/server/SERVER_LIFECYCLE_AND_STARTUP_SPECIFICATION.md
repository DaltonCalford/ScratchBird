# Server Lifecycle and Startup Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

## 1. Purpose

Define the complete server lifecycle from initial startup through normal operation to graceful shutdown. This specification covers:

- Server startup sequence and phase ordering
- Pre-listener database initialization and recovery
- Background thread initialization (garbage collection, job scheduler, clustering)
- Listener and parser pool startup
- Runtime operation modes
- Graceful shutdown and restart procedures

## 2. Architecture Principles

### 2.1 Startup Philosophy

Unlike PostgreSQL (per-database startup with postmaster forking) or MySQL (monolithic server with thread pools), ScratchBird uses a **coordinated multi-phase startup**:

1. **Core Engine Initialization** - Single main process initializes engine
2. **Database Pre-Connection** - Configured databases opened for recovery/tasks
3. **Background Services** - System threads started
4. **Listener Spawn** - Network listeners created with parser pools
5. **Parser Standby** - Parsers wait idle until connection handoff
6. **Accept Loop** - Server ready for client connections

### 2.2 Key Differences from Other Databases

| Aspect | PostgreSQL | MySQL | Firebird | ScratchBird |
|--------|------------|-------|----------|-------------|
| **Startup Model** | Postmaster forks backends | Threads in single process | Classic/SuperServer modes | Phased coordinated startup |
| **Database Open** | On first connection | All at startup | On attach | Config-driven pre-connect |
| **Parser/Backend** | Fork per connection | Thread per connection | Process/thread per connection | Pre-spawned parser pool |
| **Recovery** | Automatic on first connect | Automatic at startup | Automatic at startup | Configurable pre-listener |
| **IPC** | Shared memory | Shared memory | Shared memory/XNet | Unix sockets/named pipes |

### 2.3 Process Model

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         ScratchBird Process Architecture                         │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│                             sb_server (Main Process)                             │
│                                                                                  │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                         Startup Phase (Single-Threaded)                    │  │
│  │  1. Parse configuration                                                    │  │
│  │  2. Initialize logging                                                      │  │
│  │  3. Open database registry                                                  │  │
│  │  4. Execute startup databases/tasks                                        │  │
│  │  5. Transition to multi-threaded                                           │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                  │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                      Background Service Threads                             │  │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────────┐  │  │
│  │  │ Job Scheduler│ │ MGA GC       │ │ Cluster      │ │ Stats Collector  │  │  │
│  │  │ Thread       │ │ Thread       │ │ Membership   │ │ Thread           │  │  │
│  │  │              │ │ (per DB)     │ │ Thread       │ │                  │  │  │
│  │  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                  │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                        Listener Processes                                   │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐         │  │
│  │  │ Native      │ │ PostgreSQL  │ │ MySQL       │ │ Firebird    │         │  │
│  │  │ Listener    │ │ Listener    │ │ Listener    │ │ Listener    │         │  │
│  │  │ (process)   │ │ (process)   │ │ (process)   │ │ (process)   │         │  │
│  │  │ :3092       │ │ :5432       │ │ :3306       │ │ :3050       │         │  │
│  │  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘         │  │
│  │         │               │               │               │                 │  │
│  │         └───────────────┴───────────────┴───────────────┘                 │  │
│  │                         │                                                  │  │
│  │              Control Plane (Unix sockets/pipes)                            │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
                                     │
                                     │ fork()/CreateProcess()
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         Parser Processes (Per-Listener)                          │
│                                                                                  │
│  ┌──────────────────────────┐  ┌──────────────────────────┐                     │
│  │   Native Parser Pool     │  │   PostgreSQL Parser Pool │                     │
│  │  ┌────┐ ┌────┐ ┌────┐   │  │  ┌────┐ ┌────┐ ┌────┐    │                     │
│  │  │ P1 │ │ P2 │ │ P3 │   │  │  │ P1 │ │ P2 │ │ P3 │    │                     │
│  │  └────┘ └────┘ └────┘   │  │  └────┘ └────┘ └────┘    │                     │
│  │  Idle until handoff      │  │  Idle until handoff       │                     │
│  └──────────────────────────┘  └──────────────────────────┘                     │
│                                                                                  │
│  Each parser:                                                                    │
│  - Opens control socket to listener                                              │
│  - Waits for HELLO_ACK                                                           │
│  - Blocks on recvmsg() for socket handoff                                        │
│  - After handoff: connects to engine via IPC                                     │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## 3. Startup Sequence

### 3.1 Phase 1: Pre-Initialization (Single-Threaded)

```cpp
int Server::startup(int argc, char** argv) {
    // 1.1: Parse command-line arguments
    StartupConfig config = parseCommandLine(argc, argv);
    
    // 1.2: Early logging setup (console only)
    initializeEmergencyLogging();
    
    // 1.3: Check for existing instance
    if (!acquirePidFile(config.pid_file)) {
        LOG_FATAL("Another instance is already running (PID: {})", 
                  readExistingPid(config.pid_file));
        return EXIT_FAILURE;
    }
    
    // 1.4: Validate configuration
    ValidationResult validation = validateConfiguration(config);
    if (!validation.ok()) {
        LOG_FATAL("Configuration validation failed:");
        for (const auto& error : validation.errors()) {
            LOG_FATAL("  - {}", error);
        }
        return EXIT_FAILURE;
    }
    
    // 1.5: Set up signal handlers (early)
    initializeSignalHandlers();
    
    LOG_INFO("ScratchBird server starting (version {})", SCRATCHBIRD_VERSION);
    
    return runStartupPhases(config);
}
```

### 3.2 Phase 2: Core Initialization

```cpp
void Server::runStartupPhases(const StartupConfig& config) {
    // 2.1: Initialize full logging system
    initializeLogging(config.logging);
    LOG_INFO("Logging initialized");
    
    // 2.2: Initialize network subsystem
    if (!network::initNetwork()) {
        LOG_FATAL("Failed to initialize network subsystem");
        exit(1);
    }
    LOG_INFO("Network subsystem initialized");
    
    // 2.3: Initialize security subsystem (OpenSSL)
    security::initializeOpenSSL();
    LOG_INFO("Security subsystem initialized");
    
    // 2.4: Open database registry
    registry_ = DatabaseRegistry::open(config.registry_path);
    if (!registry_) {
        LOG_FATAL("Failed to open database registry: {}", config.registry_path);
        exit(1);
    }
    LOG_INFO("Database registry opened: {}", config.registry_path);
    
    // 2.5: Open security database (if using shared security)
    if (config.security_model == "shared") {
        security_db_ = SecurityDatabase::open(config.security_database_path);
        if (!security_db_) {
            LOG_FATAL("Failed to open security database: {}", 
                      config.security_database_path);
            exit(1);
        }
        LOG_INFO("Security database opened");
    }
}
```

### 3.3 Phase 3: Startup Database Processing

This phase handles automatic database operations before listeners start.

```cpp
void Server::executeStartupDatabases(const StartupConfig& config) {
    LOG_INFO("Processing startup databases...");
    
    // 3.1: Open configured startup databases
    for (const auto& db_config : config.startup_databases) {
        auto db_info = registry_->getDatabaseByName(db_config.database_name);
        if (!db_info) {
            LOG_ERROR("Startup database '{}' not found in registry", 
                      db_config.database_name);
            if (db_config.required) {
                LOG_FATAL("Required startup database failed to open");
                exit(1);
            }
            continue;
        }
        
        // Open database
        auto database = openDatabase(db_info->database_id, 
                                     OpenMode::RECOVERY);
        if (!database) {
            LOG_ERROR("Failed to open database '{}'", db_config.database_name);
            if (db_config.required) {
                exit(1);
            }
            continue;
        }
        
        startup_databases_[db_info->database_id] = database;
        LOG_INFO("Opened startup database: {}", db_config.database_name);
        
        // 3.2: Perform recovery if needed
        if (database->needsRecovery()) {
            LOG_INFO("Performing recovery on database: {}", db_config.database_name);
            RecoveryResult result = database->performRecovery();
            if (!result.ok()) {
                LOG_ERROR("Recovery failed for database '{}': {}", 
                          db_config.database_name, result.error());
                if (db_config.required) {
                    exit(1);
                }
            } else {
                LOG_INFO("Recovery completed for database '{}' ({} transactions)",
                         db_config.database_name, result.transaction_count());
            }
        }
        
        // 3.3: Execute startup SQL scripts
        for (const auto& script : db_config.startup_scripts) {
            LOG_INFO("Executing startup script: {}", script);
            ExecutionResult result = database->executeScript(script);
            if (!result.ok()) {
                LOG_ERROR("Startup script failed '{}': {}", script, result.error());
                if (db_config.required) {
                    exit(1);
                }
            }
        }
        
        // 3.4: Fire startup triggers
        database->fireStartupTriggers();
    }
    
    LOG_INFO("Startup database processing complete");
}
```

**Configuration Example:**

```ini
[server]
; Startup databases - opened before listeners
; Format: startup_database.<name> = <options>

startup_database.sales = {
    "required": true,
    "recovery": true,
    "scripts": [
        "/etc/scratchbird/startup/cleanup_temp_tables.sql",
        "/etc/scratchbird/startup/refresh_materialized_views.sql"
    ]
}

startup_database.reporting = {
    "required": false,
    "recovery": true,
    "scripts": []
}
```

### 3.4 Phase 4: Background Thread Initialization

```cpp
void Server::startBackgroundServices(const StartupConfig& config) {
    LOG_INFO("Starting background services...");
    
    // 4.1: Lock Manager (Firebird-style, per-database)
    // Lock manager coordinates write-write conflicts in MGA
    // Readers don't need locks (MGA provides snapshot isolation)
    for (const auto& [db_id, database] : startup_databases_) {
        auto lock_mgr = std::make_unique<LockManager>(database.get());
        lock_mgr->setDeadlockCheckInterval(config.lock_deadlock_check_interval_ms);
        lock_mgr->setLockTimeout(config.lock_timeout_ms);
        lock_mgr->start();
        lock_managers_[db_id] = std::move(lock_mgr);
        LOG_INFO("Lock manager started for database: {}", db_id);
    }
    
    // 4.2: Job Scheduler Thread
    if (config.job_scheduler_enabled) {
        job_scheduler_ = std::make_unique<JobScheduler>();
        job_scheduler_->start();
        LOG_INFO("Job scheduler started");
    }
    
    // 4.3: MGA Garbage Collector (per database)
    // Cleans up old record versions that are no longer visible to any transaction
    for (const auto& [db_id, database] : startup_databases_) {
        auto gc = std::make_unique<MgaGarbageCollector>(database.get());
        gc->setInterval(config.gc_interval_seconds);
        gc->start();
        gc_threads_[db_id] = std::move(gc);
        LOG_INFO("MGA GC started for database: {}", db_id);
    }
    
    // 4.4: Statistics Collector
    stats_collector_ = std::make_unique<StatisticsCollector>();
    stats_collector_->start();
    LOG_INFO("Statistics collector started");
    
    // 4.5: Cluster Membership (if clustered)
    if (config.cluster_enabled) {
        cluster_manager_ = std::make_unique<ClusterManager>();
        if (!cluster_manager_->joinCluster(config.cluster_config)) {
            LOG_FATAL("Failed to join cluster");
            exit(1);
        }
        LOG_INFO("Cluster membership active");
    }
    
    // 4.6: Health Monitor
    health_monitor_ = std::make_unique<HealthMonitor>();
    health_monitor_->start();
    LOG_INFO("Health monitor started");
    
    LOG_INFO("All background services started");
}
```

### 3.5 Phase 5: Listener and Parser Pool Startup

```cpp
void Server::startListeners(const StartupConfig& config) {
    LOG_INFO("Starting network listeners...");
    
    // 5.1: Create IPC directory for control sockets
    fs::create_directories(config.control_socket_dir);
    
    // 5.2: Start native listener (if enabled)
    if (config.native_listener.enabled) {
        startListenerProcess("native", config.native_listener);
    }
    
    // 5.3: Start PostgreSQL listener (if enabled)
    if (config.postgresql_listener.enabled) {
        startListenerProcess("postgresql", config.postgresql_listener);
    }
    
    // 5.4: Start MySQL listener (if enabled)
    if (config.mysql_listener.enabled) {
        startListenerProcess("mysql", config.mysql_listener);
    }
    
    // 5.5: Start Firebird listener (if enabled)
    if (config.firebird_listener.enabled) {
        startListenerProcess("firebird", config.firebird_listener);
    }
    
    LOG_INFO("All listeners started");
}

void Server::startListenerProcess(const std::string& protocol,
                                   const ListenerConfig& config) {
    LOG_INFO("Starting {} listener on port {}", protocol, config.port);
    
    // Build listener arguments
    std::vector<std::string> args = {
        fmt::format("sb_listener_{}", protocol),
        "--bind", config.bind_address,
        "--port", std::to_string(config.port),
        "--control-socket-dir", config.control_socket_dir,
        "--engine-endpoint", config.engine_ipc_path,
        "--pool-min", std::to_string(config.parser_pool.min_size),
        "--pool-max", std::to_string(config.parser_pool.max_size),
        "--tls-config", config.tls_config_path
    };
    
    // Spawn listener process
    ProcessHandle listener = spawnProcess(args);
    if (!listener.valid()) {
        LOG_FATAL("Failed to start {} listener", protocol);
        exit(1);
    }
    
    listeners_[protocol] = std::move(listener);
    
    // Wait for listener to signal ready
    if (!waitForListenerReady(protocol, config.ready_timeout_seconds)) {
        LOG_FATAL("{} listener failed to start within timeout", protocol);
        exit(1);
    }
    
    LOG_INFO("{} listener ready (PID: {})", protocol, listener.pid());
}
```

### 3.6 Phase 6: Parser Pool Handshake

```cpp
void Server::acceptParserHandshakes(const StartupConfig& config) {
    LOG_INFO("Accepting parser handshakes...");
    
    // Create control plane server
    control_plane_ = std::make_unique<ControlPlaneServer>();
    
    for (const auto& [protocol, listener_config] : config.listeners) {
        if (!listener_config.enabled) continue;
        
        std::string control_socket = fmt::format("{}/sb_listener.{}.sock",
                                                  config.control_socket_dir,
                                                  protocol);
        
        // Accept HELLO from each parser
        for (uint32_t i = 0; i < listener_config.parser_pool.min_size; i++) {
            auto socket = control_plane_->accept(control_socket);
            if (!socket) {
                LOG_ERROR("Failed to accept parser handshake for {}", protocol);
                continue;
            }
            
            // Receive HELLO message
            ControlPlaneMessage msg;
            auto status = receiveControlPlaneMessage(*socket, msg);
            if (status != Status::OK || msg.type != MessageType::HELLO) {
                LOG_ERROR("Invalid parser HELLO from {}", protocol);
                continue;
            }
            
            // Parse HELLO payload
            HelloPayload payload;
            if (!parseHelloPayload(msg.payload, payload)) {
                LOG_ERROR("Malformed parser HELLO");
                sendHelloAck(*socket, false, "Malformed HELLO");
                continue;
            }
            
            // Validate protocol match
            if (payload.protocol != protocol) {
                LOG_ERROR("Protocol mismatch: expected {}, got {}",
                          protocol, payload.protocol);
                sendHelloAck(*socket, false, "Protocol mismatch");
                continue;
            }
            
            // Send HELLO_ACK (parser now idle, waiting for handoff)
            sendHelloAck(*socket, true, "");
            
            // Register parser worker
            registerParserWorker(protocol, payload.worker_id, 
                                payload.pid, std::move(socket));
            
            LOG_DEBUG("Parser registered for {}: worker_id={}, pid={}",
                     protocol, payload.worker_id, payload.pid);
        }
        
        LOG_INFO("Parser pool ready for {}: {} workers", 
                 protocol, listener_config.parser_pool.min_size);
    }
    
    LOG_INFO("All parser pools ready");
}
```

### 3.7 Phase 7: Ready State

```cpp
void Server::enterReadyState() {
    LOG_INFO("============================================");
    LOG_INFO("ScratchBird Server Ready");
    LOG_INFO("============================================");
    LOG_INFO("Instance: {}", config_.instance_name);
    LOG_INFO("Registry: {}", config_.registry_path);
    LOG_INFO("Startup databases: {}", startup_databases_.size());
    
    for (const auto& [protocol, listener] : listeners_) {
        LOG_INFO("Listener {}: port {}, PID {}", 
                 protocol, listener.port(), listener.pid());
    }
    
    LOG_INFO("============================================");
    
    // Notify systemd (if applicable)
    notifySystemdReady();
    
    // Start main event loop
    runEventLoop();
}
```

## 4. Complete Startup Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           SERVER STARTUP FLOW                                    │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────┐
    │   START     │
    └──────┬──────┘
           │
           ▼
┌──────────────────────┐
│  PHASE 1: PRE-INIT   │  ◄── Single-threaded, console logging only
│  - Parse CLI args    │
│  - Early logging     │
│  - PID file lock     │
│  - Validate config   │
│  - Signal handlers   │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ PHASE 2: CORE INIT   │  ◄── Initialize subsystems
│  - Full logging      │
│  - Network init      │
│  - Security (TLS)    │
│  - Open registry     │
│  - Open security DB  │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ PHASE 3: STARTUP DBs │  ◄── Pre-listener database operations
│  - Open databases    │
│  - Perform recovery  │      (if needed)
│  - Execute scripts   │
│  - Fire triggers     │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ PHASE 4: BACKGROUND  │  ◄── Start system threads
│  - Job scheduler     │
│  - MGA GC threads    │
│  - Stats collector   │
│  - Cluster member    │
│  - Health monitor    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ PHASE 5: LISTENERS   │  ◄── Spawn listener processes
│  - Create IPC dir    │
│  - Spawn listeners   │      (native, pg, mysql, fb)
│  - Wait for ready    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ PHASE 6: PARSERS     │  ◄── Accept parser handshakes
│  - Accept HELLOs     │
│  - Validate parsers  │
│  - Register workers  │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ PHASE 7: READY       │  ◄── Server ready for connections
│  - Log ready state   │
│  - Notify systemd    │
│  - Event loop        │
└──────────┬───────────┘
           │
           ▼
    ┌─────────────┐
    │ RUNNING     │  ◄── Normal operation
    │ Event Loop  │
    └─────────────┘
```

## 5. Parser Lifecycle

### 5.1 Parser Startup (Child Process)

```cpp
// This runs in sb_parser_* process (child of sb_server via listener fork)
int ParserMain::run(int argc, char** argv) {
    // Parse arguments from listener
    ParserConfig config = parseArguments(argc, argv);
    
    // Initialize logging (to stderr, redirected by listener)
    initializeLogging();
    
    // Initialize network (for client communication)
    network::initNetwork();
    
    // Initialize TLS (if enabled)
    std::unique_ptr<security::TLSContext> tls_context;
    if (!config.tls_config.empty()) {
        tls_context = loadTLSContext(config.tls_config);
    }
    
    // Connect to listener control plane
    ControlPlaneClient control_plane(config.control_socket);
    
    // Send HELLO to listener
    HelloPayload hello;
    hello.protocol = config.protocol;
    hello.worker_id = generateWorkerId();
    hello.pid = getCurrentProcessId();
    
    if (!control_plane.sendHello(hello)) {
        LOG_ERROR("Failed to send HELLO to listener");
        return 1;
    }
    
    // Wait for HELLO_ACK
    HelloAck ack;
    if (!control_plane.receiveHelloAck(ack) || !ack.accepted) {
        LOG_ERROR("HELLO rejected: {}", ack.reason);
        return 1;
    }
    
    LOG_INFO("Parser registered with listener (worker_id: {})", hello.worker_id);
    
    // Enter idle loop, waiting for socket handoff
    return idleLoop(control_plane, tls_context.get(), config);
}
```

### 5.2 Idle Loop (Waiting for Handoff)

```cpp
int ParserMain::idleLoop(ControlPlaneClient& control_plane,
                          security::TLSContext* tls_ctx,
                          const ParserConfig& config) {
    while (!shutdown_requested_) {
        // Block waiting for control message from listener
        ControlPlaneMessage msg;
        auto status = control_plane.receiveMessage(msg);
        
        if (status != Status::OK) {
            LOG_ERROR("Control plane receive failed");
            return 1;
        }
        
        switch (msg.type) {
            case MessageType::HANDOFF_SOCKET:
                // Received socket fd from listener
                return handleSocketHandoff(control_plane, msg, tls_ctx, config);
                
            case MessageType::HEALTH_CHECK:
                // Respond to health check
                sendHealthReport(control_plane, HealthState::IDLE);
                break;
                
            case MessageType::RECYCLE:
                // Listener asking us to exit cleanly
                LOG_INFO("Received RECYCLE request");
                return 0;
                
            case MessageType::SHUTDOWN:
                // Server shutting down
                LOG_INFO("Received SHUTDOWN request");
                return 0;
                
            default:
                LOG_WARNING("Unexpected message type: {}", 
                           static_cast<int>(msg.type));
        }
    }
    
    return 0;
}
```

### 5.3 Socket Handoff Handling

```cpp
int ParserMain::handleSocketHandoff(ControlPlaneClient& control_plane,
                                     const ControlPlaneMessage& handoff_msg,
                                     security::TLSContext* tls_ctx,
                                     const ParserConfig& config) {
    // Parse handoff payload
    HandoffPayload payload;
    if (!parseHandoffPayload(handoff_msg.payload, payload)) {
        LOG_ERROR("Malformed handoff payload");
        sendHandoffAck(control_plane, false);
        return idleLoop(control_plane, tls_ctx, config);  // Return to idle
    }
    
    // Receive socket fd via SCM_RIGHTS
    int client_fd = receiveSocketFd(control_plane.socket());
    if (client_fd < 0) {
        LOG_ERROR("Failed to receive socket fd");
        sendHandoffAck(control_plane, false);
        return idleLoop(control_plane, tls_ctx, config);
    }
    
    // Acknowledge handoff to listener
    sendHandoffAck(control_plane, true);
    
    // Create Socket wrapper
    auto client_socket = Socket::fromFd(client_fd, AddressFamily::IPV4, 
                                        SocketType::STREAM);
    
    // Perform TLS handshake (if enabled and not already done)
    if (payload.tls_active && tls_ctx) {
        ErrorContext tls_err;
        auto status = client_socket->startTLS(*tls_ctx, &tls_err);
        if (status != Status::OK) {
            LOG_ERROR("TLS handshake failed: {}", tls_err.message);
            return idleLoop(control_plane, tls_ctx, config);
        }
        LOG_INFO("TLS handshake successful");
    }
    
    // NOW connect to engine (only when we have a client)
    auto engine_conn = connectToEngine(config.engine_endpoint);
    if (!engine_conn) {
        LOG_ERROR("Failed to connect to engine");
        return idleLoop(control_plane, tls_ctx, config);
    }
    
    // Create protocol adapter
    auto adapter = createProtocolAdapter(config.protocol);
    
    // Handle client session
    Session session(std::move(client_socket), std::move(engine_conn), 
                    adapter.get(), payload.connection_id);
    session.run();
    
    // Session ended, return to idle or exit based on config
    if (config.max_requests > 0 && ++request_count_ >= config.max_requests) {
        LOG_INFO("Max requests reached, exiting");
        return 0;
    }
    
    // Return to idle loop for next handoff
    return idleLoop(control_plane, tls_ctx, config);
}
```

## 6. IPC Fallback Mechanism

As mentioned in the requirements, if IPC fails, there's a fallback to localhost TCP:

```cpp
class EngineConnection {
public:
    static std::unique_ptr<EngineConnection> connect(
        const std::string& endpoint,
        ErrorContext* ctx = nullptr) {
        
        // Try Unix socket first (preferred)
        if (endpoint.starts_with("/")) {
            auto conn = tryUnixSocket(endpoint, ctx);
            if (conn) return conn;
            
            // Fallback to TCP on localhost
            LOG_WARNING("Unix socket failed, trying localhost TCP fallback");
            return tryLocalhostTcp(DEFAULT_ENGINE_PORT, ctx);
        }
        
        // TCP connection
        return tryTcp(endpoint, ctx);
    }
    
private:
    static std::unique_ptr<EngineConnection> tryUnixSocket(
        const std::string& path, ErrorContext* ctx) {
        
        auto socket = Socket::create(AddressFamily::LOCAL, SocketType::STREAM, ctx);
        if (!socket) return nullptr;
        
        NetworkAddress addr;
        addr.family = AddressFamily::LOCAL;
        addr.path = path;
        
        if (socket->connect(addr, ctx) != Status::OK) {
            return nullptr;
        }
        
        return std::make_unique<EngineConnection>(std::move(socket));
    }
    
    static std::unique_ptr<EngineConnection> tryLocalhostTcp(
        uint16_t port, ErrorContext* ctx) {
        
        auto socket = Socket::create(AddressFamily::IPV4, SocketType::STREAM, ctx);
        if (!socket) return nullptr;
        
        NetworkAddress addr;
        addr.family = AddressFamily::IPV4;
        addr.host = "127.0.0.1";
        addr.port = port;
        
        if (socket->connect(addr, ctx) != Status::OK) {
            return nullptr;
        }
        
        // Authenticate as internal connection
        if (!authenticateInternal(socket.get())) {
            SET_ERROR(ctx, AUTH_ERROR, "Internal authentication failed");
            return nullptr;
        }
        
        return std::make_unique<EngineConnection>(std::move(socket));
    }
};
```

## 7. Shutdown Sequence

### 7.1 Graceful Shutdown

```cpp
void Server::shutdown(ShutdownMode mode) {
    LOG_INFO("Initiating server shutdown (mode: {})", 
             mode == ShutdownMode::GRACEFUL ? "graceful" : "immediate");
    
    shutdown_requested_ = true;
    
    // Phase 1: Stop accepting new connections
    LOG_INFO("Stopping listeners...");
    for (auto& [protocol, listener] : listeners_) {
        listener.stopAccepting();
        listener.sendShutdownSignal();
    }
    
    if (mode == ShutdownMode::GRACEFUL) {
        // Wait for active connections to complete
        LOG_INFO("Waiting for active connections to complete...");
        waitForConnectionsToDrain(graceful_timeout_seconds_);
    }
    
    // Phase 2: Stop listeners
    for (auto& [protocol, listener] : listeners_) {
        listener.terminate();
    }
    listeners_.clear();
    
    // Phase 3: Stop background services
    LOG_INFO("Stopping background services...");
    if (health_monitor_) health_monitor_->stop();
    if (stats_collector_) stats_collector_->stop();
    if (cluster_manager_) cluster_manager_->leaveCluster();
    
    for (auto& [db_id, gc] : gc_threads_) {
        gc->stop();
    }
    gc_threads_.clear();
    
    if (job_scheduler_) job_scheduler_->stop();
    
    // Phase 4: Close databases
    LOG_INFO("Closing databases...");
    for (const auto& [db_id, database] : startup_databases_) {
        database->fireShutdownTriggers();
        database->close();
    }
    startup_databases_.clear();
    
    // Phase 5: Cleanup
    LOG_INFO("Cleaning up...");
    registry_->close();
    if (security_db_) security_db_->close();
    
    // Remove PID file
    removePidFile();
    
    LOG_INFO("Server shutdown complete");
}
```

### 7.2 Signal Handling

```cpp
void Server::initializeSignalHandlers() {
    // SIGTERM - Graceful shutdown
    std::signal(SIGTERM, [](int) {
        Server::instance().shutdown(ShutdownMode::GRACEFUL);
    });
    
    // SIGINT - Graceful shutdown (Ctrl+C)
    std::signal(SIGINT, [](int) {
        Server::instance().shutdown(ShutdownMode::GRACEFUL);
    });
    
    // SIGHUP - Configuration reload
    std::signal(SIGHUP, [](int) {
        Server::instance().reloadConfiguration();
    });
    
    // SIGUSR1 - Dump statistics
    std::signal(SIGUSR1, [](int) {
        Server::instance().dumpStatistics();
    });
    
    // SIGUSR2 - Rotate logs
    std::signal(SIGUSR2, [](int) {
        Server::instance().rotateLogs();
    });
    
    // SIGCHLD - Reap child processes (listeners/parsers)
    std::signal(SIGCHLD, [](int) {
        Server::instance().reapChildProcesses();
    });
}
```

## 8. Configuration Reload

```cpp
void Server::reloadConfiguration() {
    LOG_INFO("Reloading configuration...");
    
    // Load new config
    auto new_config = loadConfiguration(config_file_path_);
    
    // Apply non-disruptive changes
    // - Log level changes
    updateLogLevel(new_config.logging.level);
    
    // - Connection limit changes
    updateConnectionLimits(new_config.max_connections);
    
    // - Add/remove listeners
    for (const auto& [protocol, listener_config] : new_config.listeners) {
        if (listener_config.enabled && !listeners_.count(protocol)) {
            // Start new listener
            LOG_INFO("Starting new {} listener", protocol);
            startListenerProcess(protocol, listener_config);
        } else if (!listener_config.enabled && listeners_.count(protocol)) {
            // Stop existing listener
            LOG_INFO("Stopping {} listener", protocol);
            listeners_[protocol].stopAccepting();
            listeners_[protocol].terminate();
            listeners_.erase(protocol);
        }
    }
    
    // Changes requiring restart (log warning)
    if (new_config.registry_path != config_.registry_path) {
        LOG_WARNING("Registry path changed - restart required");
    }
    
    LOG_INFO("Configuration reload complete");
}
```

## 9. Related Specifications

- [Server Architecture](SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md) - Overall architecture
- [Database Registry](DATABASE_REGISTRY_SPECIFICATION.md) - Database registry operations
- [Installation and Initialization](INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md) - Installation flow
- [Network Listener and Parser Pool](../network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md) - Listener details
- [Control Plane Protocol](../network/CONTROL_PLANE_PROTOCOL_SPEC.md) - Control plane messages
- [Engine Parser IPC Contract](../network/ENGINE_PARSER_IPC_CONTRACT.md) - Parser-engine communication

## 10. Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| Pre-initialization phase | 🟢 Implemented | CLI parsing, PID file, validation |
| Core initialization | 🟢 Implemented | Logging, network, security |
| Startup database processing | 🔴 Not implemented | Config-driven pre-connect |
| Recovery on startup | 🟡 Partial | Basic recovery exists, needs config integration |
| Background thread startup | 🟡 Partial | GC and scheduler lifecycle defined; implementation pending (see `scheduler/README.md`) |
| Listener spawning | 🟢 Implemented | fork()/exec() model working |
| Parser handshake | 🟢 Implemented | HELLO/HELLO_ACK working |
| IPC fallback to TCP | 🔴 Not implemented | Design complete, needs implementation |
| Graceful shutdown | 🟡 Partial | Basic signal handling, needs full sequence |
| Config reload (SIGHUP) | 🔴 Not implemented | Stub exists |
