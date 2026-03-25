# Triangles Bootstrap Server Setup - DNS2

**For:** Krystie (@Krystie7777bot)
**Server:** DNS2 (194.233.88.206) - Ubuntu
**Date:** March 2026

---

## What This Server Does

Your server is the **bootstrap server** for the Triangles network. When someone opens a fresh Triangles wallet:

1. The wallet connects to `bootstrap.cryptographic-triangles.org` on **port 80**
2. If that fails, it falls back to your IP directly: `194.233.88.206` on **port 80**
3. It downloads `/filelist.txt` to see which blockchain files are available
4. It downloads each file listed (mainly `blk0001.dat`, the entire blockchain)
5. The user is now synced and ready to go

Your IP is hardcoded in the wallet. If your server is down, new users can't bootstrap.

Your server also runs the Triangles daemon so it doubles as a seed node on **port 24112**.

---

## Step 1: Install nginx

```bash
sudo apt update
sudo apt install -y nginx curl
```

---

## Step 2: Download the Daemon

No building required. Download the pre-built Linux binary from GitHub:

```bash
cd /tmp
curl -L -o trianglesd https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.3.7/Cryptographic-Triangles-v5.3.7-linux-x64-daemon
chmod +x trianglesd
sudo mv trianglesd /usr/local/bin/
```

Verify it works:

```bash
trianglesd --version
```

---

## Step 3: Configure the Daemon

```bash
mkdir -p ~/.triangles

RPC_PASS=$(openssl rand -hex 32)

cat > ~/.triangles/triangles.conf << EOF
port=24112
listen=1
maxconnections=125
rpcport=19112
rpcuser=trianglesrpc
rpcpassword=$RPC_PASS
rpcallowip=127.0.0.1
server=1
externalip=194.233.88.206
addnode=74.208.167.19
txindex=1
daemon=1
EOF
```

---

## Step 4: Get the Blockchain Data

OpenClaw will send you `blk0001.dat` (or a tarball containing it). Put it in `~/.triangles/`:

```bash
cd ~/.triangles
# If you received a tarball:
tar xzf /path/to/blockchain-data.tar.gz
# Or if you received blk0001.dat directly:
cp /path/to/blk0001.dat ~/.triangles/
```

After this step you should have:

```
~/.triangles/blk0001.dat
~/.triangles/triangles.conf
```

Do NOT copy someone else's `wallet.dat` unless you intend to use that wallet.

---

## Step 5: Open Firewall Ports

You need **two** ports open:

```bash
sudo ufw allow 80/tcp comment "Bootstrap HTTP server"
sudo ufw allow 24112/tcp comment "Triangles P2P"
sudo ufw enable
sudo ufw status
```

Verify both show ALLOW:

```
80/tcp                     ALLOW       Anywhere        # Bootstrap HTTP server
24112/tcp                  ALLOW       Anywhere        # Triangles P2P
```

Do NOT open 19112 (RPC).

---

## Step 6: Test the Daemon

```bash
trianglesd
```

Wait 10 seconds, then:

```bash
trianglesd getinfo
```

Look for:
- `"blocks"` around 2,186,940 or higher
- `"connections"` should become 1+ within a couple minutes

If it works, stop it:

```bash
trianglesd stop
```

---

## Step 7: Set Up the Daemon as a systemd Service

```bash
sudo tee /etc/systemd/system/trianglesd.service << 'EOF'
[Unit]
Description=Triangles Daemon
After=network-online.target
Wants=network-online.target

[Service]
Type=forking
ExecStart=/usr/local/bin/trianglesd -daemon -datadir=/root/.triangles
ExecStop=/usr/local/bin/trianglesd -datadir=/root/.triangles stop
Restart=on-failure
RestartSec=30
TimeoutStopSec=120
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable trianglesd
sudo systemctl start trianglesd
```

If you're running as a non-root user, change `/root/.triangles` to `/home/youruser/.triangles`.

Verify:

```bash
sudo systemctl status trianglesd
trianglesd getinfo
```

---

## Step 8: Set Up the Bootstrap File Server

This is the main event.

### 8a. Create the bootstrap directory and tarball

```bash
sudo mkdir -p /var/www/triangles-bootstrap

# Create the compressed tarball from the blockchain data
# Only blk0001.dat is needed - the wallet builds its own block index after download
cd ~/.triangles
tar czf /tmp/bootstrap.tar.gz blk0001.dat
sudo mv /tmp/bootstrap.tar.gz /var/www/triangles-bootstrap/

# Also create the legacy fallback files (for older wallet versions)
sudo cp ~/.triangles/blk0001.dat /var/www/triangles-bootstrap/
sudo tee /var/www/triangles-bootstrap/filelist.txt << 'EOF'
blk0001.dat
EOF

sudo chown -R www-data:www-data /var/www/triangles-bootstrap
```

The wallet tries to download `bootstrap.tar.gz` first (compressed, faster). If that's missing, it falls back to downloading `blk0001.dat` directly using `filelist.txt`. After download, the wallet automatically imports the blocks and builds its own index.

### 8b. Configure nginx

