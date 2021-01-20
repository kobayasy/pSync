[![Build Status](https://github.com/kobayasy/pSync/workflows/Build/badge.svg)](https://github.com/kobayasy/pSync/actions)

# pSync って何?
[Dropbox] が [Raspberry Pi] で動かないので、その代替えに作ったネットワーク双方向ファイル同期ツール。

[Raspberry Pi] 以外でも POSIX に準拠した環境であれば動作するはず。
[Raspberry Pi OS] と [macOS]、[Debian/Linux]、[Ubuntu] では普段使いしていて、問題なく動作している。
[Cygwin] でも一通りの動作は確認済みだけど、実用に耐えるかは不明。

#### 特徴は↓
- 複数のディレクトリを同期対象に指定可能。
- 専用のサーバが不要で一般ユーザ権限でインストールできる。
- ネットワーク上に流れるデータは暗号化と圧縮されるので安全で高速。

#### 詳しい説明は↓
https://kobayasy.com/psync/

---

## 説明を読むのが面倒だけどとりあえず使ってみたい人向けの、細かい説明を省いたインストールからファイル同期までの手順説明。

### インストールの前準備:
- ユーザー認証と転送データの暗号化と圧縮に [OpenSSH] を使用するので、[OpenSSH] の ssh コマンドで同期先ホストへログイン出来る設定が必要となる。 先に済ませておく事。
- ファイルの更新日時と作成日時を同期方向を決定する為に使用しているので、同期元ホスト、同期先ホスト共にその元となるシステム時刻が正確に合っている必要がある。 NTP 等で常時合っている状態にしておく事。

### ローカルホストへ pSync をインストールする手順:
```
localhost$ curl -O http://kobayasy.com/psync/psync-2.2.tar.gz
localhost$ tar xzf psync-2.2.tar.gz
localhost$ cd psync-2.2
localhost$ ./configure --prefix=$HOME
localhost$ make install
localhost$ mkdir -m700 ~/pSync
localhost$ mkdir -m700 ~/pSync/Private
localhost$ mkdir -m700 ~/pSync/Work
localhost$ cat <<EOF >~/.psync.conf
#ID     Directory
private pSync/Private
work    pSync/Work
EOF
localhost$ chmod 600 ~/.psync.conf
localhost$ 
```

### 同期先ホストへ pSync をインストールする手順:
同期先が guest@example.com の場合を例に説明。

```
localhost$ ssh guest@example.com
example.com$ curl -O http://kobayasy.com/psync/psync-2.2.tar.gz
example.com$ tar xzf psync-2.2.tar.gz
example.com$ cd psync-2.2
example.com$ ./configure --prefix=$HOME
example.com$ make install
example.com$ mkdir -m700 ~/pSync
example.com$ mkdir -m700 ~/pSync/Private
example.com$ mkdir -m700 ~/pSync/Work
example.com$ cat <<EOF >~/.psync.conf
#ID     Directory
private pSync/Private
work    pSync/Work
EOF
example.com$ chmod 600 ~/.psync.conf
example.com$ exit
localhost$ 
```

### 動作確認の手順:
ローカルホスト、同期先ホスト共に pSync のバージョン情報が表示されて Error から始まる表示が含まれていなければインストールに成功している。

```
localhost$ psync --help
pSync version 2.2 (protocol pSp1)
(以下コマンドの説明が続く)
localhost$ ssh guest@example.com psync --help
pSync version 2.2 (protocol pSp1)
(以下コマンドの説明が続く)
localhost$ 
```

バージョン情報が表示されない場合は psync に実行パスが通っていない事が考えられる。
実行パスの設定を確認して通ってないならば通す。
もしくは configure のオプションでインストール先を実行パスが通っている場所へ変更する。

表示内容に Error から始まる行が含まれる場合は、その行に原因も一緒に表示されているのでそれを取り除く。

### ファイル同期の手順:
これを実行する度に localhost の ~/pSync/ と example.com の ~guest/pSync/ のファイルが同期される。

```
localhost$ psync guest@example.com
psync
localhost$ 
```

---

## 使い方のヒント

### パスワード入力の自動化:
バックグラウンドで psync を自動実行させる場合、パスワード入力をどうやって自動化させるかが問題になる。
そんな時は、ssh 用パスワード自動入力コマンドの sshpass で解決できる。

例えば、同期先のホスト名が example.com、ユーザー名が guest で パスワードが password の場合、下記のように実行する。

```
shpass -p password psync guest@example.com
```

sshpass の man にも記載があるように、この方法は設定ファイルやスクリプトファイルに生のパスワードを書く必要がありセキュリティ的に危険なので、パスワード認証を止めて公開鍵認証に切り替える事をお勧めする。

### ファイル更新があった時に psync を自動実行:
ファイルアクセスを監視して更新タイミングでコマンド実行するツールと組み合わせる事で、ファイル更新があった時に psync を自動実行させられる。

例えば、Linux の場合だと inotify と組み合わせて、下記スクリプトで自動実行が出来る。
SYNCDIR と SYNCCMD、BUFFTIM は環境に合わせて書き換える事。

```
#! /bin/sh

SYNCDIR=~/pSync
SYNCCMD='sshpass -p password psync guest@example.com'
BUFFTIM=10000

$SYNCCMD && exec inotify-hookable -w "`realpath $SYNCDIR`" -i .psync -c "$SYNCCMD" -t "$BUFFTIM" -q
```

[Cygwin]: https://www.cygwin.com
[Debian/Linux]: https://www.debian.org
[Dropbox]: https://www.dropbox.com
[macOS]: https://www.apple.com/macos/
[OpenSSH]: https://www.openssh.com
[Raspberry Pi]: https://www.raspberrypi.org
[Raspberry Pi OS]: https://www.raspberrypi.org/software/operating-systems/#raspberry-pi-os-32-bit
[Ubuntu]: https://ubuntu.com
