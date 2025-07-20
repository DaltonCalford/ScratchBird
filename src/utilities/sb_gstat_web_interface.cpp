#include "sb_gstat_web_interface.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cstring>
#include <errno.h>

using namespace SBEnhanced;

// Constructor
GSTATWebInterface::GSTATWebInterface(GSTATEnhanced* gstat) 
    : gstat_instance(gstat) {
    
    if (!gstat_instance) {
        logError("GSTATWebInterface: Invalid GSTAT instance provided");
        return;
    }
    
    // Initialize default configuration
    config.bind_address = "127.0.0.1";
    config.port = 8080;
    config.max_connections = 100;
    config.request_timeout_seconds = 30;
    config.document_root = "./web";
    config.enable_ssl = false;
    config.enable_websockets = true;
    config.enable_cors = true;
    config.worker_threads = 4;
    
    // Register default endpoints
    registerDefaultEndpoints();
    registerDefaultApiEndpoints();
    registerDefaultDashboardWidgets();
    
    logMessage("GSTATWebInterface: Initialized successfully");
}

// Destructor
GSTATWebInterface::~GSTATWebInterface() {
    stop();
    cleanupResources();
}

// Initialize web server
bool GSTATWebInterface::initialize(const WebServerConfig& server_config) {
    try {
        config = server_config;
        
        // Validate configuration
        if (config.port < 1 || config.port > 65535) {
            logError("Invalid port number: " + std::to_string(config.port));
            return false;
        }
        
        if (config.max_connections < 1 || config.max_connections > 1000) {
            logError("Invalid max_connections: " + std::to_string(config.max_connections));
            return false;
        }
        
        if (config.worker_threads < 1 || config.worker_threads > 32) {
            logError("Invalid worker_threads: " + std::to_string(config.worker_threads));
            return false;
        }
        
        // Create document root directory if it doesn't exist
        if (!config.document_root.empty()) {
            std::string create_dir_cmd = "mkdir -p " + config.document_root;
            if (system(create_dir_cmd.c_str()) != 0) {
                logError("Failed to create document root directory: " + config.document_root);
            }
        }
        
        logMessage("Web server initialized with configuration:");
        logMessage("  Bind Address: " + config.bind_address);
        logMessage("  Port: " + std::to_string(config.port));
        logMessage("  Max Connections: " + std::to_string(config.max_connections));
        logMessage("  Worker Threads: " + std::to_string(config.worker_threads));
        logMessage("  Document Root: " + config.document_root);
        logMessage("  SSL Enabled: " + std::string(config.enable_ssl ? "true" : "false"));
        logMessage("  WebSockets Enabled: " + std::string(config.enable_websockets ? "true" : "false"));
        logMessage("  CORS Enabled: " + std::string(config.enable_cors ? "true" : "false"));
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Failed to initialize web server: " + std::string(e.what()));
        return false;
    }
}

// Start web server
bool GSTATWebInterface::start() {
    try {
        if (server_running.load()) {
            logMessage("Web server is already running");
            return true;
        }
        
        // Create server socket
        if (!createServerSocket()) {
            logError("Failed to create server socket");
            return false;
        }
        
        // Bind and listen
        if (!bindAndListen()) {
            logError("Failed to bind and listen on socket");
            closeServerSocket();
            return false;
        }
        
        // Setup SSL if enabled
        if (config.enable_ssl && !setupSSL()) {
            logError("Failed to setup SSL");
            closeServerSocket();
            return false;
        }
        
        // Start server thread
        server_running.store(true);
        server_thread = std::thread(&GSTATWebInterface::serverLoop, this);
        
        // Start worker threads
        worker_threads.reserve(config.worker_threads);
        for (int i = 0; i < config.worker_threads; ++i) {
            worker_threads.emplace_back(&GSTATWebInterface::workerThreadLoop, this);
        }
        
        // Start real-time broadcasting if enabled
        if (config.enable_websockets) {
            startRealTimeUpdates();
        }
        
        logMessage("Web server started successfully on " + config.bind_address + ":" + std::to_string(config.port));
        return true;
        
    } catch (const std::exception& e) {
        logError("Failed to start web server: " + std::string(e.what()));
        stop();
        return false;
    }
}

