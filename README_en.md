<!--
README_en.md - Last modified: 17-Jan-2026 (kobayasy)
-->

[ [Features](#Features) | [System Requirements](#System-Requirements) | [Installation](#Installation) | [Usage](#Usage) ] // [Japanase(日本語)](README_ja.md)

---

[![Linux](https://github.com/kobayasy/pSync/workflows/Linux/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-linux.yml) [![macOS](https://github.com/kobayasy/pSync/workflows/macOS/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-macos.yml) [![Windows](https://github.com/kobayasy/pSync/workflows/Windows/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-windows.yml) / [![SAST](https://github.com/kobayasy/pSync/workflows/SAST/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/codeql-analysis.yml)

<img src="psync.png" alt="pSync" align="right">

### Features
- An open source file synchronisation system utilising SSH connections.
- Synchronises files within specified directories between networked hosts.
	- You can specify multiple synchronisation directories.
	- Directory names do not need to match across hosts.
	- Synchronisation directories are managed by labels; directories sharing the same label are targeted for synchronisation.
	- Synchronisation hosts can be separated for each directory.
- Files deleted or updated during synchronisation are kept as backups of their previous state for a specified period.
- The software operates identically on both source and destination hosts.
	- It can relay synchronisation between cloud hosts or synchronise directly between local hosts.
- Even if the process is interrupted (e.g., due to communication loss), the synchronisation operation is performed atomically, ensuring that only complete synchronisations occur and the pre-sync state is maintained.
- The feature to automatically detect file updates and execute synchronisation is intentionally omitted.
	- This prevents files currently being edited from being synchronised unintentionally.
	- Users can execute synchronisation at any desired time.
	- It does not run in the background except during synchronisation, thus conserving system resources.
- SSH connection is used for communication between hosts.
	- No dedicated server software installation is required.
		- Administrator privileges are not needed for installation.
	- User authentication supports public key authentication in addition to password authentication.
	- Transferred data is encrypted and compressed.
- Dependencies on external libraries are minimal; building and installation are possible in any environment where pthread is available.
	- If libtinfo or libncurses is installed during the build process, a feature to display file synchronisation progress via a progress bar is added (this is optional).
- Designed to be memory-efficient, considering operation on embedded devices.
- Supported file types are limited to regular files, symbolic links, and directories.
	- If other types (device files, sockets, FIFOs, etc.) are included, an error will occur and synchronisation will not proceed.
- Hard links are treated as individual files, and the link structure is not maintained.
- The only metadata synchronised is permissions (owner, group, others), creation time, and modification time.
	- Other metadata (User ID, Group ID, access time, etc.) is not synchronised and depends on the destination host's environment.

### System Requirements
- SSH connection must be possible from the source host to the destination host.
	- An SSH server must be installed on the destination host.
	- An SSH client must be installed on the source host.

### Installation
This procedure is required on all hosts involved in synchronisation.
The steps are identical for both source and destination hosts.
1. Execute the build and installation using the following commands:
```sh
curl -LOJs https://github.com/kobayasy/pSync/releases/download/3.5/psync-3.5.tar.gz
tar xzf psync-3.5.tar.gz
cd psync-3.5
./configure --prefix=$HOME
make install

```
2. If `~/bin` is not in your PATH, please add it to your environment variables.
3. The following configuration is **required only for the initial setup**.
This is an example setting `~/pSync` as the synchronisation directory.
Note that while the directory name can differ between the source and destination, the label name must match.
```sh
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#Label Directory
psync  pSync
.

```

### Usage
1. The help message is displayed using the following command:
```sh
psync --help

```
2. For example, to synchronise with `guest@example.com`, execute the following command:
```sh
psync guest@example.com

```
