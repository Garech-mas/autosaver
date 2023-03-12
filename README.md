# autosaver r2
AviUtlの編集プロジェクトの自動保存

# 仕様
AviUtl version 1.10下で任意のフィルタプラグインを配置可能な場所に`autosaver.auf`を配置することでインストールできる。

起動時、または前回の自動保存時からある時間以上の間隔が空いてから編集中にレンダリングが発生したときに、現在開いている編集プロジェクトの自動保存を実行する。

ある時間は既定では5分であり、`autosaver.auf`と同じディレクトリに生成される`autosaver.json`の`duration`で設定を変更できる。
単位は秒である。

自動保存が行われる先は、編集プロジェクトを保存していればその編集プロジェクトのディレクトリに生成される`autosaver`ディレクトリ、保存していなければ`<autosaver.auf dir>\autosaver`である。

自動保存された編集プロジェクトのファイル名は`%Y-%m-%d-%H-%M-%S.aup`の形式になる。

# 方針
- 設定ファイルは独自に用意する
  - 度々aviutl.iniが吹っ飛ぶバグがあるから
- 上書き保存ではなく新たな編集プロジェクトを作っていくようにする
  - 表に出て来ない編集プロジェクトの破壊が発生したときに過去に戻れなくなる

# 更新履歴
## r2
バージョン判定サボってた