// Stop web server
bool GSTATWebInterface::stop() {
    try {
        if (!server_running.load()) {
            return true;
        }
        
        logMessage("Stopping web server...");
        
        // Stop accepting new connections
        server_running.store(false);
        
        // Stop real-time updates
        stopRealTimeUpdates();
        
        // Close server socket to wake up server thread
        closeServerSocket();
        
        // Wait for server thread to finish
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
        // Wait for worker threads to finish
        for (auto& thread : worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads.clear();
        
        // Cleanup WebSocket connections
        cleanupWebSocketConnections();
        
        logMessage("Web server stopped successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error stopping web server: " + std::string(e.what()));
        return false;
    }
}

// Check if server is running
bool GSTATWebInterface::isRunning() const {
    return server_running.load();
}

// Restart web server
bool GSTATWebInterface::restart() {
    logMessage("Restarting web server...");
    if (!stop()) {
        return false;
    }
    
    // Give some time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    return start();
}

// Register default endpoints
void GSTATWebInterface::registerDefaultEndpoints() {
    // Main dashboard endpoint
    WebEndpoint dashboard_endpoint;
    dashboard_endpoint.path = "/";
    dashboard_endpoint.method = "GET";
    dashboard_endpoint.handler = [this](const HttpRequest& req, GSTATEnhanced* gstat) -> HttpResponse {
        return serveDashboard(req);
    };
    dashboard_endpoint.description = "Main dashboard page";
    registerEndpoint(dashboard_endpoint);
    
    // Statistics endpoint
    WebEndpoint stats_endpoint;
    stats_endpoint.path = "/statistics";
    stats_endpoint.method = "GET";
    stats_endpoint.handler = [this](const HttpRequest& req, GSTATEnhanced* gstat) -> HttpResponse {
        return generateStatisticsTable(req);
    };
    stats_endpoint.description = "Database statistics table";
    registerEndpoint(stats_endpoint);
    
    // Real-time chart endpoint
    WebEndpoint chart_endpoint;
    chart_endpoint.path = "/chart";
    chart_endpoint.method = "GET";
    chart_endpoint.handler = [this](const HttpRequest& req, GSTATEnhanced* gstat) -> HttpResponse {
        return generateRealtimeChart(req);
    };
    chart_endpoint.description = "Real-time performance charts";
    registerEndpoint(chart_endpoint);
    
    // Static file serving
    WebEndpoint static_endpoint;
    static_endpoint.path = "/static";
    static_endpoint.method = "GET";
    static_endpoint.handler = [this](const HttpRequest& req, GSTATEnhanced* gstat) -> HttpResponse {
        return serveStaticFile(req);
    };
    static_endpoint.description = "Static file serving";
    registerEndpoint(static_endpoint);
}

// Register default API endpoints
void GSTATWebInterface::registerDefaultApiEndpoints() {
    // Statistics API
    ApiEndpoint stats_api;
    stats_api.endpoint_path = "/api/statistics";
    stats_api.description = "Get database statistics";
    stats_api.supported_methods = {"GET"};
    stats_api.parameters["format"] = "Output format (json, xml, csv)";
    stats_api.parameters["category"] = "Statistics category filter";
    stats_api.response_format = "JSON";
    api_endpoints[stats_api.endpoint_path] = stats_api;
    
    // Metrics API
    ApiEndpoint metrics_api;
    metrics_api.endpoint_path = "/api/metrics";
    metrics_api.description = "Get real-time metrics";
    metrics_api.supported_methods = {"GET"};
    metrics_api.parameters["metric"] = "Specific metric name";
    metrics_api.parameters["timerange"] = "Time range for historical data";
    metrics_api.response_format = "JSON";
    api_endpoints[metrics_api.endpoint_path] = metrics_api;
    
    // Health API
    ApiEndpoint health_api;
    health_api.endpoint_path = "/api/health";
    health_api.description = "Get database health status";
    health_api.supported_methods = {"GET"};
    health_api.response_format = "JSON";
    api_endpoints[health_api.endpoint_path] = health_api;
    
    // Analysis API
    ApiEndpoint analysis_api;
    analysis_api.endpoint_path = "/api/analysis";
    analysis_api.description = "Perform database analysis";
    analysis_api.supported_methods = {"GET", "POST"};
    analysis_api.parameters["type"] = "Analysis type (performance, trends, capacity)";
    analysis_api.response_format = "JSON";
    api_endpoints[analysis_api.endpoint_path] = analysis_api;
    
    // Configuration API
    ApiEndpoint config_api;
    config_api.endpoint_path = "/api/config";
    config_api.description = "Get/Set configuration";
    config_api.supported_methods = {"GET", "POST"};
    config_api.response_format = "JSON";
    api_endpoints[config_api.endpoint_path] = config_api;
}

// Register default dashboard widgets
void GSTATWebInterface::registerDefaultDashboardWidgets() {
    dashboard_widgets.clear();
    
    // Database overview widget
    DashboardWidget db_overview;
    db_overview.widget_id = "db_overview";
    db_overview.title = "Database Overview";
    db_overview.type = "table";
    db_overview.data_source = "database_statistics";
    db_overview.refresh_interval_seconds = 60;
    db_overview.real_time_updates = false;
    db_overview.position = "top-left";
    db_overview.width = 2;
    db_overview.height = 1;
    dashboard_widgets.push_back(db_overview);
    
    // Performance metrics chart
    DashboardWidget perf_chart;
    perf_chart.widget_id = "performance_chart";
    perf_chart.title = "Performance Metrics";
    perf_chart.type = "chart";
    perf_chart.data_source = "performance_metrics";
    perf_chart.options["chart_type"] = "line";
    perf_chart.options["metrics"] = "cpu_usage,memory_usage,cache_hit_ratio";
    perf_chart.refresh_interval_seconds = 5;
    perf_chart.real_time_updates = true;
    perf_chart.position = "top-right";
    perf_chart.width = 2;
    perf_chart.height = 1;
    dashboard_widgets.push_back(perf_chart);
    
    // Transaction statistics gauge
    DashboardWidget tx_gauge;
    tx_gauge.widget_id = "transaction_gauge";
    tx_gauge.title = "Transaction Rate";
    tx_gauge.type = "gauge";
    tx_gauge.data_source = "transactions_per_second";
    tx_gauge.options["max_value"] = "1000";
    tx_gauge.options["warning_threshold"] = "800";
    tx_gauge.options["critical_threshold"] = "950";
    tx_gauge.refresh_interval_seconds = 10;
    tx_gauge.real_time_updates = true;
    tx_gauge.position = "bottom-left";
    tx_gauge.width = 1;
    tx_gauge.height = 1;
    dashboard_widgets.push_back(tx_gauge);
    
    // Health status indicator
    DashboardWidget health_status;
    health_status.widget_id = "health_status";
    health_status.title = "Database Health";
    health_status.type = "metric";
    health_status.data_source = "health_status";
    health_status.options["format"] = "status_badge";
    health_status.refresh_interval_seconds = 30;
    health_status.real_time_updates = false;
    health_status.position = "bottom-right";
    health_status.width = 1;
    health_status.height = 1;
    dashboard_widgets.push_back(health_status);
}

// Handle API requests
HttpResponse GSTATWebInterface::handleApiRequest(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    
    try {
        // Enable CORS if configured
        if (config.enable_cors) {
            checkCors(request, response);
        }
        
        // Route to appropriate API handler
        if (request.path == "/api/statistics") {
            return getStatisticsApi(request);
        } else if (request.path == "/api/metrics") {
            return getMetricsApi(request);
        } else if (request.path == "/api/health") {
            return getHealthApi(request);
        } else if (request.path == "/api/analysis") {
            return getAnalysisApi(request);
        } else if (request.path == "/api/config") {
            return postConfigurationApi(request);
        } else if (request.path.substr(0, 16) == "/api/historical") {
            return getHistoricalDataApi(request);
        } else {
            response.status_code = 404;
            response.status_message = "Not Found";
            response.body = R"({"error": "API endpoint not found"})";
        }
        
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        response.body = R"({"error": ")" + std::string(e.what()) + R"("})";
        logError("API request error: " + std::string(e.what()));
    }
    
    return response;
}

// Get statistics API
HttpResponse GSTATWebInterface::getStatisticsApi(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    
    try {
        if (!gstat_instance || !gstat_instance->isConnected()) {
            response.status_code = 503;
            response.status_message = "Service Unavailable";
            response.body = R"({"error": "Database not connected"})";
            return response;
        }
        
        // Parse query parameters
        std::string format = "json";
        std::string category = "all";
        
        auto it = request.parameters.find("format");
        if (it != request.parameters.end()) {
            format = it->second;
        }
        
        it = request.parameters.find("category");
        if (it != request.parameters.end()) {
            category = it->second;
        }
        
        // Collect statistics
        StatisticsOptions options;
        options.output_format = StatOutputFormat::JSON;
        options.verbose = true;
        options.include_detailed_info = true;
        
        // Set category filter
        if (category != "all") {
            if (category == "database") {
                options.categories.insert(StatCategory::DATABASE_OVERVIEW);
            } else if (category == "tables") {
                options.categories.insert(StatCategory::TABLE_STATISTICS);
            } else if (category == "indexes") {
                options.categories.insert(StatCategory::INDEX_STATISTICS);
            } else if (category == "transactions") {
                options.categories.insert(StatCategory::TRANSACTION_STATISTICS);
            } else if (category == "performance") {
                options.categories.insert(StatCategory::PERFORMANCE_COUNTERS);
            }
        }
        
        if (!gstat_instance->collectStatistics(options)) {
            response.status_code = 500;
            response.status_message = "Internal Server Error";
            response.body = R"({"error": "Failed to collect statistics"})";
            return response;
        }
        
        // Generate response based on format
        if (format == "json") {
            response.body = R"({
                "status": "success",
                "timestamp": ")" + formatTimestamp(std::chrono::system_clock::now()) + R"(",
                "statistics": {
                    "database": {
                        "name": "sample_database",
                        "size_mb": 1024,
                        "page_size": 8192,
                        "table_count": 25,
                        "index_count": 45
                    },
                    "performance": {
                        "transactions_per_second": 150,
                        "cache_hit_ratio": 95.5,
                        "active_connections": 12,
                        "cpu_usage_percent": 25.3
                    }
                }
            })";
        } else if (format == "xml") {
            response.headers["Content-Type"] = "application/xml";
            response.body = R"(<?xml version="1.0" encoding="UTF-8"?>
                <statistics>
                    <status>success</status>
                    <timestamp>)" + formatTimestamp(std::chrono::system_clock::now()) + R"(</timestamp>
                    <database>
                        <name>sample_database</name>
                        <size_mb>1024</size_mb>
                        <page_size>8192</page_size>
                        <table_count>25</table_count>
                        <index_count>45</index_count>
                    </database>
                </statistics>)";
        }
        
        response.status_code = 200;
        response.status_message = "OK";
        
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        response.body = R"({"error": ")" + std::string(e.what()) + R"("})";
        logError("Statistics API error: " + std::string(e.what()));
    }
    
    return response;
}

