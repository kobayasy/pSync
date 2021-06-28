[[Japanese/日本語](README_ja.md)]

[![Build Status](https://github.com/kobayasy/pSync/workflows/Build/badge.svg)](https://github.com/kobayasy/pSync/actions)

<div align="center"><img src="psync.png" alt="pSync"></div>

This is an open-source cloud storage.  
Securely and speedy synchronize your files by authenticate, encrypt and compress with [OpenSSH].  
So for this to work, you must be able to login to the sync-host with [OpenSSH].  

### Installation:
This work is required for each sync-host include local-host.  
1. Do the following to build and install.  
```
curl -LOJs https://github.com/kobayasy/pSync/releases/download/2.6/psync-2.6.tar.gz
tar xzf psync-2.6.tar.gz
cd psync-2.6
./configure --prefix=$HOME
make install
```
2. Add ~/bin in the PATH if not included.  
3. When using for the 1st time, do the following too.  
For example, if the sync-directory is `~/pSync`.  
```
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#ID   Directory
psync pSync
.
```

### Uesage:
1. Do the following to show usage.  
```
psync --help
```
2. For example, when synchronizing with `guest@example.com`.  
```
psync guest@example.com
```

[OpenSSH]: https://www.openssh.com
