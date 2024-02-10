pSync is an open source cloud file sharing system.
Securely and speedy synchronize your files by authenticate, encrypt and compress with SSH.
So for this to work, you must be able to login to the sync-host with SSH.

Installation:
This work is required for each sync-host include local-host.
1. Do the following to build and install.
---
./configure --prefix=$HOME
make install
---
2. Add ~/bin in the PATH if not included.
3. When using for the 1st time, do the following too.
   For example, if the sync-directory is ~/pSync .
---
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#ID   Directory
psync pSync
.
---

Usage:
1. Do the following to show usage.
---
psync --help
---
2. For example, when synchronizing with guest@example.com .
---
psync guest@example.com
---