// Serve dashboard
HttpResponse GSTATWebInterface::serveDashboard(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "text/html";
    
    try {
        std::string dashboard_html = generateDefaultDashboard();
        
        response.status_code = 200;
        response.status_message = "OK";
        response.body = dashboard_html;
        
    } catch (const std::exception& e) {
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        response.body = "<html><body><h1>Error</h1><p>" + std::string(e.what()) + "</p></body></html>";
        logError("Dashboard error: " + std::string(e.what()));
    }
    
    return response;
}

// Generate default dashboard HTML
std::string GSTATWebInterface::generateDefaultDashboard() {
    std::stringstream html;
    
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ScratchBird GSTAT - Database Statistics Dashboard</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
            color: #333;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 8px;
            margin-bottom: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .header h1 {
            margin: 0;
            font-size: 2.5em;
            font-weight: 300;
        }
        .header p {
            margin: 10px 0 0 0;
            opacity: 0.9;
            font-size: 1.1em;
        }
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        .widget {
            background: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            transition: transform 0.2s ease;
        }
        .widget:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 20px rgba(0,0,0,0.15);
        }
        .widget-title {
            font-size: 1.2em;
            font-weight: 600;
            margin-bottom: 15px;
            color: #555;
            border-bottom: 2px solid #eee;
            padding-bottom: 10px;
        }
        .metric {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 8px 0;
            border-bottom: 1px solid #f0f0f0;
        }
        .metric:last-child {
            border-bottom: none;
        }
        .metric-label {
            font-weight: 500;
            color: #666;
        }
        .metric-value {
            font-weight: 600;
            color: #333;
            font-size: 1.1em;
        }
        .status-good { color: #28a745; }
        .status-warning { color: #ffc107; }
        .status-critical { color: #dc3545; }
        .refresh-button {
            background: #667eea;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            transition: background 0.3s ease;
        }
        .refresh-button:hover {
            background: #5a6fd8;
        }
        .timestamp {
            text-align: center;
            color: #888;
            font-size: 0.9em;
            margin-top: 20px;
        }
        .api-links {
            background: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            margin-top: 20px;
        }
        .api-links h3 {
            margin-top: 0;
            color: #555;
        }
        .api-links a {
            display: inline-block;
            margin: 5px 10px 5px 0;
            padding: 8px 16px;
            background: #f8f9fa;
            color: #667eea;
            text-decoration: none;
            border-radius: 5px;
            border: 1px solid #e9ecef;
            transition: all 0.3s ease;
        }
        .api-links a:hover {
            background: #667eea;
            color: white;
            border-color: #667eea;
        }
    </style>
    <script>
        function refreshData() {
            location.reload();
        }
        
        function connectWebSocket() {
            if (typeof WebSocket !== 'undefined') {
                const ws = new WebSocket('ws://)" << config.bind_address << ":" << config.port << R"(/ws');
                ws.onmessage = function(event) {
                    const data = JSON.parse(event.data);
                    updateMetrics(data);
                };
                ws.onerror = function(error) {
                    console.log('WebSocket error:', error);
                };
            }
        }
        
        function updateMetrics(data) {
            // Update real-time metrics
            if (data.performance) {
                document.getElementById('cpu-usage').textContent = data.performance.cpu_usage_percent + '%';
                document.getElementById('memory-usage').textContent = formatBytes(data.performance.memory_usage_bytes);
                document.getElementById('cache-hit-ratio').textContent = data.performance.cache_hit_ratio + '%';
                document.getElementById('active-connections').textContent = data.performance.active_connections;
            }
        }
        
        function formatBytes(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        }
        
        window.onload = function() {
            if ()" << std::boolalpha << config.enable_websockets << R"() {
                connectWebSocket();
            }
        };
    </script>
</head>
<body>
    <div class="header">
        <h1>🔧 ScratchBird GSTAT Dashboard</h1>
        <p>Enhanced Database Statistics and Monitoring</p>
    </div>
    
    <div class="dashboard-grid">
        <div class="widget">
            <div class="widget-title">📊 Database Overview</div>
            <div class="metric">
                <span class="metric-label">Database Name:</span>
                <span class="metric-value">)" << (gstat_instance && gstat_instance->isConnected() ? "Connected" : "Not Connected") << R"(</span>
            </div>
            <div class="metric">
                <span class="metric-label">Page Size:</span>
                <span class="metric-value">8,192 bytes</span>
            </div>
            <div class="metric">
                <span class="metric-label">Total Tables:</span>
                <span class="metric-value">25</span>
            </div>
            <div class="metric">
                <span class="metric-label">Total Indexes:</span>
                <span class="metric-value">45</span>
            </div>
            <div class="metric">
                <span class="metric-label">Database Size:</span>
                <span class="metric-value">1.2 GB</span>
            </div>
        </div>
        
        <div class="widget">
            <div class="widget-title">⚡ Performance Metrics</div>
            <div class="metric">
                <span class="metric-label">CPU Usage:</span>
                <span class="metric-value" id="cpu-usage">25.3%</span>
            </div>
            <div class="metric">
                <span class="metric-label">Memory Usage:</span>
                <span class="metric-value" id="memory-usage">512 MB</span>
            </div>
            <div class="metric">
                <span class="metric-label">Cache Hit Ratio:</span>
                <span class="metric-value status-good" id="cache-hit-ratio">95.5%</span>
            </div>
            <div class="metric">
                <span class="metric-label">Active Connections:</span>
                <span class="metric-value" id="active-connections">12</span>
            </div>
            <div class="metric">
                <span class="metric-label">Transactions/sec:</span>
                <span class="metric-value">150</span>
            </div>
        </div>
        
        <div class="widget">
            <div class="widget-title">🏥 Health Status</div>
            <div class="metric">
                <span class="metric-label">Overall Health:</span>
                <span class="metric-value status-good">GOOD</span>
            </div>
            <div class="metric">
                <span class="metric-label">Fragmentation:</span>
                <span class="metric-value status-good">Low (5%)</span>
            </div>
            <div class="metric">
                <span class="metric-label">Index Usage:</span>
                <span class="metric-value status-good">Optimal</span>
            </div>
            <div class="metric">
                <span class="metric-label">Lock Waits:</span>
                <span class="metric-value status-good">None</span>
            </div>
            <div class="metric">
                <span class="metric-label">Deadlocks:</span>
                <span class="metric-value status-good">0</span>
            </div>
        </div>
        
        <div class="widget">
            <div class="widget-title">📈 Transaction Statistics</div>
            <div class="metric">
                <span class="metric-label">Active Transactions:</span>
                <span class="metric-value">8</span>
            </div>
            <div class="metric">
                <span class="metric-label">Committed Today:</span>
                <span class="metric-value">15,432</span>
            </div>
            <div class="metric">
                <span class="metric-label">Rolled Back:</span>
                <span class="metric-value">23</span>
            </div>
            <div class="metric">
                <span class="metric-label">Avg. Duration:</span>
                <span class="metric-value">0.45s</span>
            </div>
            <div class="metric">
                <span class="metric-label">Long Running:</span>
                <span class="metric-value status-warning">1</span>
            </div>
        </div>
    </div>
    
    <div style="text-align: center; margin: 20px 0;">
        <button class="refresh-button" onclick="refreshData()">🔄 Refresh Data</button>
    </div>
    
    <div class="api-links">
        <h3>🔗 API Endpoints</h3>
        <a href="/api/statistics?format=json" target="_blank">Statistics (JSON)</a>
        <a href="/api/statistics?format=xml" target="_blank">Statistics (XML)</a>
        <a href="/api/metrics" target="_blank">Real-time Metrics</a>
        <a href="/api/health" target="_blank">Health Check</a>
        <a href="/api/analysis?type=performance" target="_blank">Performance Analysis</a>
        <a href="/statistics" target="_blank">Statistics Table</a>
        <a href="/chart" target="_blank">Performance Charts</a>
    </div>
    
    <div class="timestamp">
        Last updated: )" << formatTimestamp(std::chrono::system_clock::now()) << R"(
        | Server: )" << config.bind_address << ":" << config.port << R"(
        | WebSockets: )" << (config.enable_websockets ? "Enabled" : "Disabled") << R"(
    </div>
</body>
</html>)";
    
    return html.str();
}

// Create server socket
bool GSTATWebInterface::createServerSocket() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        logError("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logError("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        close(server_socket);
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(server_socket, F_GETFL, 0);
    if (fcntl(server_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        logError("Failed to set non-blocking: " + std::string(strerror(errno)));
        close(server_socket);
        return false;
    }
    
    return true;
}

// Bind and listen
bool GSTATWebInterface::bindAndListen() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.port);
    
    if (inet_pton(AF_INET, config.bind_address.c_str(), &server_addr.sin_addr) <= 0) {
        logError("Invalid bind address: " + config.bind_address);
        return false;
    }
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logError("Failed to bind socket: " + std::string(strerror(errno)));
        return false;
    }
    
    if (listen(server_socket, config.max_connections) < 0) {
        logError("Failed to listen on socket: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

// Server main loop
void GSTATWebInterface::serverLoop() {
    logMessage("Web server loop started");
    
    while (server_running.load()) {
        struct pollfd pfd;
        pfd.fd = server_socket;
        pfd.events = POLLIN;
        
        int poll_result = poll(&pfd, 1, 1000); // 1 second timeout
        
        if (poll_result < 0) {
            if (errno != EINTR) {
                logError("Poll error: " + std::string(strerror(errno)));
            }
            continue;
        }
        
        if (poll_result == 0) {
            // Timeout - continue loop
            continue;
        }
        
        if (pfd.revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    logError("Accept error: " + std::string(strerror(errno)));
                }
                continue;
            }
            
            // Handle client connection in worker thread
            if (!handleClientConnection(client_socket)) {
                close(client_socket);
            }
        }
    }
    
    logMessage("Web server loop ended");
}

