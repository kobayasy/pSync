[ [特徴](#特徴) | [動作環境](#動作環境) | [インストール](#インストール) | [使い方](#使い方) ] / [技術資料](doc/DEV_ja.md) / [使い方のヒント](doc/NOTE_ja.md)

---

[![Linux](https://github.com/kobayasy/pSync/workflows/Linux/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-linux.yml) [![macOS](https://github.com/kobayasy/pSync/workflows/macOS/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-macos.yml) [![Windows](https://github.com/kobayasy/pSync/workflows/Windows/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-windows.yml) / [![SAST](https://github.com/kobayasy/pSync/workflows/SAST/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/codeql-analysis.yml)

<img src="psync.png" alt="pSync" align="right">

### バージョン3.1リリース (15th of Nov., 2025)
- ファイルのメタ情報送受信を並列化し、高速化しました。
- プロトコル変更により、バージョン2系統との互換性がなくなりました。
	- バージョン2系統のサポートは終了しますので、お早めのバージョンアップをお願いします。
- メタ情報送受信の並列化に伴い、一部のコマンドラインオプション(`--sync`, `-s`, `--put`, `-p`, `--get`, `-g`)を廃止しました。

### 特徴
- オープンソースのクラウドファイル共有システムです。
- ネットワーク接続されたホスト間で、指定したディレクトリ内のファイルを同期します。
	- 複数の同期ディレクトリを指定できます。
	- ホスト間で同期ディレクトリ名を一致させる必要はありません。
	- 同期ディレクトリはラベル名で管理され、同じラベル名を持つディレクトリ同士が同期されます。
	- ディレクトリごとに同期ホストを分けることができます。
- 同期によって削除・更新されたファイルは、変更前のファイルをバックアップとして一定期間保持します。
- 同期元・同期先の区別なく、同じソフトウェアで動作します。
	- クラウドホスト間で同期を中継したり、ローカルホスト間で直接同期することも可能です。
- 通信切断などで処理が中断した場合でも、一部のファイルのみが同期されることはなく、同期前の状態が保たれます。
- ファイルの更新を自動で検出して同期を実行する機能はありません。
	- 編集途中のファイルが意図せず同期されることを防ぎます。
	- ユーザーの任意のタイミングで同期を実行できます。
	- 同期処理中以外はバックグラウンドで動作しないため、リソースを消費しません。
- ホスト間の通信にはSSH接続を使用します。
	- 専用のサーバーをインストールする必要はありません。
		- インストールに管理者権限は不要です。
	- ユーザー認証にはパスワードだけでなく公開鍵も使用できます。
	- 転送データは暗号化・圧縮されます。
- 外部ライブラリは不要で、pthread が利用可能な環境であればビルド・インストールできます。
	- ビルド時に libtinfo または libncurses がインストールされていれば、ファイル同期の進捗をプログレスバー表示する機能が追加されます(必須ではありません)。
- 組み込み機器での動作も考慮した省メモリ設計です。
- 対応しているファイル種別は、通常ファイル、シンボリックリンク、ディレクトリのみです。
	- その他の種別(デバイスファイル、ソケット、FIFO など)が含まれる場合はエラーとなり、同期は行われません。
- ハードリンクは個別のファイルとして扱われ、リンクは維持されません。
- 同期されるメタ情報は、パーミッション(所有者、グループ、その他)、作成日時、更新日時 のみです。
	- その他のメタ情報(ユーザーID、グループID、アクセス日時 など)は同期されず、同期先ホストの環境に依存します。

### 動作環境
- 同期先ホストにSSHサーバがインストールされていること。
- 同期元ホストにSSHクライアントがインストールされていること。
- 同期元ホストから同期先ホストへSSH接続できること。

### インストール
この作業は、ローカルホストを含め、同期する全てのホストで必要です。
1. 以下のコマンドでビルドとインストールを実行します。
```sh
curl -LOJs https://github.com/kobayasy/pSync/releases/download/3.1/psync-3.1.tar.gz
tar xzf psync-3.1.tar.gz
cd psync-3.1
./configure --prefix=$HOME
make install

```
2. `~/bin` にPATHが通っていない場合は、追加してください。
3. 初回のみ、以下の設定が必要です。これは `~/pSync` を同期ディレクトリにする設定例です。
```sh
mkdir ~/pSync && chmod 700 ~/pSync
cat <<. >~/.psync.conf && chmod 600 ~/.psync.conf
#Label Directory
psync  pSync
.

```

### 使い方
1. 以下のコマンドでヘルプが表示されます。
```sh
psync --help

```
2. 例えば `guest@example.com` と同期するには、以下のコマンドを実行します。
```sh
psync guest@example.com

```
