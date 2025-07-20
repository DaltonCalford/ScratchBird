# sb_gstat - Database Statistics & Analysis 🟡

sb_gstat is ScratchBird's enhanced database statistics and analysis utility that provides comprehensive database performance monitoring, optimization recommendations, and detailed reporting. It offers 100% compatibility with Firebird's GSTAT while adding advanced analytics, web-based reports, and predictive insights.

## 🚀 Quick Start

### **Basic Database Information**
```bash
# Database header information
sb_gstat -header -user SYSDBA -password masterkey mydatabase.fdb

# Table statistics overview
sb_gstat -data -user SYSDBA mydatabase.fdb

# Index analysis
sb_gstat -index -user SYSDBA mydatabase.fdb
```

### **Performance Analysis**
```bash
# Comprehensive analysis with recommendations
sb_gstat -analyze -recommendations -user SYSDBA mydatabase.fdb

# Cache efficiency report
sb_gstat -cache -user SYSDBA mydatabase.fdb

# Real-time monitoring
sb_gstat -monitor -interval 30 -user SYSDBA mydatabase.fdb
```

---

## 📋 Command Reference

### **Analysis Modes**
| Option | Description | Example |
|--------|-------------|---------|
| `-header` | Database header information | `-header` |
| `-data` | Table and record statistics | `-data` |
| `-index` | Index statistics and usage | `-index` |
| `-system` | System table information | `-system` |
| `-record` | Record-level statistics | `-record` |

### **Enhanced Analysis** (ScratchBird Features)
| Option | Description | Example |
|--------|-------------|---------|
| `-analyze` | Comprehensive performance analysis | `-analyze` |
| `-recommendations` | Optimization recommendations | `-recommendations` |
| `-cache` | Cache usage analysis | `-cache` |
| `-monitor` | Real-time monitoring | `-monitor` |
| `-performance` | Performance bottleneck analysis | `-performance` |
| `-trends` | Historical trend analysis | `-trends` |

### **Filtering Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-table <name>` | Analyze specific table | `-table customers` |
| `-schema <name>` | Analyze specific schema | `-schema finance.accounting` |
| `-relation <name>` | Analyze specific relation | `-relation orders` |
| `-exclude <pattern>` | Exclude tables matching pattern | `-exclude temp_*` |

### **Output Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-output <file>` | Write output to file | `-output report.txt` |
| `-format <type>` | Output format (text, json, xml, html) | `-format html` |
| `-verbose` | Detailed output | `-verbose` |
| `-brief` | Summary output only | `-brief` |
| `-stats` | Include statistical analysis | `-stats` |

### **Monitoring Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-interval <seconds>` | Monitoring interval | `-interval 60` |
| `-samples <count>` | Number of samples to collect | `-samples 100` |
| `-web-interface` | Start web monitoring interface | `-web-interface 8080` |
| `-alert <threshold>` | Set alert thresholds | `-alert cpu:80,memory:90` |

### **Connection Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-user <username>` | Database username | `-user SYSDBA` |
| `-password <password>` | Database password | `-password masterkey` |
| `-role <role>` | SQL role name | `-role DB_ADMIN` |
| `-trusted` | Use trusted authentication | `-trusted` |

---

## 🔧 Advanced Features

### **Comprehensive Database Analysis**
ScratchBird's enhanced analysis provides detailed insights:

```bash
# Full database health check
sb_gstat -analyze -comprehensive -user SYSDBA mydatabase.fdb

# Performance bottleneck identification
sb_gstat -analyze -performance -bottlenecks -user SYSDBA mydatabase.fdb

# Schema-specific analysis
sb_gstat -analyze -schema "ecommerce.orders" -recommendations mydatabase.fdb

# Generate optimization script
sb_gstat -analyze -generate-script optimize.sql -user SYSDBA mydatabase.fdb
```

### **Cache Analysis**
Monitor and optimize database cache usage:

```bash
# Cache hit ratio analysis
sb_gstat -cache -hit-ratio -user SYSDBA mydatabase.fdb

# Page cache distribution
sb_gstat -cache -distribution -verbose -user SYSDBA mydatabase.fdb

# Cache efficiency over time
sb_gstat -cache -trends -timeframe "last 24 hours" mydatabase.fdb

# Cache tuning recommendations
sb_gstat -cache -recommendations -target-ratio 95 mydatabase.fdb
```

