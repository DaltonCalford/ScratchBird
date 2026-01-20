# Deployment Guide

## Overview

This guide covers deployment strategies for the distributed database system across different environments: cloud (AWS, GCP, Azure), self-hosted, and hybrid deployments. It includes infrastructure requirements, configuration examples, and operational procedures.

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

## Deployment Models

### Cloud Deployment

**Characteristics**:
- Elastic scalability
- Managed services integration
- Pay-as-you-go pricing
- Software-based clock synchronization
- Multi-region capability

**Best for**:
- Variable workloads
- Rapid scaling requirements
- Minimal operational overhead
- Multi-region presence

### Self-Hosted Deployment

**Characteristics**:
- Full infrastructure control
- Hardware clock synchronization option
- Dedicated networking
- Fixed capacity planning
- Lower long-term costs at scale

**Best for**:
- Data sovereignty requirements
- Consistent high-volume workloads
- Strict latency requirements
- Regulatory compliance

### Hybrid Deployment

**Characteristics**:
- TX engines in cloud (elastic)
- Ingestion/OLAP on-premises (data control)
- Cross-environment replication
- Balanced cost/control trade-offs

**Best for**:
- Gradual cloud migration
- Data residency with cloud burst
- Cost optimization
- Risk mitigation

## AWS Deployment

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│  VPC: db-cluster-vpc (10.0.0.0/16)                      │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Public Subnet (10.0.1.0/24)                    │   │
│  │  ├─ Application Load Balancer (PostgreSQL)      │   │
│  │  ├─ Application Load Balancer (MySQL)           │   │
│  │  └─ Bastion Host                                │   │
│  └─────────────────────────────────────────────────┘   │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Private Subnet 1 (10.0.10.0/24) - us-east-1a  │   │
│  │  ├─ TX Engine Auto Scaling Group                │   │
│  │  ├─ Protocol Handlers (EC2)                     │   │
│  │  └─ Clock Master (EC2)                          │   │
│  └─────────────────────────────────────────────────┘   │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Private Subnet 2 (10.0.20.0/24) - us-east-1b  │   │
│  │  ├─ Ingestion Engines (EC2)                     │   │
│  │  ├─ OLAP Shards (EC2)                           │   │
│  │  └─ Clock Master Backup (EC2)                   │   │
│  └─────────────────────────────────────────────────┘   │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Data Services                                   │   │
│  │  ├─ Amazon MSK (Kafka)                          │   │
│  │  ├─ ElastiCache (Redis) - Result Cache         │   │
│  │  └─ S3 - OLAP Cold Storage                      │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Infrastructure as Code (Terraform)

