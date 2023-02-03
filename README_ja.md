[インストール](#インストール)
|
[使い方](#使い方)
*|*
[使い方のヒント](doc/NOTE_ja.md)
*|*
[技術資料](doc/DEV_ja.md)
*|*
[英語(English)](README_en.md)

***

[![Linux](https://github.com/kobayasy/pSync/workflows/Linux/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-linux.yml)
[![macOS](https://github.com/kobayasy/pSync/workflows/macOS/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-macos.yml)
[![Windows](https://github.com/kobayasy/pSync/workflows/Windows/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-windows.yml)
*|*
[![SAST](https://github.com/kobayasy/pSync/workflows/SAST/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/codeql-analysis.yml)

<img src="psync.png" alt="pSync" align="right">

[pSync] はオープンソースのクラウドストーレージです。
[OpenSSH] によって認証/暗号化/圧縮を行い、安全かつ高速にファイル同期を行ないます。
よってこれを動かすには、[OpenSSH] で同期先ホストへログイン出来る必要が有ります。

## インストール
ローカルホストを含めた全ての同期ホストに対してこの作業が必要です。
1. 以下の手順でビルドとインストールを行ないます。
```
curl -LOJs https://github.com/kobayasy/pSync/releases/download/2.22/psync-2.22.tar.gz
tar xzf psync-2.22.tar.gz
cd psync-2.22
./configure --prefix=$HOME
make install
```
2. PATH に ~/bin が含まれていない場合は追加してください。
3. 始めて使う場合は以下の手順も必要です。
これは `~/pSync` を同期ディレクトリにする例です。
```
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#ID   Directory
psync pSync
.
```

## 使い方
1. 以下の手順で使い方が表示されます。
```
psync --help
```
2. 例えば `guest@example.com` と同期する場合は以下の手順になります。
```
psync guest@example.com
```

[pSync]: https://github.com/kobayasy/pSync
[OpenSSH]: https://www.openssh.com