### **Real-Time Monitoring**
Continuous database monitoring with alerts:

```bash
# Start real-time monitoring dashboard
sb_gstat -monitor -web-interface 8080 -user SYSDBA mydatabase.fdb

# Monitor with custom alerts
sb_gstat -monitor \
    -alert "cache_hit_ratio<90,page_reads>1000,active_connections>50" \
    -interval 30 \
    -log monitoring.log \
    mydatabase.fdb

# Export monitoring data
sb_gstat -monitor \
    -samples 1440 \
    -interval 60 \
    -export monitoring_data.json \
    mydatabase.fdb
```

### **Historical Trend Analysis**
Analyze database performance trends:

```bash
# Weekly performance trends
sb_gstat -trends -timeframe "last 7 days" -user SYSDBA mydatabase.fdb

# Compare time periods
sb_gstat -trends \
    -compare "last week" "this week" \
    -format html \
    -output comparison_report.html \
    mydatabase.fdb

# Predictive analysis
sb_gstat -trends -predict -forecast-days 30 -user SYSDBA mydatabase.fdb
```

### **Multi-Format Reporting**
Generate reports in various formats:

```bash
# HTML report with charts
sb_gstat -analyze -format html -output database_report.html mydatabase.fdb

# JSON for integration with monitoring tools
sb_gstat -analyze -format json -output metrics.json mydatabase.fdb

# XML for enterprise reporting
sb_gstat -analyze -format xml -schema-aware -output report.xml mydatabase.fdb

# CSV for spreadsheet analysis
sb_gstat -data -format csv -output tables_stats.csv mydatabase.fdb
```

---

## 💼 Real-World Examples

### **Daily Health Check Script**
```bash
#!/bin/bash
# daily_health_check.sh - Comprehensive database health monitoring

DB_FILE="production.fdb"
REPORT_DIR="/reports/health_checks"
DATE_STAMP=$(date +%Y-%m-%d)
REPORT_FILE="$REPORT_DIR/health_check_$DATE_STAMP.html"

# Ensure report directory exists
mkdir -p "$REPORT_DIR"

echo "Starting daily health check for $DB_FILE at $(date)"

# Generate comprehensive health report
sb_gstat -analyze \
    -comprehensive \
    -recommendations \
    -performance \
    -cache \
    -trends \
    -format html \
    -output "$REPORT_FILE" \
    -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    "$DB_FILE"

if [ $? -eq 0 ]; then
    echo "Health check completed successfully"
    
    # Extract key metrics for alerting
    CACHE_HIT_RATIO=$(sb_gstat -cache -brief "$DB_FILE" | grep "Hit ratio" | awk '{print $3}' | sed 's/%//')
    ACTIVE_CONNECTIONS=$(sb_gstat -monitor -samples 1 "$DB_FILE" | grep "Active connections" | awk '{print $3}')
    
    # Check for alerts
    if [ "$CACHE_HIT_RATIO" -lt 85 ]; then
        echo "ALERT: Low cache hit ratio: $CACHE_HIT_RATIO%" | \
            mail -s "Database Alert - Low Cache Performance" admin@company.com
    fi
    
    if [ "$ACTIVE_CONNECTIONS" -gt 100 ]; then
        echo "ALERT: High connection count: $ACTIVE_CONNECTIONS" | \
            mail -s "Database Alert - High Connection Count" admin@company.com
    fi
    
    # Clean up old reports (keep 30 days)
    find "$REPORT_DIR" -name "health_check_*.html" -mtime +30 -delete
    
else
    echo "ERROR: Health check failed"
    mail -s "Database Health Check Failed" admin@company.com < /dev/null
fi
```