```hcl
# main.tf

provider "aws" {
  region = var.aws_region
}

# VPC
module "vpc" {
  source = "terraform-aws-modules/vpc/aws"
  
  name = "db-cluster-vpc"
  cidr = "10.0.0.0/16"
  
  azs             = ["${var.aws_region}a", "${var.aws_region}b"]
  private_subnets = ["10.0.10.0/24", "10.0.20.0/24"]
  public_subnets  = ["10.0.1.0/24", "10.0.2.0/24"]
  
  enable_nat_gateway = true
  enable_vpn_gateway = false
  
  tags = {
    Environment = var.environment
  }
}

# Security Groups
resource "aws_security_group" "tx_engine" {
  name        = "tx-engine-sg"
  vpc_id      = module.vpc.vpc_id
  
  # PostgreSQL
  ingress {
    from_port   = 5432
    to_port     = 5432
    protocol    = "tcp"
    cidr_blocks = ["10.0.0.0/16"]
  }
  
  # MySQL
  ingress {
    from_port   = 3306
    to_port     = 3306
    protocol    = "tcp"
    cidr_blocks = ["10.0.0.0/16"]
  }
  
  # Clock heartbeat
  ingress {
    from_port   = 8888
    to_port     = 8888
    protocol    = "udp"
    cidr_blocks = ["10.0.0.0/16"]
  }
  
  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

# TX Engine Auto Scaling Group
resource "aws_launch_template" "tx_engine" {
  name_prefix   = "tx-engine-"
  image_id      = data.aws_ami.ubuntu.id
  instance_type = var.tx_engine_instance_type
  
  vpc_security_group_ids = [aws_security_group.tx_engine.id]
  
  user_data = base64encode(templatefile("${path.module}/user_data_tx_engine.sh", {
    cluster_name = var.cluster_name
    s3_bucket    = aws_s3_bucket.config.bucket
  }))
  
  block_device_mappings {
    device_name = "/dev/sda1"
    
    ebs {
      volume_size = 100
      volume_type = "gp3"
      iops        = 3000
      throughput  = 125
    }
  }
  
  tag_specifications {
    resource_type = "instance"
    
    tags = {
      Name = "tx-engine"
      Tier = "transaction"
    }
  }
}

resource "aws_autoscaling_group" "tx_engine" {
  name                = "tx-engine-asg"
  vpc_zone_identifier = module.vpc.private_subnets
  
  min_size         = var.tx_engine_min_count
  max_size         = var.tx_engine_max_count
  desired_capacity = var.tx_engine_desired_count
  
  launch_template {
    id      = aws_launch_template.tx_engine.id
    version = "$Latest"
  }
  
  target_group_arns = [
    aws_lb_target_group.postgresql.arn,
    aws_lb_target_group.mysql.arn
  ]
  
  health_check_type         = "ELB"
  health_check_grace_period = 300
  
  tag {
    key                 = "Name"
    value               = "tx-engine"
    propagate_at_launch = true
  }
}

# Auto Scaling Policies
resource "aws_autoscaling_policy" "tx_engine_scale_up" {
  name                   = "tx-engine-scale-up"
  autoscaling_group_name = aws_autoscaling_group.tx_engine.name
  adjustment_type        = "ChangeInCapacity"
  scaling_adjustment     = 2
  cooldown               = 300
}

resource "aws_cloudwatch_metric_alarm" "tx_engine_cpu_high" {
  alarm_name          = "tx-engine-cpu-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 2
  metric_name         = "CPUUtilization"
  namespace           = "AWS/EC2"
  period              = 60
  statistic           = "Average"
  threshold           = 75
  
  dimensions = {
    AutoScalingGroupName = aws_autoscaling_group.tx_engine.name
  }
  
  alarm_actions = [aws_autoscaling_policy.tx_engine_scale_up.arn]
}

# MSK (Kafka) Cluster
resource "aws_msk_cluster" "replication" {
  cluster_name           = "db-replication-cluster"
  kafka_version          = "3.5.1"
  number_of_broker_nodes = 3
  
  broker_node_group_info {
    instance_type   = "kafka.m5.large"
    client_subnets  = module.vpc.private_subnets
    security_groups = [aws_security_group.msk.id]
    
    storage_info {
      ebs_storage_info {
        volume_size = 1000
      }
    }
  }
  
  encryption_info {
    encryption_in_transit {
      client_broker = "TLS"
      in_cluster    = true
    }
  }
}

# S3 Bucket for OLAP Cold Storage
resource "aws_s3_bucket" "olap_cold_storage" {
  bucket = "${var.cluster_name}-olap-cold-storage"
  
  tags = {
    Name = "OLAP Cold Storage"
  }
}

resource "aws_s3_bucket_lifecycle_configuration" "olap_cold_storage" {
  bucket = aws_s3_bucket.olap_cold_storage.id
  
  rule {
    id     = "archive-old-data"
    status = "Enabled"
    
    transition {
      days          = 90
      storage_class = "GLACIER"
    }
    
    expiration {
      days = 2555  # 7 years
    }
  }
}
```

### AWS Time Sync Configuration

