[[README](README_ja.md)]

***

# 使い方のヒント

## パスワード入力の自動化
バックグラウンドで psync を自動実行させる場合、パスワード入力をどうやって自動化させるかが問題になる。
そんな時は、ssh 用パスワード自動入力コマンドの sshpass で解決できる。

例えば、同期先のホスト名が `example.com`、ユーザー名が `guest` で パスワードが `password` の場合、下記のように実行する。

```
shpass -p password psync guest@example.com
```

sshpass の man にも記載があるように、この方法は設定ファイルやスクリプトファイルに生のパスワードを書く必要がありセキュリティ的に危険なので、パスワード認証を止めて公開鍵認証に切り替える事をお勧めする。

## ファイル更新があった時に psync を自動実行
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