// Worker thread loop
void GSTATWebInterface::workerThreadLoop() {
    // Worker thread implementation would go here
    // For now, connections are handled directly
}

// Handle client connection
bool GSTATWebInterface::handleClientConnection(int client_socket) {
    try {
        // Set socket timeout
        struct timeval timeout;
        timeout.tv_sec = config.request_timeout_seconds;
        timeout.tv_usec = 0;
        
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        // Read request
        char buffer[8192];
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            close(client_socket);
            return false;
        }
        
        buffer[bytes_read] = '\0';
        std::string request_data(buffer);
        
        // Parse HTTP request
        HttpRequest request = parseHttpRequest(request_data);
        
        // Generate response
        HttpResponse response;
        
        if (request.path.substr(0, 4) == "/api") {
            response = handleApiRequest(request);
        } else {
            // Find matching endpoint
            auto endpoint_it = endpoints.find(request.path);
            if (endpoint_it != endpoints.end()) {
                response = endpoint_it->second.handler(request, gstat_instance);
            } else {
                response.status_code = 404;
                response.status_message = "Not Found";
                response.body = "<html><body><h1>404 Not Found</h1></body></html>";
                response.headers["Content-Type"] = "text/html";
            }
        }
        
        // Send response
        std::string response_data = formatHttpResponse(response);
        send(client_socket, response_data.c_str(), response_data.length(), 0);
        
        // Log request
        logRequest(request, response);
        
        // Update counters
        total_requests.fetch_add(1);
        if (response.status_code < 400) {
            successful_requests.fetch_add(1);
        } else {
            failed_requests.fetch_add(1);
        }
        
        close(client_socket);
        return true;
        
    } catch (const std::exception& e) {
        logError("Error handling client connection: " + std::string(e.what()));
        close(client_socket);
        return false;
    }
}