```bash
#!/bin/bash
# Configure AWS Time Sync Service

# Install chrony
apt-get update
apt-get install -y chrony

# Configure chrony for AWS Time Sync
cat > /etc/chrony/chrony.conf <<EOF
# AWS Time Sync Service
server 169.254.169.123 prefer iburst minpoll 4 maxpoll 4

# Drift file
driftfile /var/lib/chrony/drift

# Allow clock to be stepped in first three updates
makestep 0.1 3

# Enable kernel synchronization
rtcsync

# Logging
logdir /var/log/chrony
log measurements statistics tracking
EOF

# Restart chrony
systemctl restart chrony

# Verify synchronization
sleep 5
chronyc sources
chronyc tracking
```

### Instance Sizing

```yaml
# Production sizing recommendations

Transaction Engines:
  Instance: c6i.2xlarge (8 vCPU, 16 GB RAM)
  Count: 4-20 (auto-scaling)
  Storage: 100 GB GP3 SSD
  Network: Up to 12.5 Gbps
  
Ingestion Engines:
  Instance: c6i.4xlarge (16 vCPU, 32 GB RAM)
  Count: 2-4 (static)
  Storage: 500 GB GP3 SSD
  Network: Up to 18.75 Gbps
  
OLAP Shards:
  Instance: r6i.4xlarge (16 vCPU, 128 GB RAM)
  Count: 4-12 (per shard)
  Storage: 2 TB GP3 SSD + S3 for cold data
  Network: Up to 18.75 Gbps
  
MSK Kafka:
  Instance: kafka.m5.large (2 vCPU, 8 GB RAM)
  Count: 3 brokers
  Storage: 1 TB per broker
```

## GCP Deployment

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│  VPC: db-cluster-network                                │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Region: us-central1                            │   │
│  │                                                  │   │
│  │  ┌──────────────────────────────────────────┐  │   │
│  │  │  Zone: us-central1-a                     │  │   │
│  │  │  ├─ TX Engine Managed Instance Group    │  │   │
│  │  │  ├─ Cloud Load Balancer (PostgreSQL)    │  │   │
│  │  │  └─ Clock Master (Compute Engine)       │  │   │
│  │  └──────────────────────────────────────────┘  │   │
│  │                                                  │   │
│  │  ┌──────────────────────────────────────────┐  │   │
│  │  │  Zone: us-central1-b                     │  │   │
│  │  │  ├─ Ingestion Engines (Compute Engine)  │  │   │
│  │  │  ├─ OLAP Shards (Compute Engine)        │  │   │
│  │  │  └─ Clock Master Backup                 │  │   │
│  │  └──────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────┘   │
│                                                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Managed Services                                │   │
│  │  ├─ Pub/Sub (Replication Stream)               │   │
│  │  ├─ Memorystore (Redis) - Cache                │   │
│  │  └─ Cloud Storage - OLAP Cold Storage          │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Deployment Manager Template

