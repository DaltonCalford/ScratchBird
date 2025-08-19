#pragma once

#include "sb_gstat_enhanced.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>

namespace SBEnhanced {

// HTTP request structure
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> parameters;
    std::string body;
    std::string remote_address;
    std::chrono::system_clock::time_point timestamp;
};

// HTTP response structure
struct HttpResponse {
    int status_code = 200;
    std::string status_message = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    std::string content_type = "text/html";
    bool keep_alive = false;
};

// Web endpoint configuration
struct WebEndpoint {
    std::string path;
    std::string method;
    std::function<HttpResponse(const HttpRequest&, GSTATEnhanced*)> handler;
    bool requires_auth = false;
    std::vector<std::string> allowed_roles;
    std::string description;
};

// WebSocket connection for real-time updates
struct WebSocketConnection {
    int socket_fd = -1;
    std::string connection_id;
    std::string remote_address;
    std::chrono::system_clock::time_point connected_time;
    std::atomic<bool> active{true};
    std::mutex send_mutex;
    std::queue<std::string> message_queue;
    std::set<std::string> subscribed_metrics;
};

// Web server configuration
struct WebServerConfig {
    std::string bind_address = "127.0.0.1";
    int port = 8080;
    int max_connections = 100;
    int request_timeout_seconds = 30;
    std::string document_root = "./web";
    std::string ssl_cert_file;
    std::string ssl_key_file;
    bool enable_ssl = false;
    bool enable_websockets = true;
    bool enable_cors = true;
    std::string auth_token;
    std::vector<std::string> allowed_origins;
    int worker_threads = 4;
};

// Dashboard widget configuration
struct DashboardWidget {
    std::string widget_id;
    std::string title;
    std::string type; // "chart", "table", "metric", "gauge", "text"
    std::string data_source; // metric name or query
    std::map<std::string, std::string> options;
    int refresh_interval_seconds = 60;
    bool real_time_updates = false;
    std::string position; // "top-left", "top-right", etc.
    int width = 1;
    int height = 1;
};

// API endpoint for statistics
struct ApiEndpoint {
    std::string endpoint_path;
    std::string description;
    std::vector<std::string> supported_methods;
    std::map<std::string, std::string> parameters;
    std::string response_format;
    bool requires_database_connection = true;
};

} // namespace SBEnhanced

// Enhanced GSTAT Web Interface class
class GSTATWebInterface {
private:
    // Core components
    GSTATEnhanced* gstat_instance;
    SBEnhanced::WebServerConfig config;
    
    // Web server infrastructure
    std::atomic<bool> server_running{false};
    std::thread server_thread;
    std::vector<std::thread> worker_threads;
    int server_socket = -1;
    
    // Connection management
    std::vector<std::unique_ptr<SBEnhanced::WebSocketConnection>> websocket_connections;
    std::mutex connections_mutex;
    std::atomic<int> next_connection_id{1};
    
    // Endpoint routing
    std::map<std::string, SBEnhanced::WebEndpoint> endpoints;
    std::map<std::string, SBEnhanced::ApiEndpoint> api_endpoints;
    
    // Dashboard management
    std::vector<SBEnhanced::DashboardWidget> dashboard_widgets;
    std::map<std::string, std::string> dashboard_templates;
    std::mutex dashboard_mutex;
    
    // Real-time data broadcasting
    std::thread broadcast_thread;
    std::atomic<bool> broadcasting{false};
    std::chrono::seconds broadcast_interval{5};
    
    // Security and authentication
    std::map<std::string, std::string> session_tokens;
    std::mutex auth_mutex;
    std::chrono::hours token_lifetime{24};
    
    // Performance tracking
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> active_connections{0};
    std::map<std::string, std::chrono::microseconds> endpoint_response_times;

public:
    GSTATWebInterface(GSTATEnhanced* gstat);
    ~GSTATWebInterface();
    
    // Server lifecycle
    bool initialize(const SBEnhanced::WebServerConfig& config);
    bool start();
    bool stop();
    bool isRunning() const;
    bool restart();
    
    // Configuration management
    bool loadConfiguration(const std::string& config_file);
    bool saveConfiguration(const std::string& config_file);
    void setConfig(const SBEnhanced::WebServerConfig& config);
    SBEnhanced::WebServerConfig getConfig() const;
    
    // Endpoint management
    bool registerEndpoint(const SBEnhanced::WebEndpoint& endpoint);
    bool unregisterEndpoint(const std::string& path);
    std::vector<SBEnhanced::WebEndpoint> getEndpoints() const;
    bool registerApiEndpoint(const SBEnhanced::ApiEndpoint& endpoint);
    
    // Dashboard management
    bool createDashboard(const std::string& dashboard_name, 
                        const std::vector<SBEnhanced::DashboardWidget>& widgets);
    bool updateDashboard(const std::string& dashboard_name, 
                        const std::vector<SBEnhanced::DashboardWidget>& widgets);
    bool deleteDashboard(const std::string& dashboard_name);
    std::vector<std::string> getDashboards() const;
    std::string generateDashboardHTML(const std::string& dashboard_name);
    