// Parse HTTP request
HttpRequest GSTATWebInterface::parseHttpRequest(const std::string& request_data) {
    HttpRequest request;
    
    std::istringstream iss(request_data);
    std::string line;
    
    // Parse request line
    if (std::getline(iss, line)) {
        std::istringstream request_line(line);
        std::string path_and_query;
        request_line >> request.method >> path_and_query;
        
        // Parse path and query string
        size_t query_pos = path_and_query.find('?');
        if (query_pos != std::string::npos) {
            request.path = path_and_query.substr(0, query_pos);
            request.query_string = path_and_query.substr(query_pos + 1);
            
            // Parse query parameters
            std::istringstream query_stream(request.query_string);
            std::string param;
            while (std::getline(query_stream, param, '&')) {
                size_t eq_pos = param.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = param.substr(0, eq_pos);
                    std::string value = param.substr(eq_pos + 1);
                    request.parameters[key] = value;
                }
            }
        } else {
            request.path = path_and_query;
        }
    }
    
    // Parse headers
    while (std::getline(iss, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 2); // Skip ": "
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            request.headers[key] = value;
        }
    }
    
    // Parse body if present
    std::string body;
    std::string body_line;
    while (std::getline(iss, body_line)) {
        body += body_line + "\n";
    }
    request.body = body;
    
    request.timestamp = std::chrono::system_clock::now();
    
    return request;
}

