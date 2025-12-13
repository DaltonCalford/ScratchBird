# SSL/TLS Setup

Configure encrypted connections for ScratchBird.

[Back to Configuration Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

SSL/TLS encrypts data between clients and the server, protecting against:
- Eavesdropping
- Man-in-the-middle attacks
- Credential theft

---

## Quick Setup

### 1. Generate Certificates

```bash
# Create SSL directory
sudo mkdir -p /etc/scratchbird/ssl
cd /etc/scratchbird/ssl

# Generate self-signed certificate (for testing)
sudo openssl req -new -x509 -days 365 -nodes \
    -out server.crt \
    -keyout server.key \
    -subj "/CN=scratchbird-server"

# Set permissions
sudo chown scratchbird:scratchbird server.crt server.key
sudo chmod 600 server.key
sudo chmod 644 server.crt
```

### 2. Enable SSL

Edit `/etc/scratchbird/sb_server.conf`:

```ini
[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
min_protocol = TLSv1.2
```

### 3. Restart Server

```bash
sudo systemctl restart scratchbird
```

### 4. Test Connection

```bash
# Connect with SSL
psql "host=localhost port=5432 sslmode=require dbname=mydb user=admin"
```

---

## Certificate Types

### Self-Signed (Testing Only)

Quick to create but triggers browser/client warnings:

```bash
openssl req -new -x509 -days 365 -nodes \
    -out server.crt -keyout server.key \
    -subj "/CN=myserver.example.com"
```

### Let's Encrypt (Free, Trusted)

For public-facing servers:

```bash
# Install certbot
sudo apt install certbot

# Get certificate
sudo certbot certonly --standalone -d db.example.com

# Copy certificates
sudo cp /etc/letsencrypt/live/db.example.com/fullchain.pem /etc/scratchbird/ssl/server.crt
sudo cp /etc/letsencrypt/live/db.example.com/privkey.pem /etc/scratchbird/ssl/server.key

# Set permissions
sudo chown scratchbird:scratchbird /etc/scratchbird/ssl/*
sudo chmod 600 /etc/scratchbird/ssl/server.key
```

### Commercial CA

For enterprise deployments:
1. Generate CSR (Certificate Signing Request)
2. Submit to CA (DigiCert, Comodo, etc.)
3. Install received certificate

```bash
# Generate private key
openssl genrsa -out server.key 4096

# Generate CSR
openssl req -new -key server.key -out server.csr \
    -subj "/C=US/ST=State/L=City/O=Company/CN=db.example.com"

# Submit server.csr to CA, receive server.crt
```

---

## Configuration Options

### [ssl] Section

```ini
[ssl]
# Enable SSL
enabled = true

# Server certificate (PEM format)
cert_file = /etc/scratchbird/ssl/server.crt

# Server private key (PEM format)
key_file = /etc/scratchbird/ssl/server.key

# CA certificate for client verification (optional)
ca_file = /etc/scratchbird/ssl/ca.crt

# Minimum TLS version
min_protocol = TLSv1.2

# Require client certificate
require_client_cert = false
```

### Minimum TLS Version

| Value | Compatibility |
|-------|---------------|
| `TLSv1.2` | Most clients (recommended) |
| `TLSv1.3` | Modern clients only |

---

## Client Certificate Authentication

For high-security environments, require client certificates.

### Server Setup

1. Create CA certificate:
```bash
# Generate CA key
openssl genrsa -out ca.key 4096

# Generate CA certificate
openssl req -new -x509 -days 3650 -key ca.key \
    -out ca.crt -subj "/CN=ScratchBird CA"
```

2. Configure server:
```ini
[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
ca_file = /etc/scratchbird/ssl/ca.crt
require_client_cert = true
```

3. Update hba.conf:
```
hostssl   all   all   0.0.0.0/0   cert
```

### Client Setup

Generate client certificate:

```bash
# Generate client key
openssl genrsa -out client.key 4096

# Generate CSR
openssl req -new -key client.key -out client.csr \
    -subj "/CN=clientuser"

# Sign with CA
openssl x509 -req -days 365 -in client.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out client.crt
```

Connect with client certificate:

```bash
psql "host=server port=5432 sslmode=verify-full \
      sslcert=client.crt sslkey=client.key sslrootcert=ca.crt \
      dbname=mydb user=clientuser"
```

---

## SSL Modes (Client Side)

When connecting, clients can specify SSL requirements:

| Mode | Description |
|------|-------------|
| `disable` | Never use SSL |
| `allow` | Use SSL if server supports |
| `prefer` | Prefer SSL, fall back to plain |
| `require` | Require SSL, don't verify certificate |
| `verify-ca` | Require SSL, verify CA |
| `verify-full` | Require SSL, verify CA and hostname |

### psql Examples

```bash
# Require SSL
psql "host=server sslmode=require dbname=mydb"

# Verify certificate
psql "host=server sslmode=verify-full sslrootcert=ca.crt dbname=mydb"
```

### Connection String

```
postgresql://admin:pass@server:5432/mydb?sslmode=require
```

---

## Verifying SSL

### Check Server SSL Status

```bash
# Using openssl
openssl s_client -connect localhost:5432 -starttls postgres

# Check certificate
openssl s_client -connect localhost:5432 -starttls postgres 2>/dev/null | \
    openssl x509 -noout -text
```

### Check Connection SSL

```sql
-- In sb_isql or psql
SELECT ssl, version FROM pg_stat_ssl WHERE pid = pg_backend_pid();

-- Or
SHOW ssl;
```

---

## Certificate Renewal

### Manual Renewal

```bash
# Generate new certificate
# ... (same process as creation)

# Replace files
sudo cp new_server.crt /etc/scratchbird/ssl/server.crt
sudo cp new_server.key /etc/scratchbird/ssl/server.key

# Set permissions
sudo chown scratchbird:scratchbird /etc/scratchbird/ssl/*
sudo chmod 600 /etc/scratchbird/ssl/server.key

# Reload (no restart needed)
sudo systemctl reload scratchbird
```

### Automated Renewal (Let's Encrypt)

Create renewal hook `/etc/letsencrypt/renewal-hooks/post/scratchbird`:

```bash
#!/bin/bash
cp /etc/letsencrypt/live/db.example.com/fullchain.pem /etc/scratchbird/ssl/server.crt
cp /etc/letsencrypt/live/db.example.com/privkey.pem /etc/scratchbird/ssl/server.key
chown scratchbird:scratchbird /etc/scratchbird/ssl/*
chmod 600 /etc/scratchbird/ssl/server.key
systemctl reload scratchbird
```

```bash
chmod +x /etc/letsencrypt/renewal-hooks/post/scratchbird
```

---

## Troubleshooting

### "SSL not available"

Server not compiled with SSL or SSL disabled:

```bash
# Check if SSL is enabled
grep -i ssl /etc/scratchbird/sb_server.conf
```

### "Certificate verify failed"

Client can't verify server certificate:

```bash
# Add root CA to trusted certificates
psql "host=server sslmode=verify-full sslrootcert=/path/to/ca.crt"
```

### "Private key doesn't match certificate"

Key and certificate don't match:

```bash
# Check modulus (should match)
openssl x509 -noout -modulus -in server.crt | md5sum
openssl rsa -noout -modulus -in server.key | md5sum
```

### Permission Denied

Key file permissions too open:

```bash
# Fix permissions
sudo chmod 600 /etc/scratchbird/ssl/server.key
sudo chown scratchbird:scratchbird /etc/scratchbird/ssl/server.key
```

---

## Security Best Practices

1. **Use TLS 1.2+** - Disable older protocols
2. **Strong keys** - 4096-bit RSA or ECDSA
3. **Proper permissions** - Key files readable only by server
4. **Regular renewal** - Replace certificates before expiry
5. **Verify certificates** - Use `verify-full` on clients
6. **Consider client certs** - For high-security environments

---

## Next Steps

- [Configure authentication](hba.conf.md)
- [Security best practices](../admin/security.md)
- [Monitoring](../admin/monitoring.md)
