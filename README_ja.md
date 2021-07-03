[[英語/English](README_en.md)] [[使い方のヒント](NOTE_ja.md)]

[![Build](https://github.com/kobayasy/pSync/workflows/Build/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build.yml)
[![CodeQL](https://github.com/kobayasy/pSync/workflows/CodeQL/badge.svg)](https://github.com/kobayasy/pSync/security/code-scanning)

<div align="center"><img src="psync.png" alt="pSync"></div>

オープンソースのクラウドストーレージです。  
[OpenSSH] によって認証/暗号化/圧縮を行い、安全かつ高速にファイル同期を行ないます。  
よってこれを動かすには、[OpenSSH] で同期先ホストへログイン出来る必要が有ります。  

### インストール:
ローカルホストを含めた全ての同期ホストに対してこの作業が必要です。  
1. 以下の手順でビルドとインストールを行ないます。  
```
curl -LOJs https://github.com/kobayasy/pSync/releases/download/2.7/psync-2.7.tar.gz
tar xzf psync-2.7.tar.gz
cd psync-2.7
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

### 使い方:
1. 以下の手順で使い方が表示されます。  
```
psync --help
```
2. 例えば `guest@example.com` と同期する場合は以下の手順になります。  
```
psync guest@example.com
```

[OpenSSH]: https://www.openssh.com