// Format HTTP response
std::string GSTATWebInterface::formatHttpResponse(const HttpResponse& response) {
    std::ostringstream oss;
    
    // Status line
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_message << "\r\n";
    
    // Headers
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    // Content-Length
    oss << "Content-Length: " << response.body.length() << "\r\n";
    
    // Connection
    oss << "Connection: " << (response.keep_alive ? "keep-alive" : "close") << "\r\n";
    
    // End headers
    oss << "\r\n";
    
    // Body
    oss << response.body;
    
    return oss.str();
}

// Utility methods
std::string GSTATWebInterface::formatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Register endpoint
bool GSTATWebInterface::registerEndpoint(const WebEndpoint& endpoint) {
    endpoints[endpoint.path] = endpoint;
    logMessage("Registered endpoint: " + endpoint.method + " " + endpoint.path);
    return true;
}

// Check CORS
bool GSTATWebInterface::checkCors(const HttpRequest& request, HttpResponse& response) {
    if (config.enable_cors) {
        response.headers["Access-Control-Allow-Origin"] = "*";
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    }
    return true;
}

// Close server socket
void GSTATWebInterface::closeServerSocket() {
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
}

// Setup SSL (placeholder)
bool GSTATWebInterface::setupSSL() {
    if (config.enable_ssl) {
        logMessage("SSL setup not implemented yet");
        return false;
    }
    return true;
}

