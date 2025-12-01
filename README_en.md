<!--
README_ja.md - Last modified: 29-Nov-2025 (kobayasy)
-->

[ [Features](#Features) | [System Requirements](#System-Requirements) | [Installation](#Installation) | [Usage](#Usage) ] // [Japanase(日本語)](README_ja.md)

---

[![Linux](https://github.com/kobayasy/pSync/workflows/Linux/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-linux.yml) [![macOS](https://github.com/kobayasy/pSync/workflows/macOS/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-macos.yml) [![Windows](https://github.com/kobayasy/pSync/workflows/Windows/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-windows.yml) / [![SAST](https://github.com/kobayasy/pSync/workflows/SAST/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/codeql-analysis.yml)

<img src="psync.png" alt="pSync" align="right">

### Features
- An open source cloud file sharing system.
- Synchronises files in specified directories between network-connected hosts.
	- Multiple sync directories can be specified.
	- Sync directory names do not need to match between hosts.
	- Sync directories are managed by labels; directories with the same label are synchronised.
	- Different hosts can be specified for each directory.
- For files deleted or updated by a sync, the previous version is kept as a backup for a set period.
- It runs as the same software on both source and destination hosts, with no distinction between them.
	- It is possible to relay synchronisation between cloud hosts or to synchronise directly between local hosts.
- If the process is interrupted (e.g., by a network disconnection), partial synchronisation is prevented, and the pre-sync state is maintained.
- It does not automatically detect file updates to start synchronisation.
	- This prevents work-in-progress files from being unintentionally synchronised.
	- Users can run the synchronisation at any time.
	- It does not run in the background outside of the synchronisation process, thus conserving system resources.
- Uses SSH for communication between hosts.
	- No dedicated server installation is required.
		- Administrator privileges are not required for installation.
	- Supports public key authentication as well as passwords.
	- Data transfer is encrypted and compressed.
- No external libraries are required; it can be built and installed in any environment where pthread is available.
	- If libtinfo or libncurses is installed at build time, an optional progress bar for file sync will be enabled.
- A memory-efficient design, also suitable for embedded devices.
- Supported file types are regular files, symbolic links, and directories.
	- An error will occur and synchronisation will be aborted if other file types (e.g., device files, sockets, FIFOs) are present.
- Hard links are treated as separate files; the link is not preserved.
- The only metadata synchronised are permissions (owner, group, and other), creation time, and modification time.
	- Other metadata (e.g., user ID, group ID, access time) are not synchronised and will depend on the destination host's settings.

### System Requirements
- SSH server installed on the destination host.
- SSH client installed on the source host.
- SSH connectivity from the source host to the destination host.

### Installation
This procedure is required on all hosts to be synchronised, including the local host.
1. Run the following commands to build and install:
```sh
curl -LOJs https://github.com/kobayasy/pSync/releases/download/3.2/psync-3.2.tar.gz
tar xzf psync-3.2.tar.gz
cd psync-3.2
./configure --prefix=$HOME
make install

```
2. If `~/bin` is not in your PATH, please add it.
3. The following one-time setup is required. This example configures `~/pSync` as a sync directory:
```sh
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#Label Directory
psync  pSync
.

```

### Usage
1. To display the help message, run the following command:
```sh
psync --help

```
2. To synchronise with `guest@example.com`, for example, run the following command:
```sh
psync guest@example.com

```