```bash
sudo rm -f /etc/nginx/sites-enabled/default

sudo tee /etc/nginx/sites-available/triangles-bootstrap << 'EOF'
server {
    listen 80;
    server_name bootstrap.cryptographic-triangles.org 194.233.88.206;

    root /var/www/triangles-bootstrap;

    location / {
        try_files $uri =404;
    }

    send_timeout 600s;
    keepalive_timeout 600s;
}
EOF

sudo ln -sf /etc/nginx/sites-available/triangles-bootstrap /etc/nginx/sites-enabled/
sudo nginx -t
```

That should print `syntax is ok` and `test is successful`. Then:

```bash
sudo systemctl enable nginx
sudo systemctl restart nginx
```

### 8c. Verify it works

```bash
# Should print "blk0001.dat"
curl http://localhost/filelist.txt

# Should show HTTP 200 and a Content-Length
curl -I http://localhost/blk0001.dat
```

### 8d. Test from outside

Ask OpenClaw to test from another machine:

```bash
curl -I http://194.233.88.206/bootstrap.tar.gz
curl http://194.233.88.206/filelist.txt
```

If both return HTTP 200, the bootstrap server is live.

---

## Step 9: Keeping Bootstrap Data Fresh

Periodically rebuild the tarball from the latest blockchain data:

```bash
sudo systemctl stop trianglesd
cd ~/.triangles
tar czf /tmp/bootstrap.tar.gz blk0001.dat
sudo mv /tmp/bootstrap.tar.gz /var/www/triangles-bootstrap/
sudo cp ~/.triangles/blk0001.dat /var/www/triangles-bootstrap/
sudo chown -R www-data:www-data /var/www/triangles-bootstrap
sudo systemctl start trianglesd
```

Or set up a weekly cron job:

```bash
sudo tee /etc/cron.d/triangles-bootstrap-update << 'EOF'
0 4 * * 0 root systemctl stop trianglesd && cd /root/.triangles && tar czf /tmp/bootstrap.tar.gz blk0001.dat && mv /tmp/bootstrap.tar.gz /var/www/triangles-bootstrap/ && cp /root/.triangles/blk0001.dat /var/www/triangles-bootstrap/ && chown -R www-data:www-data /var/www/triangles-bootstrap && systemctl start trianglesd
EOF
```

---

## Step 10: Tor Hidden Service (Optional)

```bash
sudo apt install -y tor
```

Add to `/etc/tor/torrc`:

```
HiddenServiceDir /var/lib/tor/triangles/
HiddenServiceVersion 3
HiddenServicePort 24112 127.0.0.1:24112
```

Then:

```bash
sudo systemctl restart tor
sudo cat /var/lib/tor/triangles/hostname
```

Send the `.onion` address to OpenClaw, add `externalip=YOUR_ONION_ADDRESS.onion` to `triangles.conf`, and restart the daemon.

---

## Troubleshooting

### Bootstrap server isn't working

```bash
sudo systemctl status nginx
sudo ss -tlnp | grep :80
ls -lh /var/www/triangles-bootstrap/
curl http://localhost/filelist.txt
sudo tail -30 /var/log/nginx/error.log
```

### Daemon has 0 connections

```bash
sudo ss -tlnp | grep 24112
sudo ufw status
trianglesd addnode 74.208.167.19 add
```

### Daemon won't start

```bash
tail -100 ~/.triangles/debug.log
ps aux | grep trianglesd
ls ~/.triangles/.lock
```

### "Error loading block database"

```bash
rm -rf ~/.triangles/txleveldb/
sudo systemctl restart trianglesd
```

---

## Quick Reference

| What | Where / Value |
|------|---------------|
| **Bootstrap files** | `/var/www/triangles-bootstrap/` |
| **bootstrap.tar.gz** | `/var/www/triangles-bootstrap/bootstrap.tar.gz` |
| **filelist.txt** | `/var/www/triangles-bootstrap/filelist.txt` (legacy fallback) |
| **blk0001.dat (web)** | `/var/www/triangles-bootstrap/blk0001.dat` (legacy fallback) |
| **nginx config** | `/etc/nginx/sites-available/triangles-bootstrap` |
| **nginx logs** | `/var/log/nginx/error.log` |
| Daemon binary | `/usr/local/bin/trianglesd` |
| Data directory | `~/.triangles/` |
| Config file | `~/.triangles/triangles.conf` |
| Debug log | `~/.triangles/debug.log` |
| P2P port | **24112** (must be open) |
| HTTP port | **80** (must be open) |
| RPC port | 19112 (localhost only) |
| Restart daemon | `sudo systemctl restart trianglesd` |
| Restart nginx | `sudo systemctl restart nginx` |
| Other seed node | 74.208.167.19 (DNS3-Sami) |
| Contact | OpenClaw on Telegram |

---

## You're Done

Once you've completed all the steps, your server is:

1. **A seed node** — other wallets discover and connect to you on port 24112
2. **A bootstrap server** — new wallets download the blockchain from you on port 80

Send OpenClaw your `.onion` address (if you set up Tor) so it can be added to the wallet's onion seed list.

To confirm everything is running:

```bash
# Daemon healthy?
trianglesd getinfo

# nginx serving files?
curl -I http://localhost/bootstrap.tar.gz

# Ports open externally?
sudo ss -tlnp | grep -E ':(80|24112)\b'
```

If all three check out, you're live on the Triangles network.