```yaml
# gcp-deployment.yaml

resources:
# VPC Network
- name: db-cluster-network
  type: compute.v1.network
  properties:
    autoCreateSubnetworks: false

# Subnet
- name: db-cluster-subnet
  type: compute.v1.subnetwork
  properties:
    network: $(ref.db-cluster-network.selfLink)
    ipCidrRange: 10.128.0.0/20
    region: us-central1

# Firewall Rules
- name: allow-db-protocols
  type: compute.v1.firewall
  properties:
    network: $(ref.db-cluster-network.selfLink)
    allowed:
    - IPProtocol: TCP
      ports: ["5432", "3306", "1433", "3050"]
    - IPProtocol: UDP
      ports: ["8888"]
    sourceRanges: ["10.128.0.0/20"]

# TX Engine Instance Template
- name: tx-engine-template
  type: compute.v1.instanceTemplate
  properties:
    properties:
      machineType: n2-standard-8
      disks:
      - boot: true
        autoDelete: true
        initializeParams:
          sourceImage: projects/ubuntu-os-cloud/global/images/family/ubuntu-2204-lts
          diskSizeGb: 100
          diskType: pd-ssd
      networkInterfaces:
      - network: $(ref.db-cluster-network.selfLink)
        subnetwork: $(ref.db-cluster-subnet.selfLink)
      metadata:
        items:
        - key: startup-script
          value: |
            #!/bin/bash
            # Install and configure TX engine
            # ... (see startup script below)

# TX Engine Managed Instance Group
- name: tx-engine-mig
  type: compute.v1.instanceGroupManager
  properties:
    baseInstanceName: tx-engine
    instanceTemplate: $(ref.tx-engine-template.selfLink)
    targetSize: 4
    zone: us-central1-a
    autoHealingPolicies:
    - healthCheck: $(ref.tx-engine-health-check.selfLink)
      initialDelaySec: 300

# Auto Scaler
- name: tx-engine-autoscaler
  type: compute.v1.autoscaler
  properties:
    zone: us-central1-a
    target: $(ref.tx-engine-mig.selfLink)
    autoscalingPolicy:
      minNumReplicas: 4
      maxNumReplicas: 20
      cpuUtilization:
        utilizationTarget: 0.75
      coolDownPeriodSec: 300

# Cloud Pub/Sub Topic for Replication
- name: replication-topic
  type: pubsub.v1.topic
  properties:
    topic: db-replication-events

# Cloud Storage Bucket for OLAP
- name: olap-cold-storage
  type: storage.v1.bucket
  properties:
    location: us-central1
    storageClass: STANDARD
    lifecycle:
      rule:
      - action:
          type: SetStorageClass
          storageClass: NEARLINE
        condition:
          age: 90
      - action:
          type: Delete
        condition:
          age: 2555
```

### GCP Time Sync Configuration

```bash
#!/bin/bash
# Configure GCP NTP

# Install chrony
apt-get update
apt-get install -y chrony

# Configure chrony for GCP
cat > /etc/chrony/chrony.conf <<EOF
# GCP metadata server time
server metadata.google.internal prefer iburst minpoll 4 maxpoll 4

# Drift file
driftfile /var/lib/chrony/drift

# Allow clock to be stepped
makestep 0.1 3

# Enable kernel synchronization
rtcsync

# Logging
logdir /var/log/chrony
log measurements statistics tracking
EOF

# Restart chrony
systemctl restart chrony

# Verify
chronyc sources
chronyc tracking
```

## Self-Hosted Deployment

### Hardware Requirements

```yaml
# Production Hardware Specifications

Clock Master Servers (2 for HA):
  CPU: 4 cores, 3.0+ GHz
  RAM: 8 GB
  Storage: 50 GB SSD (OS)
  Network: 10 Gbps
  Special: GPS receiver (optional, for hardware sync)
  
Transaction Engine Servers:
  CPU: 16 cores, 3.0+ GHz (or 32 vCPU)
  RAM: 32-64 GB
  Storage: 500 GB NVMe SSD
  Network: 10-25 Gbps
  Count: 4-20 servers
  
Ingestion Engine Servers:
  CPU: 32 cores, 2.5+ GHz
  RAM: 64-128 GB
  Storage: 1 TB NVMe SSD
  Network: 10-25 Gbps
  Count: 2-4 servers
  
OLAP Shard Servers:
  CPU: 32 cores, 2.5+ GHz
  RAM: 256-512 GB
  Storage: 10-20 TB SSD (or NVMe for hot, HDD for cold)
  Network: 25-40 Gbps
  Count: 4-12 servers per shard
  
Kafka Brokers:
  CPU: 8 cores, 2.5+ GHz
  RAM: 32 GB
  Storage: 2-5 TB SSD
  Network: 10 Gbps
  Count: 3-5 brokers
  
Network Infrastructure:
  Switch: 10/25/40 Gbps, low latency
  Dedicated VLAN for clock sync (recommended)
  Redundant paths
```

