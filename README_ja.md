<!--
README_ja.md - Last modified: 13-Jan-2026 (kobayasy)
-->

[ [特徴](#特徴) | [動作環境](#動作環境) | [インストール](#インストール) | [使い方](#使い方) ] / [技術資料](doc/DEV_ja.md) / [雑記](doc/NOTE_ja.md) // [英語(English)](README_en.md)

---

[![Linux](https://github.com/kobayasy/pSync/workflows/Linux/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-linux.yml) [![macOS](https://github.com/kobayasy/pSync/workflows/macOS/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-macos.yml) [![Windows](https://github.com/kobayasy/pSync/workflows/Windows/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/build-windows.yml) / [![SAST](https://github.com/kobayasy/pSync/workflows/SAST/badge.svg)](https://github.com/kobayasy/pSync/actions/workflows/codeql-analysis.yml)

<img src="psync.png" alt="pSync" align="right">

### 特徴
- SSH接続を利用したオープンソースのファイル同期システムです。
- ネットワーク接続されたホスト間で、指定したディレクトリ内のファイルを同期します。
	- 複数の同期ディレクトリを指定できます。
	- ホスト間で同期ディレクトリ名を一致させる必要はありません。
	- 同期ディレクトリはラベル名で管理され、同じラベル名を持つディレクトリ同士が同期対象となります。
	- ディレクトリごとに同期ホストを分けることができます。
- 同期により削除・更新されたファイルは、変更前の状態がバックアップとして一定期間保持されます。
- 同期元と同期先の区別なく、同じソフトウェアで動作します。
	- クラウドホスト間で同期を中継したり、ローカルホスト間で直接同期することも可能です。
- 通信切断などにより処理が中断した場合でも、同期処理はアトミックに実行され、一部のファイルのみが同期されることはなく、同期前の状態が保たれます。
- ファイルの更新を自動で検出して同期を実行する機能は意図的に搭載していません。
	- これにより、編集途中のファイルが意図せず同期されることを防ぎます。
	- ユーザーの任意のタイミングで同期を実行できます。
	- 同期処理中以外はバックグラウンドで動作しないため、システムリソースを消費しません。
- ホスト間の通信にはSSH接続を使用します。
	- 専用のサーバーソフトウェアをインストールする必要はありません。
		- インストールに管理者権限は不要です。
	- ユーザー認証にはパスワード認証に加え、公開鍵認証も利用可能です。
	- 転送データは暗号化・圧縮されます。
- 外部ライブラリへの依存は最小限で、pthread が利用可能な環境であればビルド・インストールが可能です。
	- ビルド時に libtinfo または libncurses がインストールされていれば、ファイル同期の進捗をプログレスバーで表示する機能が追加されます(必須ではありません)。
- 組み込み機器での動作も考慮した省メモリ設計です。
- 対応しているファイル種別は、通常ファイル、シンボリックリンク、ディレクトリのみです。
	- その他の種別(デバイスファイル、ソケット、FIFO など)が含まれる場合はエラーとなり、同期は行われません。
- ハードリンクは個別のファイルとして扱われ、リンクは維持されません。
- 同期されるメタ情報は、パーミッション(所有者、グループ、その他)、作成日時、更新日時 のみです。
	- その他のメタ情報(ユーザーID、グループID、アクセス日時 など)は同期されず、同期先ホストの環境に依存します。

### 動作環境
- 同期元ホストから同期先ホストへSSH接続できる必要があります。
	- 同期先ホストにSSHサーバーがインストールされている必要があります。
	- 同期元ホストにSSHクライアントがインストールされている必要があります。

### インストール
この作業は、同期を行う全てのホストで必要です。
同期元と同期先で違いはなく同じ作業になります。
1. 以下のコマンドでビルドとインストールを実行します。
```sh
curl -LOJs https://github.com/kobayasy/pSync/releases/download/3.4/psync-3.4.tar.gz
tar xzf psync-3.4.tar.gz
cd psync-3.4
./configure --prefix=$HOME
make install

```
2. `~/bin` にPATHが通っていない場合は、環境変数に追加してください。
3. **初回起動時のみ**、以下の設定が必要です。
これは `~/pSync` を同期ディレクトリとして設定する例です。
同期元と同期先でディレクトリ名は異なっても構いませんが、ラベル名は一致させる必要があります。
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
2. 例として、`guest@example.com` と同期するには、以下のコマンドを実行します。
```sh
psync guest@example.com

```