    // Widget management
    bool addWidget(const std::string& dashboard_name, const SBEnhanced::DashboardWidget& widget);
    bool removeWidget(const std::string& dashboard_name, const std::string& widget_id);
    bool updateWidget(const std::string& dashboard_name, const SBEnhanced::DashboardWidget& widget);
    std::vector<SBEnhanced::DashboardWidget> getWidgets(const std::string& dashboard_name) const;
    
    // Real-time updates
    bool startRealTimeUpdates();
    bool stopRealTimeUpdates();
    bool broadcastMetricUpdate(const std::string& metric_name, const std::string& value);
    bool subscribeToMetric(const std::string& connection_id, const std::string& metric_name);
    bool unsubscribeFromMetric(const std::string& connection_id, const std::string& metric_name);
    
    // WebSocket management
    bool handleWebSocketConnection(int client_socket);
    bool sendWebSocketMessage(const std::string& connection_id, const std::string& message);
    bool broadcastWebSocketMessage(const std::string& message);
    void cleanupWebSocketConnections();
    
    // Authentication and security
    std::string generateAuthToken(const std::string& username);
    bool validateAuthToken(const std::string& token);
    bool revokeAuthToken(const std::string& token);
    void cleanupExpiredTokens();
    
    // API endpoints
    SBEnhanced::HttpResponse handleApiRequest(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse getStatisticsApi(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse getMetricsApi(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse getHealthApi(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse getAnalysisApi(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse getHistoricalDataApi(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse postConfigurationApi(const SBEnhanced::HttpRequest& request);
    
    // Dashboard endpoints
    SBEnhanced::HttpResponse serveDashboard(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse serveStaticFile(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse generateRealtimeChart(const SBEnhanced::HttpRequest& request);
    SBEnhanced::HttpResponse generateStatisticsTable(const SBEnhanced::HttpRequest& request);
    
    // Utility methods
    std::string formatResponseAsJSON(const std::map<std::string, std::string>& data);
    std::string formatResponseAsXML(const std::map<std::string, std::string>& data);
    std::string formatResponseAsCSV(const std::vector<std::map<std::string, std::string>>& data);
    std::string parseQueryString(const std::string& query_string);
    std::map<std::string, std::string> parseFormData(const std::string& form_data);
    
    // Performance monitoring
    uint64_t getTotalRequests() const;
    uint64_t getSuccessfulRequests() const;
    uint64_t getFailedRequests() const;
    uint64_t getActiveConnections() const;
    std::chrono::microseconds getAverageResponseTime(const std::string& endpoint) const;
    
    // Error handling
    std::string getLastError() const;
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();

private:
    // Server implementation
    void serverLoop();
    void workerThreadLoop();
    bool handleClientConnection(int client_socket);
    SBEnhanced::HttpRequest parseHttpRequest(const std::string& request_data);
    std::string formatHttpResponse(const SBEnhanced::HttpResponse& response);
    
    // Real-time broadcasting
    void broadcastLoop();
    void collectAndBroadcastMetrics();
    std::string generateMetricsUpdate();
    
    // Template processing
    std::string processTemplate(const std::string& template_content, 
                               const std::map<std::string, std::string>& variables);
    std::string loadTemplate(const std::string& template_name);
    std::string generateDefaultDashboard();
    
    // Default endpoint registration
    void registerDefaultEndpoints();
    void registerDefaultApiEndpoints();
    void registerDefaultDashboardWidgets();
    
    // Security helpers
    bool validateRequest(const SBEnhanced::HttpRequest& request);
    bool checkCors(const SBEnhanced::HttpRequest& request, SBEnhanced::HttpResponse& response);
    std::string generateCSRFToken();
    bool validateCSRFToken(const std::string& token);
    
    // Network utilities
    bool createServerSocket();
    bool bindAndListen();
    bool setupSSL();
    void closeServerSocket();
    std::string getClientAddress(int client_socket);
    
    // Resource management
    void cleanupResources();
    void cleanupInactiveConnections();
    void limitConnections();
    
    // Error logging
    void logError(const std::string& error);
    void logRequest(const SBEnhanced::HttpRequest& request, const SBEnhanced::HttpResponse& response);
    void logWebSocketEvent(const std::string& event, const std::string& connection_id);
    
    // HTML generation helpers
    std::string generateNavigationHTML();
    std::string generateHeaderHTML(const std::string& title);
    std::string generateFooterHTML();
    std::string generateChartJS(const SBEnhanced::DashboardWidget& widget);
    std::string generateTableHTML(const std::vector<std::map<std::string, std::string>>& data);
    
    // JavaScript and CSS resources
    std::string getJavaScriptLibraries();
    std::string getCSSStyles();
    std::string getRealtimeUpdateScript();
    std::string getWebSocketScript();
    
    // Error handling
    std::vector<std::string> error_log;
    std::mutex error_mutex;
    std::string last_error;
};