### Ansible Deployment

```yaml
# ansible/playbook.yml

---
- name: Deploy Database Cluster
  hosts: all
  become: yes
  
  vars:
    cluster_name: "production-cluster"
    clock_master_ip: "10.0.1.10"
    kafka_brokers: ["10.0.2.10:9092", "10.0.2.11:9092", "10.0.2.12:9092"]
  
  tasks:
  - name: Update system packages
    apt:
      update_cache: yes
      upgrade: dist
  
  - name: Install base packages
    apt:
      name:
        - chrony
        - python3
        - python3-pip
        - build-essential
        - git
      state: present
  
  - name: Configure clock synchronization
    template:
      src: templates/chrony.conf.j2
      dest: /etc/chrony/chrony.conf
    notify: restart chrony
  
  - name: Deploy cluster software
    include_tasks: "{{ role }}.yml"
    vars:
      role: "{{ inventory_hostname | regex_replace('^([^-]+).*', '\\1') }}"

- name: Configure TX Engines
  hosts: tx_engines
  become: yes
  
  tasks:
  - name: Create application user
    user:
      name: dbengine
      system: yes
      shell: /bin/bash
  
  - name: Deploy TX engine binary
    copy:
      src: ../bin/tx-engine
      dest: /opt/db/tx-engine
      mode: '0755'
      owner: dbengine
  
  - name: Deploy configuration
    template:
      src: templates/tx-engine.yaml.j2
      dest: /etc/db/tx-engine.yaml
      owner: dbengine
  
  - name: Create systemd service
    template:
      src: templates/tx-engine.service.j2
      dest: /etc/systemd/system/tx-engine.service
    notify: restart tx-engine
  
  - name: Enable and start service
    systemd:
      name: tx-engine
      enabled: yes
      state: started

- name: Configure Ingestion Engines
  hosts: ingestion_engines
  become: yes
  
  tasks:
  - name: Deploy ingestion engine binary
    copy:
      src: ../bin/ingestion-engine
      dest: /opt/db/ingestion-engine
      mode: '0755'
      owner: dbengine
  
  - name: Deploy configuration
    template:
      src: templates/ingestion-engine.yaml.j2
      dest: /etc/db/ingestion-engine.yaml
      owner: dbengine
  
  - name: Create systemd service
    template:
      src: templates/ingestion-engine.service.j2
      dest: /etc/systemd/system/ingestion-engine.service
    notify: restart ingestion-engine

handlers:
  - name: restart chrony
    systemd:
      name: chrony
      state: restarted
  
  - name: restart tx-engine
    systemd:
      name: tx-engine
      state: restarted
  
  - name: restart ingestion-engine
    systemd:
      name: ingestion-engine
      state: restarted
```

### Hardware Clock Setup (GPS)

```bash
#!/bin/bash
# Setup GPS receiver for hardware time sync

# Install required packages
apt-get install -y gpsd gpsd-clients chrony

# Configure GPSD
cat > /etc/default/gpsd <<EOF
START_DAEMON="true"
USBAUTO="true"
DEVICES="/dev/ttyUSB0"
GPSD_OPTIONS="-n -G"
EOF

# Configure chrony with GPS
cat > /etc/chrony/chrony.conf <<EOF
# GPS receiver (local stratum 0)
refclock SHM 0 refid GPS precision 1e-1 offset 0.0 delay 0.2
refclock SHM 1 refid PPS precision 1e-9

# Allow local clients
allow 10.0.0.0/8

# Serve time to network
local stratum 1

# Other settings
driftfile /var/lib/chrony/drift
makestep 0.1 3
rtcsync
EOF

# Start services
systemctl enable gpsd
systemctl start gpsd
systemctl restart chrony

# Verify GPS lock
sleep 30
cgps -s

# Check chrony synchronization
chronyc sources
chronyc tracking
```

## Configuration Management

### Configuration File Structure