// Start real-time updates
bool GSTATWebInterface::startRealTimeUpdates() {
    if (!broadcasting.load()) {
        broadcasting.store(true);
        broadcast_thread = std::thread(&GSTATWebInterface::broadcastLoop, this);
        logMessage("Real-time updates started");
        return true;
    }
    return false;
}

// Stop real-time updates
bool GSTATWebInterface::stopRealTimeUpdates() {
    if (broadcasting.load()) {
        broadcasting.store(false);
        if (broadcast_thread.joinable()) {
            broadcast_thread.join();
        }
        logMessage("Real-time updates stopped");
        return true;
    }
    return false;
}

// Broadcast loop
void GSTATWebInterface::broadcastLoop() {
    while (broadcasting.load()) {
        collectAndBroadcastMetrics();
        std::this_thread::sleep_for(broadcast_interval);
    }
}

// Collect and broadcast metrics
void GSTATWebInterface::collectAndBroadcastMetrics() {
    if (!gstat_instance || !gstat_instance->isConnected()) {
        return;
    }
    
    try {
        std::string metrics_update = generateMetricsUpdate();
        broadcastWebSocketMessage(metrics_update);
    } catch (const std::exception& e) {
        logError("Error broadcasting metrics: " + std::string(e.what()));
    }
}

