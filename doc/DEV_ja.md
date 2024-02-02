[技術資料](#技術資料)
[
[配布ファイル](#配布ファイル)
|
[通信プロトコル](#通信プロトコル)
|
[ファイル同期関数の使い方](#ファイル同期関数の使い方)
]
*|*
[README](../README_ja.md)

***

# 技術資料

## 配布ファイル
|ファイル名|内容|
---|---
|README.txt|インストール手順と簡単な使い方説明|
|psync.{c,h}|ファイル同期|
|psync_psp1.{c,h}|通信プロトコル, ファイル同期起動|
|main.c|設定ファイル解析, 引数解析, 通信プロトコル起動|
|popen3.{c,h}|プロセス起動, プロセス間通信|
|progress.{c,h}|進捗通知|
|tpbar.{c,h}|プロブレスバー表示|
|info.{c,h}|進捗表示|
|common.{c,h}|エラー判定/分岐, 中断判定/分岐, 数値データ[デ]シリアライザ, リスト処理|
|ja/|日本語manマニュアル|
|　psync.1.in|　psync.1 の生成元|
|　psync.conf.5.in|　psync.conf.5 の生成元|
|Makefile.in|Makefile の生成元|
|configure.ac|config.h.in と configure, conf/ の生成元|
|config.h.in|autoreconf -i 実行で configure.ac から自動生成される|
|configure|(同上)|
|conf/|(同上)|

## 通信プロトコル
![pSp1 protocol](psp.svg)

## ファイル同期関数の使い方
* 使い方は簡単。
同期元と相手のディレクトリに対してそれぞれ psync_new(), psync_run(), psync_free() を順番に呼び出すだけ。
* 下記はカレントディレクトリの dir1/ と dir2/ 内のファイルを同期するサンプルコード。
説明用に エラー処理、中断処理、進捗表示 を省いている。
* psync_run() 呼び出し前に同期元の fdout を相手の fdin に、相手の fdout を元の fdin にそれぞれファイルディスクリプタで繋ぐ。
psync コマンドは SSH の標準入出力で繋いでいるが手段は何でも良い。
この例ではパイプで繋いでいる。
* 同期元と相手とで psync_run() の引数は PSYNC_MASTER と PSYNC_SLAVE で異なる値に設定する。
* psync_run() は同期元と相手の間でファイルデータ受け渡しの為に並列実行が必要。
psync コマンドでは別PC動作で並列実行しているが手段は何でも良い。
この例では pthread を使っている。
```
/* psync_example.c - ファイル同期関数の使い方サンプルコード
 */

#include <pthread.h>  /* pthread_create() pthread_join() */
#include <unistd.h>   /* pipe() close() */
#include "psync.h"    /* psync_new() psync_run() psync_free() */

typedef struct {
    PSYNC *psync;
    PSYNC_MODE mode;
    pthread_t tid;
} PARAM;

static void *run_thread(void *data)
{
    PARAM *param = data;

    psync_run(param->mode, param->psync);
    return NULL;
}

int main(int argc, char *argv[])
{
    PARAM param1;
    PARAM param2;
    int pipe1[2];
    int pipe2[2];

    pipe(pipe1);
    pipe(pipe2);
    param1.psync = psync_new("dir1", NULL);
    param1.mode = PSYNC_MASTER;
    param1.psync->fdin = pipe1[0];
    param1.psync->fdout = pipe2[1];
    param2.psync = psync_new("dir2", NULL);
    param2.mode = PSYNC_SLAVE;
    param2.psync->fdin = pipe2[0];
    param2.psync->fdout = pipe1[1];
    pthread_create(&param1.tid, NULL, run_thread, &param1);
    pthread_create(&param2.tid, NULL, run_thread, &param2);
    pthread_join(param1.tid, NULL);
    pthread_join(param2.tid, NULL);
    psync_free(param1.psync);
    psync_free(param2.psync);
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);
    return 0;
}
```
上記サンプルコードと pSync 配布ファイルの psync.c, psync.h, common.c, common.h, progress.c, progress.h を pthread を有効にして ビルド & リンク する。
GCC だとバージョンにもよるけど下記で良いと思う。
```
gcc -pthread -o psync_example psync_example.c psync.c common.c progress.c
```
下記はファイル同期の実行例。
* 実行の結果 dir1/ と dair2/ の内容が同期されて同じになる。
```
$ mkdir dir1 dir2
$ echo foo > dir1/file1
$ echo bar > dir1/file2
$ echo baz > dir2/file3
$ echo qux > dir2/file4
$ ls dir1 dir2
dir1:
file1   file2

dir2:
file3   file4
$ ./psync_example
$ ls dir1 dir2
dir1:
file1   file2   file3   file4

dir2:
file1   file2   file3   file4
$ cat dir1/file*
foo
bar
baz
qux
$ cat dir2/file*
foo
bar
baz
qux
$ echo quux > dir1/file5
$ echo FOO > dir2/file1
$ ls dir1 dir2
dir1:
file1   file2   file3   file4   file5

dir2:
file1   file2   file3   file4
$ cat dir1/file*
foo
bar
baz
qux
quux
$ cat dir2/file*
FOO
bar
baz
qux
$ ./psync_example
$ ls dir1 dir2
dir1:
file1   file2   file3   file4   file5

dir2:
file1   file2   file3   file4   file5
$ cat dir1/file*
FOO
bar
baz
qux
quux
$ cat dir2/file*
FOO
bar
baz
qux
quux
$
```