```
/etc/db/
├── cluster.yaml          # Cluster-wide settings
├── tx-engine.yaml        # TX engine configuration
├── ingestion.yaml        # Ingestion engine configuration
├── olap.yaml            # OLAP shard configuration
└── security/
    ├── ca.crt           # Certificate authority
    ├── server.crt       # Server certificate
    └── server.key       # Server private key
```

### Example: cluster.yaml

```yaml
cluster:
  name: production-cluster
  environment: production
  region: us-east-1
  
  # Clock synchronization
  clock:
    mode: software  # software | hardware
    master_host: 10.0.1.10
    backup_host: 10.0.1.11
    heartbeat_port: 8888
    heartbeat_interval_ms: 100
    max_acceptable_uncertainty_ns: 50000000  # 50ms
  
  # Replication
  replication:
    broker: kafka
    brokers:
      - kafka1.internal:9092
      - kafka2.internal:9092
      - kafka3.internal:9092
    topic_prefix: "db-changes-"
    compression: zstd
  
  # Security
  security:
    enable_tls: true
    tls_cert_path: /etc/db/security/server.crt
    tls_key_path: /etc/db/security/server.key
    tls_ca_path: /etc/db/security/ca.crt
    
    auth_method: kerberos  # kerberos | ldap | jwt
    kerberos_realm: EXAMPLE.COM
    kerberos_keytab: /etc/db/security/db.keytab
  
  # Monitoring
  monitoring:
    enable: true
    prometheus_port: 9090
    log_level: info
    log_output: /var/log/db/cluster.log
```

### Example: tx-engine.yaml

```yaml
tx_engine:
  server_id: auto  # Auto-assign from pool
  
  # Network
  listeners:
    - protocol: postgresql
      port: 5432
      max_connections: 1000
      
    - protocol: mysql
      port: 3306
      max_connections: 1000
  
  # Storage
  storage:
    data_dir: /var/lib/db/tx-engine
    retention_hours: 24
    wal_dir: /var/lib/db/tx-engine/wal  # optional post-gold WAL only
    checkpoint_interval_seconds: 300
  
  # Cache
  cache:
    query_result_cache_mb: 4096
    hot_row_cache_mb: 8192
    prepared_statement_cache_size: 10000
    
  # Consistency
  consistency:
    default_mode: bounded_staleness
    table_overrides:
      financial_transactions: strict_serializable
      sessions: eventual
  
  # Replication
  replication:
    enable: true
    buffer_size_mb: 512
    flush_interval_ms: 100
    compression: zstd
  
  # Resources
  resources:
    worker_threads: 16
    io_threads: 4
```

## Operational Procedures

### Initial Cluster Bootstrap

```bash
#!/bin/bash
# bootstrap-cluster.sh

set -e

echo "=== Bootstrapping Database Cluster ==="

# 1. Start clock masters
echo "Starting clock masters..."
ansible-playbook -i inventory.ini deploy-clock-masters.yml

# Wait for clock sync
sleep 30

# 2. Start Kafka brokers
echo "Starting Kafka brokers..."
ansible-playbook -i inventory.ini deploy-kafka.yml

# Wait for Kafka to be ready
sleep 60

# 3. Initialize database schema
echo "Initializing schema..."
./bin/db-admin --config cluster.yaml schema init

# 4. Start ingestion engines
echo "Starting ingestion engines..."
ansible-playbook -i inventory.ini deploy-ingestion-engines.yml

sleep 30

# 5. Start OLAP shards
echo "Starting OLAP shards..."
ansible-playbook -i inventory.ini deploy-olap-shards.yml

sleep 30

# 6. Start TX engines
echo "Starting TX engines..."
ansible-playbook -i inventory.ini deploy-tx-engines.yml

sleep 60

# 7. Verify cluster health
echo "Verifying cluster health..."
./bin/db-admin --config cluster.yaml cluster health

echo "=== Bootstrap Complete ==="
```