### **Performance Monitoring Dashboard**
```bash
#!/bin/bash
# performance_dashboard.sh - Real-time performance monitoring

DB_FILE="$1"
WEB_PORT="${2:-8080}"

if [ -z "$DB_FILE" ]; then
    echo "Usage: $0 <database_file> [web_port]"
    exit 1
fi

echo "Starting performance monitoring dashboard"
echo "Database: $DB_FILE"
echo "Web interface: http://localhost:$WEB_PORT"
echo "Press Ctrl+C to stop monitoring"

# Start web-based monitoring interface
sb_gstat -monitor \
    -web-interface "$WEB_PORT" \
    -interval 10 \
    -alert "cache_hit_ratio<90,page_reads>5000,active_connections>200" \
    -samples unlimited \
    -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    "$DB_FILE" &

MONITOR_PID=$!

# Create real-time console display
while true; do
    clear
    echo "=== ScratchBird Performance Dashboard ==="
    echo "Database: $DB_FILE"
    echo "Time: $(date)"
    echo "Web interface: http://localhost:$WEB_PORT"
    echo ""
    
    # Current statistics
    sb_gstat -brief -cache -performance "$DB_FILE"
    
    echo ""
    echo "=== Recent Activity ==="
    
    # Monitor recent activity
    sb_gstat -monitor -samples 1 -verbose "$DB_FILE"
    
    echo ""
    echo "Press Ctrl+C to stop monitoring"
    
    sleep 30
done

# Cleanup on exit
trap "kill $MONITOR_PID 2>/dev/null" EXIT
```

### **Optimization Analysis Script**
```bash
#!/bin/bash
# optimization_analysis.sh - Database optimization recommendations

DB_FILE="$1"
OUTPUT_DIR="/optimization/analysis"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

if [ -z "$DB_FILE" ]; then
    echo "Usage: $0 <database_file>"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "=== DATABASE OPTIMIZATION ANALYSIS ==="
echo "Database: $DB_FILE"
echo "Analysis started at: $(date)"
echo ""

# Comprehensive analysis
echo "Step 1: Comprehensive database analysis..."
sb_gstat -analyze \
    -comprehensive \
    -recommendations \
    -performance \
    -bottlenecks \
    -format html \
    -output "$OUTPUT_DIR/analysis_$TIMESTAMP.html" \
    -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    "$DB_FILE"

# Cache analysis
echo "Step 2: Cache performance analysis..."
sb_gstat -cache \
    -analysis \
    -recommendations \
    -target-ratio 95 \
    -format json \
    -output "$OUTPUT_DIR/cache_analysis_$TIMESTAMP.json" \
    "$DB_FILE"

# Table-specific analysis
echo "Step 3: Table-specific analysis..."
TABLES=$(sb_isql -execute "SELECT RDB\$RELATION_NAME FROM RDB\$RELATIONS WHERE RDB\$SYSTEM_FLAG = 0" "$DB_FILE" | grep -v "RDB\$" | tail -n +3)

for table in $TABLES; do
    if [ -n "$table" ]; then
        echo "Analyzing table: $table"
        sb_gstat -table "$table" \
            -analyze \
            -recommendations \
            -format text \
            -output "$OUTPUT_DIR/table_${table}_$TIMESTAMP.txt" \
            "$DB_FILE"
    fi
done

# Index analysis
echo "Step 4: Index analysis..."
sb_gstat -index \
    -analysis \
    -selectivity \
    -usage-statistics \
    -recommendations \
    -format html \
    -output "$OUTPUT_DIR/index_analysis_$TIMESTAMP.html" \
    "$DB_FILE"

# Generate optimization script
echo "Step 5: Generating optimization script..."
sb_gstat -analyze \
    -generate-script \
    -output "$OUTPUT_DIR/optimization_script_$TIMESTAMP.sql" \
    "$DB_FILE"

# Summary report
echo "Step 6: Creating summary report..."
cat > "$OUTPUT_DIR/summary_$TIMESTAMP.txt" << EOF
=== DATABASE OPTIMIZATION ANALYSIS SUMMARY ===
Database: $DB_FILE
Analysis completed at: $(date)

Files generated:
- analysis_$TIMESTAMP.html: Comprehensive analysis report
- cache_analysis_$TIMESTAMP.json: Cache performance data
- table_*_$TIMESTAMP.txt: Individual table analyses
- index_analysis_$TIMESTAMP.html: Index performance report
- optimization_script_$TIMESTAMP.sql: Recommended optimizations

Next steps:
1. Review the comprehensive analysis report
2. Examine individual table recommendations
3. Consider implementing suggested optimizations
4. Monitor performance after changes

EOF

echo ""
echo "=== OPTIMIZATION ANALYSIS COMPLETED ==="
echo "Results saved to: $OUTPUT_DIR"
echo "Summary report: $OUTPUT_DIR/summary_$TIMESTAMP.txt"
echo ""
echo "Recommended actions:"
echo "1. Review: $OUTPUT_DIR/analysis_$TIMESTAMP.html"
echo "2. Execute: $OUTPUT_DIR/optimization_script_$TIMESTAMP.sql"
echo "3. Monitor: Run this analysis again after optimization"
```

