[使い方のヒント](#使い方のヒント) [ [パスワード入力の自動](#パスワード入力の自動化) | [ファイル更新があった時にpsyncを自動実行](#ファイル更新があった時にpsyncを自動実行) ] / [README](../README_ja.md)

---

# 使い方のヒント

## パスワード入力の自動化
バックグラウンドで psync を自動実行させる場合、パスワード入力をどうやって自動化させるかが問題になります。
そんな時は、ssh 用パスワード自動入力コマンドの sshpass で解決できます。
例えば、同期先のホスト名が `example.com`、ユーザー名が `guest` でパスワードが `password` の場合、下記のように実行します。
```sh
shpass -p password psync guest@example.com

```
sshpass の man にも記載があるように、この方法は設定ファイルやスクリプトファイルに生のパスワードを書く必要があり、セキュリティ上危険なため、パスワード認証を止めて公開鍵認証に切り替えることをお勧めします。

## ファイル更新があった時にpsyncを自動実行
ファイルアクセスを監視し、更新のタイミングでコマンドを実行するツールと組み合わせることで、ファイル更新時に psync を自動実行させることが可能です。
例えば、Linux の場合だと inotify と組み合わせて、下記のスクリプトで自動実行ができます。
SYNCDIR、SYNCCMD、BUFFTIM は、お使いの環境に合わせて書き換えてください。
```sh
#! /bin/sh

SYNCDIR=~/pSync
SYNCCMD='sshpass -p password psync guest@example.com'
BUFFTIM=10000

$SYNCCMD && exec inotify-hookable -w "`realpath $SYNCDIR`" -i .psync -c "$SYNCCMD" -t "$BUFFTIM" -q
```
