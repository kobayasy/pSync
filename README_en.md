[Installation](#installation)
|
[Usage](#usage)
*|*
[Japanese(日本語)](README_ja.md)

***

[![Linux](https://github.com/kobayasy/pSync/workflows/Linux/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-linux.yml)
[![macOS](https://github.com/kobayasy/pSync/workflows/macOS/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-macos.yml)
[![Windows](https://github.com/kobayasy/pSync/workflows/Windows/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-windows.yml)
*|*
[![SAST](https://github.com/kobayasy/pSync/workflows/SAST/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/codeql-analysis.yml)

<img src="psync.png" alt="pSync" align="right">

[pSync] is an open-source cloud storage.
Securely and speedy synchronize your files by authenticate, encrypt and compress with [OpenSSH].
So for this to work, you must be able to login to the sync-host with [OpenSSH].

## Installation
This work is required for each sync-host include local-host.
1. Do the following to build and install.
```
curl -LOJs https://github.com/kobayasy/pSync/releases/download/2.14/psync-2.14.tar.gz
tar xzf psync-2.14.tar.gz
cd psync-2.14
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

## Usage
1. Do the following to show usage.
```
psync --help
```
2. For example, when synchronizing with `guest@example.com`.
```
psync guest@example.com
```

[pSync]: https://github.com/kobayasy/pSync
[OpenSSH]: https://www.openssh.com