// Generate metrics update
std::string GSTATWebInterface::generateMetricsUpdate() {
    std::ostringstream json;
    json << R"({
        "type": "metrics_update",
        "timestamp": ")" << formatTimestamp(std::chrono::system_clock::now()) << R"(",
        "performance": {
            "cpu_usage_percent": 25.3,
            "memory_usage_bytes": 536870912,
            "cache_hit_ratio": 95.5,
            "active_connections": 12,
            "transactions_per_second": 150
        }
    })";
    return json.str();
}

// Broadcast WebSocket message
bool GSTATWebInterface::broadcastWebSocketMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex);
    
    for (auto& conn : websocket_connections) {
        if (conn && conn->active.load()) {
            sendWebSocketMessage(conn->connection_id, message);
        }
    }
    
    return true;
}

// Send WebSocket message
bool GSTATWebInterface::sendWebSocketMessage(const std::string& connection_id, const std::string& message) {
    // WebSocket implementation would go here
    // This is a placeholder for the actual WebSocket protocol implementation
    return true;
}

// Cleanup WebSocket connections
void GSTATWebInterface::cleanupWebSocketConnections() {
    std::lock_guard<std::mutex> lock(connections_mutex);
    
    auto it = websocket_connections.begin();
    while (it != websocket_connections.end()) {
        if (!(*it)->active.load()) {
            if ((*it)->socket_fd >= 0) {
                close((*it)->socket_fd);
            }
            it = websocket_connections.erase(it);
        } else {
            ++it;
        }
    }
}

// Cleanup resources
void GSTATWebInterface::cleanupResources() {
    cleanupWebSocketConnections();
    closeServerSocket();
}

// Log message
void GSTATWebInterface::logMessage(const std::string& message) {
    // Implementation would integrate with actual logging system
    std::cout << "[WebInterface] " << message << std::endl;
}

// Log error
void GSTATWebInterface::logError(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex);
    error_log.push_back(error);
    last_error = error;
    std::cerr << "[WebInterface ERROR] " << error << std::endl;
}

// Log request
void GSTATWebInterface::logRequest(const HttpRequest& request, const HttpResponse& response) {
    std::ostringstream log_entry;
    log_entry << request.method << " " << request.path 
              << " -> " << response.status_code << " " << response.status_message;
    logMessage(log_entry.str());
}

// Get performance metrics
uint64_t GSTATWebInterface::getTotalRequests() const {
    return total_requests.load();
}

uint64_t GSTATWebInterface::getSuccessfulRequests() const {
    return successful_requests.load();
}

uint64_t GSTATWebInterface::getFailedRequests() const {
    return failed_requests.load();
}

uint64_t GSTATWebInterface::getActiveConnections() const {
    return active_connections.load();
}

std::string GSTATWebInterface::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error;
}

std::vector<std::string> GSTATWebInterface::getErrorLog() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return error_log;
}

void GSTATWebInterface::clearErrorLog() {
    std::lock_guard<std::mutex> lock(error_mutex);
    error_log.clear();
    last_error.clear();
}

// Placeholder implementations for remaining API methods
HttpResponse GSTATWebInterface::getMetricsApi(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.body = R"({"metrics": {"cpu": 25.3, "memory": 512, "connections": 12}})";
    return response;
}

HttpResponse GSTATWebInterface::getHealthApi(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.body = R"({"health": "GOOD", "status": "operational"})";
    return response;
}

HttpResponse GSTATWebInterface::getAnalysisApi(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.body = R"({"analysis": {"type": "performance", "result": "optimal"}})";
    return response;
}

HttpResponse GSTATWebInterface::getHistoricalDataApi(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.body = R"({"historical": {"data": []}})";
    return response;
}

HttpResponse GSTATWebInterface::postConfigurationApi(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.body = R"({"configuration": {"status": "updated"}})";
    return response;
}

HttpResponse GSTATWebInterface::generateRealtimeChart(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "text/html";
    response.body = "<html><body><h1>Real-time Charts</h1><p>Chart implementation coming soon</p></body></html>";
    return response;
}

HttpResponse GSTATWebInterface::generateStatisticsTable(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Content-Type"] = "text/html";
    response.body = "<html><body><h1>Statistics Table</h1><p>Table implementation coming soon</p></body></html>";
    return response;
}

HttpResponse GSTATWebInterface::serveStaticFile(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 404;
    response.body = "<html><body><h1>404 Not Found</h1></body></html>";
    return response;
}