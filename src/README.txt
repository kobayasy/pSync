pSync is an open source cloud file sharing system.

Installation:
This process is required for each sync host, including the local host.
1. Run the following commands to build and install.
---
./configure --prefix=$HOME
make install
---
2. Add ~/bin to your PATH if it's not already included.
3. When using for the first time, perform the following initial setup.
   For example, if your sync directory is ~/pSync:
---
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#ID   Directory
psync pSync
.
---

Usage:
1. To show the usage instructions, run the following command.
---
psync --help
---
2. For example, to synchronise with guest@example.com .
---
psync guest@example.com
---