### **Automated Performance Alerting**
```bash
#!/bin/bash
# performance_alerting.sh - Automated performance monitoring with alerts

DB_FILE="$1"
ALERT_EMAIL="admin@company.com"
LOG_FILE="/var/log/database_alerts.log"
PID_FILE="/var/run/database_monitor.pid"

# Configuration
CACHE_THRESHOLD=85
CONNECTION_THRESHOLD=150
PAGE_READ_THRESHOLD=10000
MONITOR_INTERVAL=60

# Check if already running
if [ -f "$PID_FILE" ]; then
    if kill -0 $(cat "$PID_FILE") 2>/dev/null; then
        echo "Performance monitoring already running (PID: $(cat $PID_FILE))"
        exit 1
    else
        rm -f "$PID_FILE"
    fi
fi

# Start monitoring
echo $$ > "$PID_FILE"

echo "Starting performance monitoring for $DB_FILE" >> "$LOG_FILE"
echo "Alert thresholds: Cache < $CACHE_THRESHOLD%, Connections > $CONNECTION_THRESHOLD, Page reads > $PAGE_READ_THRESHOLD" >> "$LOG_FILE"

while true; do
    # Collect current metrics
    METRICS=$(sb_gstat -monitor -samples 1 -format json "$DB_FILE")
    
    if [ $? -eq 0 ]; then
        # Parse metrics (simplified - in practice use jq or similar)
        CACHE_HIT_RATIO=$(echo "$METRICS" | grep -o '"cache_hit_ratio":[0-9]*' | cut -d: -f2)
        ACTIVE_CONNECTIONS=$(echo "$METRICS" | grep -o '"active_connections":[0-9]*' | cut -d: -f2)
        PAGE_READS=$(echo "$METRICS" | grep -o '"page_reads":[0-9]*' | cut -d: -f2)
        
        ALERT_SENT=false
        ALERT_MESSAGE=""
        
        # Check cache hit ratio
        if [ "$CACHE_HIT_RATIO" -lt "$CACHE_THRESHOLD" ]; then
            ALERT_MESSAGE="$ALERT_MESSAGE\nLow cache hit ratio: $CACHE_HIT_RATIO% (threshold: $CACHE_THRESHOLD%)"
            ALERT_SENT=true
        fi
        
        # Check active connections
        if [ "$ACTIVE_CONNECTIONS" -gt "$CONNECTION_THRESHOLD" ]; then
            ALERT_MESSAGE="$ALERT_MESSAGE\nHigh connection count: $ACTIVE_CONNECTIONS (threshold: $CONNECTION_THRESHOLD)"
            ALERT_SENT=true
        fi
        
        # Check page reads
        if [ "$PAGE_READS" -gt "$PAGE_READ_THRESHOLD" ]; then
            ALERT_MESSAGE="$ALERT_MESSAGE\nHigh page read activity: $PAGE_READS (threshold: $PAGE_READ_THRESHOLD)"
            ALERT_SENT=true
        fi
        
        # Send alert if thresholds exceeded
        if [ "$ALERT_SENT" = true ]; then
            TIMESTAMP=$(date)
            echo "[$TIMESTAMP] ALERT TRIGGERED: $ALERT_MESSAGE" >> "$LOG_FILE"
            
            echo -e "Database Performance Alert - $TIMESTAMP\n\nDatabase: $DB_FILE\n$ALERT_MESSAGE\n\nCurrent metrics:\n$METRICS" | \
                mail -s "Database Performance Alert" "$ALERT_EMAIL"
        fi
        
        # Log current status
        echo "$(date): Cache: $CACHE_HIT_RATIO%, Connections: $ACTIVE_CONNECTIONS, Page reads: $PAGE_READS" >> "$LOG_FILE"
        
    else
        echo "$(date): ERROR - Failed to collect database metrics" >> "$LOG_FILE"
        echo "Database monitoring error - failed to collect metrics" | \
            mail -s "Database Monitoring Error" "$ALERT_EMAIL"
    fi
    
    sleep "$MONITOR_INTERVAL"
done

# Cleanup on exit
trap "rm -f $PID_FILE" EXIT
```

