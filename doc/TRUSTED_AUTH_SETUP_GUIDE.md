# ScratchBird Trusted Authentication Setup Guide
## Windows Client to Linux Server Cross-Platform Authentication

This guide explains how to set up trusted authentication so Windows users can connect to a Linux ScratchBird server using their Windows domain credentials, without entering usernames or passwords.

---

## 📋 **What You'll Need**

### Requirements
- **Windows Server** with Active Directory Domain Services
- **Linux Server** (Ubuntu/Debian/CentOS/RHEL) for ScratchBird
- **Windows Client** computers joined to the domain
- **Network connectivity** between all systems
- **Administrative access** to all systems

### What This Setup Achieves
- Windows users connect to Linux ScratchBird using their Windows login
- No passwords needed - uses Windows domain authentication
- Secure Kerberos-based authentication
- Seamless user experience

---

## 🔧 **Part 1: Windows Domain Controller Setup**

### Step 1.1: Verify Active Directory is Running
1. On your Windows Server, open **Server Manager**
2. Click **Tools** → **Active Directory Users and Computers**
3. Verify you can see your domain (e.g., `COMPANY.LOCAL`)
4. Note your domain name - you'll need this later

### Step 1.2: Create Service Account for ScratchBird
1. In **Active Directory Users and Computers**:
   - Right-click **Users** → **New** → **User**
   - Username: `scratchbird-service`
   - Password: Create a strong password (save this!)
   - Uncheck "User must change password at next logon"
   - Check "Password never expires"

2. Set Service Principal Name (SPN):
   - Open **Command Prompt as Administrator**
   - Run: `setspn -A scratchbird/your-linux-server.company.local scratchbird-service`
   - Replace `your-linux-server.company.local` with your actual Linux server hostname

### Step 1.3: Create Keytab File
1. On Windows Server, open **Command Prompt as Administrator**
2. Run this command (replace with your details):
   ```cmd
   ktpass -princ scratchbird/your-linux-server.company.local@COMPANY.LOCAL ^
          -mapuser scratchbird-service@COMPANY.LOCAL ^
          -crypto AES256-SHA1 ^
          -ptype KRB5_NT_PRINCIPAL ^
          -pass YourStrongPassword ^
          -out scratchbird.keytab
   ```
3. This creates `scratchbird.keytab` - copy this file to your Linux server

---

## 🐧 **Part 2: Linux Server Setup**

### Step 2.1: Install Required Packages

