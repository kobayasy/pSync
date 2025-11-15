[技術資料](#技術資料) [ [配布ファイル](#配布ファイル) | [通信プロトコル](#通信プロトコル) | [設定ファイル構文](#設定ファイル構文) | [ファイルの同期方向決定アルゴリズム](#ファイルの同期方向決定アルゴリズム) | [進捗状況出力フォーマット](#進捗状況出力フォーマット) | [ファイル同期関数の使い方](#ファイル同期関数の使い方) ] / [README](../README_ja.md)

---

# 技術資料

## 配布ファイル
| ファイル名 | 内容
| -- | --
| [README.txt](../src/README.txt) | インストール手順と簡単な使い方説明
| [psync.h](../src/psync.h)<br>[psync.c](../src/psync.c) | [ファイル同期](#ファイル同期関数の使い方)([同期方向決定](#ファイルの同期方向決定アルゴリズム))
| [psync_psp1.h](../src/psync_psp1.h)<br>[psync_psp1.c](../src/psync_psp1.c) | [通信プロトコル](#通信プロトコル), ファイル同期起動
| [main.c](../src/main.c) | [設定ファイル解析](#設定ファイル構文), 引数解析, 通信プロトコル起動
| [popen3.h](../src/popen3.h)<br>[popen3.c](../src/popen3.c) | プロセス起動, プロセス間通信
| [progress.h](../src/progress.h)<br>[progress.c](../src/progress.c) | [進捗通知](#進捗状況出力フォーマット)
| [info.h](../src/info.h)<br>[info.c](../src/info.c) | [進捗表示](#進捗状況出力フォーマット)
| [tpbar.h](../src/tpbar.h)<br>[tpbar.c](../src/tpbar.c) | プログレスバー表示
| [common.h](../src/common.h)<br>[common.c](../src/common.c) | エラー判定/分岐, 中断判定/分岐, 数値データ[デ]シリアライザ, リスト処理
| ja/ | 日本語manマニュアル
| &emsp;[psync.1.in](../src/ja/psync.1.in) | &emsp;psync.1 の生成元
| &emsp;[psync.conf.5.in](../src/ja/psync.conf.5.in) | &emsp;psync.conf.5 の生成元
| [Makefile.in](../src/Makefile.in) | Makefile の生成元
| [configure.ac](../src/configure.ac) | config.h.in と configure, conf/ の生成元
| config.h.in<br>configure<br>conf/ | autoreconf -i 実行で configure.ac から自動生成される

## 通信プロトコル
```plantuml
/' psp.pu - pSp3 protocol
 ' Last modified: 15-Nov-2025 (kobayasy)
 '/

@startuml
title pSp3 protocol

skin rose
'skinparam BackgroundColor transparent
hide footbox

participant "Local host\n(Master)" as local
participant "Remote host\n(Slave)" as remote

/'
Remote STDIN and STDOUT
｜              Local sequence
｜              ｜              Remote sequence
↓               ↓               ↓ 
 '/
                local -> local: psp_new()
                activate local
                  local --> local
                deactivate local
loop
                local -> local: psp_config()
                activate local
  note over local
    Loads the list of sync directory and label pairs from \~/.psync.conf
  end note
                  local --> local
                deactivate local
end
note over remote
  STDIN: Receives file data
  STDOUT: Sends file data
  STDERR: Sends status data
end note
create remote
  local -->> remote: Connect\n(ssh ... 'psync --remote')
                                remote -> remote: psp_new()
                                activate remote
                                  remote --> remote
                                deactivate remote
  loop
                                remote -> remote: psp_config()
                                activate remote
    note over remote
      Loads the list of sync directory and label pairs from \~/.psync.conf
    end note
                                  remote --> remote
                                deactivate remote
  end
                local -> local: psp_run()
                activate local
                                remote -> remote: psp_run()
                                activate remote
  par
    local ->> remote: Protocol ID: 'pSp3'
  else
    local <<- remote: Protocol ID: 'pSp3'
  end
  par
        local ->> remote: List of local labels
  else
        local <<- remote: List of remote labels
  end
  loop Process directories for all matching labels on both Local and Remote
                  local -> local: psync_new()
                  activate local
                                  remote -> remote: psync_new()
                                  activate remote
    note over local, remote
      Acquire an exclusive lock on the sync directory
    end note
                    local --> local
                  deactivate local
                                    remote --> remote
                                  deactivate remote
    par
      local ->> remote: ACK (Local sync directory is ready)
    else
      local <<- remote: ACK (Remote sync directory is ready)
    end
                  local -> local: psync_run()
                  activate local
                                  remote -> remote: psync_run()
                                  activate remote
    note over local, remote
      Create a file list within the sync directory
    end note
    par
            local ->> remote: Local file list

    else
            local <<- remote: Remote file list
    end
    note over local, remote
      Create a post-sync file list from the Local and Remote file lists
    end note
    note over local, remote
      Copy files to be uploaded to a temporary upload location
    end note
    par
      local ->> remote: Upload files from the temporary upload location
    else
      local <<- remote: Download files to a temporary download location
    end
    note over local, remote
      Apply files from the temporary download location to the sync directory
    end note
                    local --> local
                  deactivate local
                                    remote --> remote
                                  deactivate remote
                  local -> local: psync_free()
                  activate local
                                  remote -> remote: psync_free()
                                  activate remote
    note over local, remote
      Release the exclusive lock on the sync directory
    end note
                    local --> local
                  deactivate local
                                    remote --> remote
                                  deactivate remote
  end
                  local --> local
                deactivate local
                                  remote --> remote
                                deactivate remote
                                remote -> remote: psp_free()
                                activate remote
                                  remote --> remote
                                deactivate remote
  local -->> remote: Disconnect
destroy remote
                local -> local: psp_free()
                activate local
                  local --> local
                deactivate local

@enduml
```

%%
![pSp1 protocol](psp.svg)
%%

## 設定ファイル構文
```plantuml
/' psyncConf.pu - Configuration file syntax
 ' Last modified: 15-Nov-2025 (kobayasy)
 '/

@startebnf
title Configuration file syntax

skin rose
'skinparam BackgroundColor transparent

ConfigFile={{Whitespace},[Config,{Whitespace}],["#",CommentText],"\n"};
Config=(Label(*Must not contain Whitespace, '#' or '='*),{Whitespace}-,SyncDirectory(*Must not contain '#'*)|("expire"|"backup"),"=","1-9",{"0-9"});
Whitespace="\x20"|"\t"|"\r"|"\n"|"\v"|"\f";

@endebnf
```

%%
![psync conf](psyncConf.svg)
%%

## ファイルの同期方向決定アルゴリズム
```plantuml
/' psyncMode.pu - ファイルの同期方向決定アルゴリズム(pSp3)
 ' Last modified: 15-Nov-2025 (kobayasy)
 '/

@startuml
title ファイルの同期方向決定アルゴリズム(pSp3)

skin rose
'skinparam BackgroundColor transparent

start
floating note: make_fsynced_func()
  switch (Local と Remote の\nファイルを比較)
  case (両方に存在)
    switch (Local と Remote の\n更新日時を比較)
    case (Remote の方が新しい)
      :Local のファイル内容を\nRemote の内容に更新;
    case (Local の方が新しい)
      :Remote のファイル内容を\nLocal の内容に更新;
    case (同じ)
    endswitch
  case (Local にのみ存在)
    if (Local の更新日時と\nRemote の削除日時を比較) then (Remote の方が新しい)
      :Local のファイルを削除;
    else (Remote の削除記録がない,\nLocal の方が新しい or 同時)
      :Local のファイルを\nRemote へ追加;
    endif
  case (Remote にのみ存在)
    if (Remote の更新日時と\nLocal の削除日時を比較) then (Local の方が新しい)
      :Remote のファイルを削除;
    else (Local の削除記録がない,\nRemote の方が新しい or 同時)
      :Remote のファイルを\nLocal へ追加;
    endif
  endswitch
end

@enduml
```

%%
![psync mode](psyncMode.svg)
%%

## 進捗状況出力フォーマット
```plantuml
/' psyncInfo.pu - 進捗状況出力フォーマット
 ' Last modified: 15-Nov-2025 (kobayasy)
 '/

@startebnf
title 進捗状況出力フォーマット

skin rose
'skinparam BackgroundColor transparent

(*info にファイルディスクリプタを設定すると同期処理の進捗状況がASCII文字列でリアルタイムに出力される*)
進捗状況={"["(*ディレクトリの同期開始*),ディレクトリ識別名,"\n",{("S"(*同期対象ファイルの検索中*),"+",検索済み同期対象ファイル数|"U"(*同期ファイルのアップロード準備中*),"+",アップロードするファイルサイズの合計|"D"(*同期ファイルのダウンロード中*),"+",ダウンロード済みファイルサイズの合計|"R"(*ファイルの削除中*),"+",削除済みファイル数|"C"(*ファイルの更新中*),"+",更新済みファイル数|"!"(*エラー発生*),(エラー内容を示す文字列(*先頭文字が + か - 以外*)|((("+"(*処理の継続が可能*)|"-"(*処理の継続が不可能*))),エラーコード))),"\n"}-,"]"(*ディレクトリの同期完了*),"\n"}-;

@endebnf
```

%%
![psync info](psyncInfo.svg)
%%

## ファイル同期関数の使い方
- 使い方は簡単。 同期元と相手のディレクトリに対してそれぞれ psync_new(), psync_run(), psync_free() を順番に呼び出すだけ。
- 下記はカレントディレクトリの dir1/ と dir2/ 内のファイルを同期するサンプルコード。 説明用に エラー処理、中断処理、進捗表示 を省いている。
- psync_run() 呼び出し前に同期元の fdout を相手の fdin に、相手の fdout を元の fdin にそれぞれファイルディスクリプタで繋ぐ。 psync コマンドは SSH の標準入出力で繋いでいるが手段は何でも良い。 この例ではパイプで繋いでいる。
- psync_run() は同期元と相手の間でファイルデータ受け渡しの為に並列実行が必要。 psync コマンドでは別PC動作で並列実行しているが手段は何でも良い。 この例では pthread を使っている。
```c
/* psync_example.c - ファイル同期関数の使い方サンプルコード
 */

#include <pthread.h>  /* pthread_create() pthread_join() */
#include <unistd.h>   /* pipe() close() */
#include "psync.h"    /* psync_new() psync_run() psync_free() */

typedef struct {
    PSYNC *psync;
    pthread_t tid;
} PARAM;

static void *run_thread(void *data)
{
    PARAM *param = data;

    psync_run(param->psync);
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
    param1.psync->fdin = pipe1[0];
    param1.psync->fdout = pipe2[1];
    param2.psync = psync_new("dir2", NULL);
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
```sh
gcc -pthread -o psync_example psync_example.c psync.c common.c progress.c

```

下記はファイル同期の実行例。
- 実行の結果 dir1/ と dir2/ の内容が同期されて同じになる。
```sh
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