---

## 🔍 Performance Interpretation

### **Understanding Cache Statistics**
```bash
# Analyze cache performance
sb_gstat -cache -verbose mydatabase.fdb
```

**Key Metrics:**
- **Cache Hit Ratio**: Should be >90% for good performance
- **Page Reads**: High values indicate cache misses
- **Page Writes**: Monitor for write-heavy workloads
- **Cache Distribution**: Balanced across data types

**Optimization Recommendations:**
- Hit ratio <85%: Increase cache size
- High page reads: Add more RAM or optimize queries
- Uneven distribution: Analyze query patterns

### **Table Statistics Analysis**
```bash
# Detailed table analysis
sb_gstat -table customers -analyze -verbose mydatabase.fdb
```

**Key Metrics:**
- **Record Count**: Total number of records
- **Average Record Length**: Storage efficiency
- **Fragmentation**: Space utilization
- **Index Selectivity**: Query optimization potential

### **Index Performance**
```bash
# Index usage analysis
sb_gstat -index -usage-statistics -recommendations mydatabase.fdb
```

**Key Metrics:**
- **Selectivity**: Lower values indicate better performance
- **Usage Count**: Frequency of index usage
- **Duplicate Ratio**: Index efficiency
- **Depth**: B-tree traversal cost

---

## 🆘 Troubleshooting

### **Common Analysis Issues**

**Issue**: "Cannot connect to database"
```bash
# Check database status
sb_gfix -info mydatabase.fdb

# Try with different authentication
sb_gstat -trusted -header mydatabase.fdb

# Check file permissions
ls -la mydatabase.fdb
```

**Issue**: "Statistics collection timeout"
```bash
# Use brief analysis for large databases
sb_gstat -brief -header mydatabase.fdb

# Analyze specific tables only
sb_gstat -table specific_table mydatabase.fdb

# Increase timeout
sb_gstat -timeout 300 -analyze mydatabase.fdb
```

**Issue**: "Web interface not accessible"
```bash
# Check if port is available
netstat -ln | grep 8080

# Start with different port
sb_gstat -monitor -web-interface 8081 mydatabase.fdb

# Check firewall settings
sudo ufw status | grep 8080
```

### **Performance Issues**

**Issue**: Analysis taking too long
```bash
# Use sampling for large databases
sb_gstat -analyze -sample-rate 10 mydatabase.fdb

# Focus on specific areas
sb_gstat -cache -performance mydatabase.fdb

# Use parallel analysis
sb_gstat -analyze -parallel -threads 4 mydatabase.fdb
```

---

## 🎯 Next Steps

- **[sb_gfix - Database Maintenance](13-sb_gfix.md)** - Learn database repair and optimization
- **[sb_gsec - Security Management](14-sb_gsec.md)** - Master database security
- **[Performance Tuning](20-performance.md)** - Advanced performance optimization
- **[Monitoring Guide](21-monitoring.md)** - Comprehensive monitoring strategies

## 📚 Related Documentation

- **[Database Engine](05-database-engine.md)** - Understanding ScratchBird architecture
- **[Best Practices](28-best-practices.md)** - Database optimization best practices
- **[Troubleshooting](25-troubleshooting.md)** - Performance troubleshooting guide

---

## 💡 Pro Tips

> **Regular Analysis**: Schedule weekly database analysis to catch issues early
> ```bash
> # Weekly analysis with email report
> 0 2 * * 1 sb_gstat -analyze -email-report admin@company.com database.fdb
> ```

> **Monitor Trends**: Track performance metrics over time for predictive maintenance
> ```bash
> # Continuous trend monitoring
> sb_gstat -trends -continuous -alert-on-degradation database.fdb
> ```

> **Automate Optimization**: Use generated optimization scripts for routine maintenance
> ```bash
> # Generate and review optimization script
> sb_gstat -analyze -generate-script monthly_optimization.sql database.fdb
> ```

**📊 Ready to optimize your database?** sb_gstat provides enterprise-grade analytics and monitoring capabilities to keep your ScratchBird database running at peak performance!