# Kubernetes Installation

**Last Updated:** 2026-01-30

---

## Overview

This guide covers deploying ScratchBird on Kubernetes clusters. ScratchBird can be deployed as a StatefulSet with persistent storage for production workloads.

---

## Prerequisites

### Cluster Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| Kubernetes | 1.25+ | 1.28+ |
| Nodes | 1 | 3+ |
| Node Memory | 4 GB | 8+ GB |
| Node CPU | 2 cores | 4+ cores |

### Required Tools

- `kubectl` - Kubernetes CLI
- `helm` (optional) - Package manager for Kubernetes

### Storage Requirements

ScratchBird requires persistent storage. Ensure your cluster has:
- A default StorageClass, or
- A specific StorageClass for database workloads

Check available storage classes:
```bash
kubectl get storageclass
```

---

## Method 1: Helm Chart (Recommended)

### Add Helm Repository

```bash
helm repo add scratchbird https://charts.scratchbird.dev
helm repo update
```

### Install with Default Values

```bash
helm install scratchbird scratchbird/scratchbird \
    --namespace scratchbird \
    --create-namespace
```

### Install with Custom Values

Create `values.yaml`:

```yaml
# ScratchBird Helm Chart Values

replicaCount: 1

image:
  repository: scratchbird/scratchbird
  tag: "latest"
  pullPolicy: IfNotPresent

# Service configuration
service:
  type: ClusterIP
  ports:
    native: 3092
    postgresql: 5432
    mysql: 3306
    firebird: 3050

# Resource limits
resources:
  requests:
    memory: "512Mi"
    cpu: "500m"
  limits:
    memory: "2Gi"
    cpu: "2000m"

# Persistence configuration
persistence:
  enabled: true
  storageClass: ""  # Use default StorageClass
  accessMode: ReadWriteOnce
  size: 10Gi

# ScratchBird configuration
config:
  maxConnections: 100
  sharedBuffers: "256MB"
  workMem: "8MB"

# Authentication
auth:
  adminUser: admin
  adminPassword: ""  # Leave empty to auto-generate
  existingSecret: ""  # Use existing secret

# PostgreSQL compatibility
postgresql:
  enabled: true
  port: 5432

# MySQL compatibility
mysql:
  enabled: true
  port: 3306

# Firebird compatibility
firebird:
  enabled: false
  port: 3050

# Metrics and monitoring
metrics:
  enabled: true
  port: 9090
  serviceMonitor:
    enabled: false
    namespace: monitoring

# Pod security
podSecurityContext:
  fsGroup: 1000
  runAsUser: 1000
  runAsNonRoot: true

# Node selection
nodeSelector: {}
tolerations: []
affinity: {}
```

Install with custom values:
```bash
helm install scratchbird scratchbird/scratchbird \
    --namespace scratchbird \
    --create-namespace \
    -f values.yaml
```

### Upgrade Existing Installation

```bash
helm upgrade scratchbird scratchbird/scratchbird \
    --namespace scratchbird \
    -f values.yaml
```

### Uninstall

```bash
helm uninstall scratchbird --namespace scratchbird
```

---

## Method 2: Manual Kubernetes Manifests

### Namespace

```yaml
# namespace.yaml
apiVersion: v1
kind: Namespace
metadata:
  name: scratchbird
  labels:
    app: scratchbird
```

### ConfigMap

```yaml
# configmap.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: scratchbird-config
  namespace: scratchbird
data:
  sb_server.conf: |
    [server]
    mode = multi-database
    data_dir = /var/lib/scratchbird
    max_connections = 100
    worker_threads = 0
    shutdown_timeout = 30

    [network]
    bind_address = 0.0.0.0
    native_port = 3092
    pg_port = 5432
    mysql_port = 3306
    fb_port = 3050

    [ssl]
    enabled = false

    [memory]
    buffer_pool_size = 256MB
    work_mem = 8MB

    [logging]
    level = info
    destination = stderr

    [statistics]
    enabled = true
    export = prometheus
    prometheus_port = 9090
```

### Secret

```yaml
# secret.yaml
apiVersion: v1
kind: Secret
metadata:
  name: scratchbird-credentials
  namespace: scratchbird
type: Opaque
stringData:
  admin-password: "your-secure-password-here"
```

### PersistentVolumeClaim

```yaml
# pvc.yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: scratchbird-data
  namespace: scratchbird
spec:
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: 10Gi
  # Optionally specify StorageClass
  # storageClassName: fast-ssd
```

### StatefulSet