### Adding a TX Engine

```bash
#!/bin/bash
# add-tx-engine.sh

NEW_HOST=$1

if [ -z "$NEW_HOST" ]; then
  echo "Usage: $0 <hostname>"
  exit 1
fi

echo "Adding TX engine: $NEW_HOST"

# 1. Provision hardware/VM
echo "Provisioning instance..."
terraform apply -var="tx_engine_count=+1"

# 2. Deploy software
echo "Deploying software..."
ansible-playbook -i inventory.ini deploy-tx-engines.yml --limit "$NEW_HOST"

# 3. Warm up cache
echo "Warming up cache..."
./bin/db-admin --config cluster.yaml tx-engine warm-cache --host "$NEW_HOST"

# 4. Add to load balancer
echo "Adding to load balancer..."
aws elbv2 register-targets \
  --target-group-arn $TARGET_GROUP_ARN \
  --targets Id=$NEW_HOST

# 5. Verify
echo "Verifying..."
./bin/db-admin --config cluster.yaml tx-engine health --host "$NEW_HOST"

echo "TX engine added successfully"
```

### Rolling Update

```bash
#!/bin/bash
# rolling-update.sh

VERSION=$1

if [ -z "$VERSION" ]; then
  echo "Usage: $0 <version>"
  exit 1
fi

echo "Performing rolling update to version: $VERSION"

# Get list of all TX engines
TX_ENGINES=$(./bin/db-admin --config cluster.yaml tx-engine list)

for ENGINE in $TX_ENGINES; do
  echo "Updating TX engine: $ENGINE"
  
  # 1. Remove from load balancer
  echo "  Removing from load balancer..."
  aws elbv2 deregister-targets \
    --target-group-arn $TARGET_GROUP_ARN \
    --targets Id=$ENGINE
  
  # Wait for connections to drain
  sleep 60
  
  # 2. Stop service
  echo "  Stopping service..."
  ssh $ENGINE "sudo systemctl stop tx-engine"
  
  # 3. Deploy new version
  echo "  Deploying new version..."
  scp bin/tx-engine-$VERSION $ENGINE:/opt/db/tx-engine
  
  # 4. Start service
  echo "  Starting service..."
  ssh $ENGINE "sudo systemctl start tx-engine"
  
  # Wait for warmup
  sleep 30
  
  # 5. Add back to load balancer
  echo "  Adding back to load balancer..."
  aws elbv2 register-targets \
    --target-group-arn $TARGET_GROUP_ARN \
    --targets Id=$ENGINE
  
  # 6. Verify
  echo "  Verifying..."
  ./bin/db-admin --config cluster.yaml tx-engine health --host $ENGINE
  
  echo "  $ENGINE updated successfully"
  
  # Brief pause between updates
  sleep 30
done

echo "Rolling update complete"
```

### Backup and Recovery

```bash
#!/bin/bash
# backup-cluster.sh

BACKUP_DIR=/backups/$(date +%Y%m%d-%H%M%S)
mkdir -p $BACKUP_DIR

echo "Starting backup to: $BACKUP_DIR"

# 1. Backup cluster configuration
echo "Backing up configuration..."
cp -r /etc/db/ $BACKUP_DIR/config/

# 2. Backup Ingestion Engine state
echo "Backing up Ingestion state..."
./bin/db-admin --config cluster.yaml ingestion snapshot \
  --output $BACKUP_DIR/ingestion/

# 3. Backup OLAP partitions metadata
echo "Backing up OLAP metadata..."
./bin/db-admin --config cluster.yaml olap snapshot \
  --output $BACKUP_DIR/olap-metadata/

# 4. Trigger OLAP data snapshot (S3/GCS)
echo "Backing up OLAP data..."
./bin/db-admin --config cluster.yaml olap backup \
  --destination s3://backups/olap/$(date +%Y%m%d-%H%M%S)/

# 5. Backup Kafka offsets
echo "Backing up Kafka state..."
kafka-consumer-groups.sh --bootstrap-server kafka1:9092 \
  --describe --all-groups > $BACKUP_DIR/kafka-offsets.txt

# 6. Create backup manifest
cat > $BACKUP_DIR/manifest.yaml <<EOF
timestamp: $(date -u +%Y-%m-%dT%H:%M:%SZ)
cluster: $CLUSTER_NAME
version: $VERSION
components:
  - config
  - ingestion
  - olap-metadata
  - olap-data
  - kafka-offsets
EOF

echo "Backup complete: $BACKUP_DIR"

# Upload manifest to S3
aws s3 cp $BACKUP_DIR/manifest.yaml \
  s3://backups/manifests/$(basename $BACKUP_DIR).yaml
```

