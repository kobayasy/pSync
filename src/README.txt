pSync is an open source cloud file sharing system. Securely and quickly synchronise your files using SSH for authentication, encryption, and compression.

Installation:
This procedure is required on all hosts involved in synchronisation. The steps are identical for both source and destination hosts.
1. Execute the build and installation using the following commands:
---
./configure --prefix=$HOME
make install
---
2. If ~/bin is not in your PATH, please add it to your environment variables.
3. The following configuration is required only for the initial setup. This is an example setting ~/pSync as the synchronisation directory. Note that while the directory name can differ between the source and destination, the label name must match.
---
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#Label Directory
psync  pSync
.
---

Usage:
1. The help message is displayed using the following command:
---
psync --help
---
2. For example, to synchronise with guest@example.com, execute the following command:
---
psync guest@example.com
---