```yaml
# statefulset.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: scratchbird
  namespace: scratchbird
spec:
  serviceName: scratchbird
  replicas: 1
  selector:
    matchLabels:
      app: scratchbird
  template:
    metadata:
      labels:
        app: scratchbird
    spec:
      securityContext:
        fsGroup: 1000
        runAsUser: 1000
        runAsNonRoot: true
      containers:
        - name: scratchbird
          image: scratchbird/scratchbird:latest
          imagePullPolicy: IfNotPresent
          ports:
            - name: native
              containerPort: 3092
              protocol: TCP
            - name: postgresql
              containerPort: 5432
              protocol: TCP
            - name: mysql
              containerPort: 3306
              protocol: TCP
            - name: firebird
              containerPort: 3050
              protocol: TCP
            - name: metrics
              containerPort: 9090
              protocol: TCP
          env:
            - name: SCRATCHBIRD_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: scratchbird-credentials
                  key: admin-password
          volumeMounts:
            - name: data
              mountPath: /var/lib/scratchbird
            - name: config
              mountPath: /etc/scratchbird
              readOnly: true
          resources:
            requests:
              memory: "512Mi"
              cpu: "500m"
            limits:
              memory: "2Gi"
              cpu: "2000m"
          livenessProbe:
            exec:
              command:
                - /bin/sh
                - -c
                - sb_isql -c "SELECT 1" -H localhost -p 3092 -U admin
            initialDelaySeconds: 30
            periodSeconds: 10
            timeoutSeconds: 5
            failureThreshold: 3
          readinessProbe:
            exec:
              command:
                - /bin/sh
                - -c
                - sb_isql -c "SELECT 1" -H localhost -p 3092 -U admin
            initialDelaySeconds: 10
            periodSeconds: 5
            timeoutSeconds: 3
            failureThreshold: 3
      volumes:
        - name: config
          configMap:
            name: scratchbird-config
  volumeClaimTemplates:
    - metadata:
        name: data
      spec:
        accessModes:
          - ReadWriteOnce
        resources:
          requests:
            storage: 10Gi
```

### Service

```yaml
# service.yaml
apiVersion: v1
kind: Service
metadata:
  name: scratchbird
  namespace: scratchbird
  labels:
    app: scratchbird
spec:
  type: ClusterIP
  ports:
    - name: native
      port: 3092
      targetPort: 3092
      protocol: TCP
    - name: postgresql
      port: 5432
      targetPort: 5432
      protocol: TCP
    - name: mysql
      port: 3306
      targetPort: 3306
      protocol: TCP
    - name: firebird
      port: 3050
      targetPort: 3050
      protocol: TCP
    - name: metrics
      port: 9090
      targetPort: 9090
      protocol: TCP
  selector:
    app: scratchbird
```

### Headless Service (for StatefulSet DNS)

```yaml
# service-headless.yaml
apiVersion: v1
kind: Service
metadata:
  name: scratchbird-headless
  namespace: scratchbird
  labels:
    app: scratchbird
spec:
  type: ClusterIP
  clusterIP: None
  ports:
    - name: native
      port: 3092
      targetPort: 3092
  selector:
    app: scratchbird
```

### Apply All Manifests

```bash
# Create namespace
kubectl apply -f namespace.yaml

# Create configurations
kubectl apply -f configmap.yaml
kubectl apply -f secret.yaml

# Create storage
kubectl apply -f pvc.yaml

# Create services
kubectl apply -f service.yaml
kubectl apply -f service-headless.yaml

# Create StatefulSet
kubectl apply -f statefulset.yaml
```

Or apply all at once:
```bash
kubectl apply -f ./kubernetes/
```

---

## Exposing ScratchBird

### Option 1: LoadBalancer Service

For cloud providers with LoadBalancer support:

```yaml
# service-lb.yaml
apiVersion: v1
kind: Service
metadata:
  name: scratchbird-lb
  namespace: scratchbird
  annotations:
    # AWS-specific annotations
    service.beta.kubernetes.io/aws-load-balancer-type: nlb
    service.beta.kubernetes.io/aws-load-balancer-scheme: internet-facing
spec:
  type: LoadBalancer
  ports:
    - name: postgresql
      port: 5432
      targetPort: 5432
  selector:
    app: scratchbird
```

### Option 2: NodePort

For on-premises or development:

```yaml
# service-nodeport.yaml
apiVersion: v1
kind: Service
metadata:
  name: scratchbird-nodeport
  namespace: scratchbird
spec:
  type: NodePort
  ports:
    - name: postgresql
      port: 5432
      targetPort: 5432
      nodePort: 30432
  selector:
    app: scratchbird
```

### Option 3: Ingress (TCP)

Using NGINX Ingress Controller with TCP support:

```yaml
# tcp-services-configmap.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: tcp-services
  namespace: ingress-nginx
data:
  "5432": "scratchbird/scratchbird:5432"
  "3306": "scratchbird/scratchbird:3306"
```

---

## Connecting to ScratchBird

### From Within the Cluster

```bash
# Using service DNS
psql -h scratchbird.scratchbird.svc.cluster.local -p 5432 -U admin -d scratchbird

# From a pod in the same namespace
psql -h scratchbird -p 5432 -U admin -d scratchbird
```

### Port Forwarding (Development)

```bash
# Forward PostgreSQL port
kubectl port-forward -n scratchbird svc/scratchbird 5432:5432

# In another terminal
psql -h localhost -p 5432 -U admin -d scratchbird
```

### From Outside the Cluster

Using LoadBalancer:
```bash
# Get external IP
kubectl get svc -n scratchbird scratchbird-lb

# Connect
psql -h <EXTERNAL-IP> -p 5432 -U admin -d scratchbird
```