**For Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y krb5-user krb5-config sssd sssd-tools realmd adcli
```

**For CentOS/RHEL:**
```bash
sudo yum install -y krb5-workstation sssd sssd-tools realmd adcli
```

### Step 2.2: Configure Kerberos
1. Edit `/etc/krb5.conf`:
   ```bash
   sudo nano /etc/krb5.conf
   ```

2. Replace contents with (adjust for your domain):
   ```ini
   [libdefaults]
       default_realm = COMPANY.LOCAL
       dns_lookup_realm = true
       dns_lookup_kdc = true
       ticket_lifetime = 24h
       renew_lifetime = 7d
       forwardable = true

   [realms]
       COMPANY.LOCAL = {
           kdc = your-domain-controller.company.local
           admin_server = your-domain-controller.company.local
       }

   [domain_realm]
       .company.local = COMPANY.LOCAL
       company.local = COMPANY.LOCAL
   ```

### Step 2.3: Join Linux Server to Domain
1. Discover the domain:
   ```bash
   sudo realm discover company.local
   ```

2. Join the domain:
   ```bash
   sudo realm join --user=administrator company.local
   ```
   - Enter the domain administrator password when prompted

3. Verify the join:
   ```bash
   sudo realm list
   ```

### Step 2.4: Configure SSSD
1. Edit `/etc/sssd/sssd.conf`:
   ```bash
   sudo nano /etc/sssd/sssd.conf
   ```

2. Add these lines:
   ```ini
   [sssd]
   domains = company.local
   config_file_version = 2
   services = nss, pam

   [domain/company.local]
   ad_domain = company.local
   krb5_realm = COMPANY.LOCAL
   realmd_tags = manages-system joined-with-samba
   cache_credentials = True
   id_provider = ad
   krb5_store_password_if_offline = True
   default_shell = /bin/bash
   ldap_id_mapping = True
   use_fully_qualified_names = False
   fallback_homedir = /home/%u
   access_provider = ad
   ```

3. Restart SSSD:
   ```bash
   sudo systemctl restart sssd
   sudo systemctl enable sssd
   ```

### Step 2.5: Install ScratchBird Keytab
1. Copy the `scratchbird.keytab` file from Windows server to Linux:
   ```bash
   sudo cp scratchbird.keytab /etc/scratchbird.keytab
   sudo chown scratchbird:scratchbird /etc/scratchbird.keytab
   sudo chmod 600 /etc/scratchbird.keytab
   ```

2. Set environment variable for ScratchBird:
   ```bash
   echo 'export KRB5_KTNAME=/etc/scratchbird.keytab' | sudo tee -a /etc/environment
   ```

### Step 2.6: Test Domain Authentication
1. Test user lookup:
   ```bash
   getent passwd domain-username
   ```

2. Test Kerberos authentication:
   ```bash
   kinit domain-username@COMPANY.LOCAL
   klist
   ```

---

## 🔥 **Part 3: ScratchBird Server Configuration**

### Step 3.1: Configure ScratchBird Authentication
1. Edit `/etc/scratchbird/scratchbird.conf`:
   ```bash
   sudo nano /etc/scratchbird/scratchbird.conf
   ```

2. Add/modify these settings:
   ```ini
   # Enable trusted authentication
   AuthServer = Srp, Linux_GSSAPI
   AuthClient = Srp, Linux_GSSAPI, Legacy_Auth
   
   # User manager for mapping domain users
   UserManager = Srp, Linux_GSSAPI
   
   # Wire compression (optional, improves performance)
   WireCompression = true
   ```

### Step 3.2: Create User Mapping
1. Connect to ScratchBird as SYSDBA:
   ```bash
   isql -user SYSDBA -password your-sysdba-password
   ```

2. Create global mapping for domain users:
   ```sql
   CREATE GLOBAL MAPPING TRUSTED_DOMAIN_USERS
   USING PLUGIN Linux_GSSAPI
   FROM ANY USER
   TO USER;
   
   COMMIT;
   ```

3. Or create specific user mappings:
   ```sql
   CREATE GLOBAL MAPPING DOMAIN_ADMIN
   USING PLUGIN Linux_GSSAPI  
   FROM "COMPANY\administrator"
   TO USER SYSDBA;
   
   CREATE GLOBAL MAPPING DOMAIN_USER
   USING PLUGIN Linux_GSSAPI
   FROM "COMPANY\john.doe"
   TO USER JOHN_DOE;
   
   COMMIT;
   ```

### Step 3.3: Restart ScratchBird
```bash
sudo systemctl restart scratchbird
sudo systemctl status scratchbird
```

---

## 💻 **Part 4: Windows Client Configuration**

### Step 4.1: Verify Client Domain Join
1. On Windows client, open **Command Prompt**
2. Run: `echo %USERDOMAIN%` - should show your domain name
3. Run: `whoami` - should show `DOMAIN\username`

### Step 4.2: Test Kerberos Connectivity
1. Open **Command Prompt as Administrator**
2. Test connection to Linux server:
   ```cmd
   telnet your-linux-server.company.local 3050
   ```
   - Should connect (press Ctrl+C to exit)

### Step 4.3: Configure ScratchBird Client
1. If using ScratchBird client tools, no additional configuration needed
2. For applications, use connection string:
   ```
   your-linux-server.company.local:employee_database
   ```
   - No username/password required!

---

## 🧪 **Part 5: Testing the Setup**

### Step 5.1: Test from Windows Client
1. Open **Command Prompt** (as domain user)
2. Test connection:
   ```cmd
   isql your-linux-server.company.local:employee_database
   ```
   - Should connect without prompting for credentials

### Step 5.2: Test from Application
Example connection strings:
- **FlameRobin:** `your-linux-server.company.local:employee_database`
- **IBExpert:** `your-linux-server.company.local:employee_database`
- **ODBC:** `DRIVER={ScratchBird/InterBase(r) driver};DBNAME=your-linux-server.company.local:employee_database;`

### Step 5.3: Verify Authentication Method
1. On Linux server, check ScratchBird logs:
   ```bash
   sudo tail -f /var/log/scratchbird/scratchbird.log
   ```
2. Look for messages like: `Authentication plugin Linux_GSSAPI is loaded`

---

## 🚨 **Troubleshooting Guide**

### Common Issues and Solutions

#### Issue: "Unable to complete network request"
**Cause:** Network connectivity or DNS issues
**Solution:**
1. Verify DNS resolution: `nslookup your-linux-server.company.local`
2. Check firewall: `sudo ufw status` (Ubuntu) or `firewall-cmd --list-all` (CentOS)
3. Ensure port 3050 is open: `sudo netstat -tlnp | grep 3050`

#### Issue: "Authentication failed"
**Cause:** Kerberos/domain authentication problems
**Solution:**
1. Test domain authentication: `kinit username@COMPANY.LOCAL`
2. Check time synchronization: `sudo ntpdate -s your-domain-controller.company.local`
3. Verify keytab: `sudo klist -k /etc/scratchbird.keytab`

#### Issue: "Plugin Linux_GSSAPI not found"
**Cause:** GSS-API libraries not installed or ScratchBird not compiled with GSS-API support
**Solution:**
1. Install GSS-API: `sudo apt install libgssapi-krb5-2` (Ubuntu)
2. Verify ScratchBird compilation: Check for `TRUSTED_AUTH_GSSAPI` in build

#### Issue: "User mapping failed"
**Cause:** User mapping configuration issues
**Solution:**
1. Check mappings: `SELECT * FROM SEC$GLOBAL_AUTH_MAPPING;`
2. Verify plugin name exactly matches: `Linux_GSSAPI`
3. Check principal format: Should be `DOMAIN\username`

#### Issue: Client keeps prompting for password
**Cause:** Client-side authentication falling back to password
**Solution:**
1. Verify client is domain-joined: `echo %USERDOMAIN%`
2. Check ScratchBird client version supports trusted auth
3. Ensure connection string doesn't include username/password

---

## 🔒 **Security Considerations**

### Best Practices
1. **Keytab Security:** Protect `/etc/scratchbird.keytab` with strict permissions (600)
2. **Time Synchronization:** Keep all systems synchronized (Kerberos requirement)
3. **Network Security:** Use firewalls to restrict access to port 3050
4. **User Mapping:** Be specific with user mappings rather than using wildcard mappings
5. **Monitoring:** Monitor authentication logs for suspicious activity

### Firewall Configuration
**Ubuntu/Debian:**
```bash
sudo ufw allow from your-client-subnet/24 to any port 3050
```

**CentOS/RHEL:**
```bash
sudo firewall-cmd --permanent --add-rich-rule="rule family=ipv4 source address=your-client-subnet/24 port protocol=tcp port=3050 accept"
sudo firewall-cmd --reload
```

---

## 📞 **Support and Maintenance**

### Regular Maintenance Tasks
1. **Monthly:** Check domain trust relationship
2. **Quarterly:** Review user mappings and remove unused accounts
3. **Annually:** Rotate service account passwords and regenerate keytabs

### Log Files to Monitor
- **ScratchBird:** `/var/log/scratchbird/scratchbird.log`
- **SSSD:** `/var/log/sssd/sssd.log`
- **Kerberos:** `/var/log/krb5kdc.log`

### Getting Help
- Check system logs first: `sudo journalctl -u scratchbird`
- Test authentication step by step using the troubleshooting guide
- Verify each component (DNS, Kerberos, SSSD) individually

---

## 🎯 **Quick Reference**

### Key Commands
```bash
# Test domain connectivity
realm list

# Test user lookup
getent passwd domain-user

# Test Kerberos
kinit username@DOMAIN.LOCAL

# Check ScratchBird status
sudo systemctl status scratchbird

# View authentication logs
sudo tail -f /var/log/scratchbird/scratchbird.log
```

### Important Files
- `/etc/krb5.conf` - Kerberos configuration
- `/etc/sssd/sssd.conf` - Domain services configuration
- `/etc/scratchbird/scratchbird.conf` - ScratchBird server configuration
- `/etc/scratchbird.keytab` - ScratchBird service credentials

This completes the setup for trusted authentication between Windows clients and Linux ScratchBird servers!