## Monitoring and Alerting

See **09-Monitoring-Observability.md** for comprehensive monitoring setup.

### Quick Health Check

```bash
#!/bin/bash
# health-check.sh

# Check all components
./bin/db-admin --config cluster.yaml cluster health

# Expected output:
# Clock Masters:     ✓ 2/2 healthy (uncertainty: 2ms)
# TX Engines:        ✓ 8/8 healthy (avg latency: 3ms)
# Ingestion Engines: ✓ 2/2 healthy (lag: 120ms)
# OLAP Shards:       ✓ 4/4 healthy (queries/s: 150)
# Kafka Brokers:     ✓ 3/3 healthy
# Overall:           ✓ HEALTHY
```

## Security Hardening

### Network Security

```bash
# Firewall rules (iptables)

# Allow PostgreSQL from application servers only
iptables -A INPUT -p tcp --dport 5432 -s 10.0.100.0/24 -j ACCEPT
iptables -A INPUT -p tcp --dport 5432 -j DROP

# Allow MySQL from application servers only
iptables -A INPUT -p tcp --dport 3306 -s 10.0.100.0/24 -j ACCEPT
iptables -A INPUT -p tcp --dport 3306 -j DROP

# Allow clock heartbeat within cluster
iptables -A INPUT -p udp --dport 8888 -s 10.0.0.0/16 -j ACCEPT
iptables -A INPUT -p udp --dport 8888 -j DROP

# Allow SSH from bastion only
iptables -A INPUT -p tcp --dport 22 -s 10.0.1.100 -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j DROP
```

### TLS Configuration

```yaml
# tls-config.yaml

tls:
  # Minimum TLS version
  min_version: "1.3"
  
  # Cipher suites (TLS 1.3)
  cipher_suites:
    - TLS_AES_256_GCM_SHA384
    - TLS_CHACHA20_POLY1305_SHA256
    - TLS_AES_128_GCM_SHA256
  
  # Certificate rotation
  cert_rotation_days: 90
  
  # Client authentication
  require_client_cert: true
  client_ca_path: /etc/db/security/client-ca.crt
```

## Cost Optimization

### Cloud Cost Estimates (AWS)

```
Monthly Cost Estimate (Production):

TX Engines:
  8x c6i.2xlarge (on-demand): $1,100
  Auto-scaling to 20: max $2,750/month
  
Ingestion Engines:
  2x c6i.4xlarge: $900
  
OLAP Shards:
  4x r6i.4xlarge: $2,400
  
MSK Kafka:
  3x kafka.m5.large: $900
  
ElastiCache Redis:
  2x cache.r6g.large: $500
  
S3 Storage:
  10 TB: $230
  
Data Transfer:
  1 TB/month: $90
  
Total: ~$6,000 - $7,000/month

Cost Optimization Tips:
- Use Reserved Instances for stable components (30-50% savings)
- Use Spot Instances for TX engines (up to 70% savings)
- Enable S3 Intelligent Tiering
- Right-size instances based on actual metrics
```

This deployment guide provides a comprehensive foundation for deploying the distributed database system across different environments. Adjust configurations based on specific requirements and scale.