---

## Monitoring

### Prometheus ServiceMonitor

```yaml
# servicemonitor.yaml
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: scratchbird
  namespace: scratchbird
  labels:
    app: scratchbird
spec:
  selector:
    matchLabels:
      app: scratchbird
  endpoints:
    - port: metrics
      interval: 30s
      path: /metrics
```

### Prometheus Scrape Config

If not using ServiceMonitor:

```yaml
# Add to prometheus.yml
scrape_configs:
  - job_name: 'scratchbird'
    kubernetes_sd_configs:
      - role: pod
        namespaces:
          names:
            - scratchbird
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_label_app]
        action: keep
        regex: scratchbird
      - source_labels: [__meta_kubernetes_pod_container_port_name]
        action: keep
        regex: metrics
```

---

## Backup and Restore

### CronJob for Automated Backups

```yaml
# backup-cronjob.yaml
apiVersion: batch/v1
kind: CronJob
metadata:
  name: scratchbird-backup
  namespace: scratchbird
spec:
  schedule: "0 2 * * *"  # Daily at 2 AM
  jobTemplate:
    spec:
      template:
        spec:
          containers:
            - name: backup
              image: scratchbird/scratchbird:latest
              command:
                - /bin/sh
                - -c
                - |
                  DATE=$(date +%Y%m%d_%H%M%S)
                  sb_backup -H scratchbird -p 3092 -U admin \
                    -o /backup/scratchbird_${DATE}.sbk
              env:
                - name: SCRATCHBIRD_PASSWORD
                  valueFrom:
                    secretKeyRef:
                      name: scratchbird-credentials
                      key: admin-password
              volumeMounts:
                - name: backup
                  mountPath: /backup
          restartPolicy: OnFailure
          volumes:
            - name: backup
              persistentVolumeClaim:
                claimName: scratchbird-backup
```

### Manual Backup

```bash
# Create backup pod
kubectl run backup --rm -it --restart=Never \
    -n scratchbird \
    --image=scratchbird/scratchbird:latest \
    -- sb_backup -H scratchbird -p 3092 -U admin -o /tmp/backup.sbk

# Copy backup locally
kubectl cp scratchbird/backup:/tmp/backup.sbk ./backup.sbk
```

---

## Scaling and High Availability

### Read Replicas (Future)

ScratchBird read replicas are planned for Beta. The StatefulSet can be configured with multiple replicas:

```yaml
spec:
  replicas: 3  # 1 primary + 2 replicas
```

### Pod Disruption Budget

```yaml
# pdb.yaml
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: scratchbird-pdb
  namespace: scratchbird
spec:
  minAvailable: 1
  selector:
    matchLabels:
      app: scratchbird
```

---

## Troubleshooting

### Check Pod Status

```bash
# List pods
kubectl get pods -n scratchbird

# Describe pod
kubectl describe pod -n scratchbird scratchbird-0

# View logs
kubectl logs -n scratchbird scratchbird-0

# Follow logs
kubectl logs -n scratchbird scratchbird-0 -f
```

### Check PersistentVolume

```bash
# List PVCs
kubectl get pvc -n scratchbird

# Describe PVC
kubectl describe pvc -n scratchbird scratchbird-data
```

### Connect to Pod

```bash
# Shell into pod
kubectl exec -it -n scratchbird scratchbird-0 -- /bin/sh

# Run SQL command
kubectl exec -it -n scratchbird scratchbird-0 -- \
    sb_isql -H localhost -p 3092 -U admin -c "SELECT 1"
```

### Common Issues

**Pod stuck in Pending:**
```bash
# Check events
kubectl get events -n scratchbird --sort-by=.metadata.creationTimestamp

# Usually caused by:
# - Insufficient resources
# - No available PersistentVolume
# - Node selector/affinity issues
```

**Pod in CrashLoopBackOff:**
```bash
# Check logs
kubectl logs -n scratchbird scratchbird-0 --previous

# Usually caused by:
# - Configuration errors
# - Permission issues on volume
# - Port conflicts
```

---

## Cleanup

### Delete All Resources

```bash
# Delete StatefulSet (preserves PVC)
kubectl delete statefulset -n scratchbird scratchbird

# Delete services
kubectl delete svc -n scratchbird --all

# Delete ConfigMap and Secret
kubectl delete configmap -n scratchbird scratchbird-config
kubectl delete secret -n scratchbird scratchbird-credentials

# Delete PVC (CAUTION: deletes data)
kubectl delete pvc -n scratchbird --all

# Delete namespace
kubectl delete namespace scratchbird
```

### Using Helm

```bash
helm uninstall scratchbird -n scratchbird
kubectl delete pvc -n scratchbird --all  # If needed
kubectl delete namespace scratchbird
```

---

## Next Steps

- [First Connection](../getting-started/first-connection.md) - Connect and run your first query
- [Basic SQL](../getting-started/basic-sql.md) - Learn ScratchBird SQL basics
- [Backup and Restore](../admin/backup-restore.md) - Backup procedures

