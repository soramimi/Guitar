C/Migemoライブラリ説明書
                                                            Since: 15-Aug-2001
                                                                Version: 1.2.0
                                                  Author: MURAOKA Taro (KoRoN)
                                                     Last Change: 02-Jan-2013.

説明
  C/MigemoはMigemo(Ruby/Migemo)をC言語で実装したものです。C/Migemoライブラリを
  利用するソフトウェアは「ローマ字のまま日本語を(インクリメンタルに)検索する」
  機能を持つことが可能になります。C言語で実装したことにより、本家Ruby/Migemoに
  比べ、C言語で書かれた多くのソフトウェアからの利用が容易になること、及び(たぶ
  ん)実行速度の向上が期待できます。

  またC/Migemoを利用するためには別途辞書ファイル(dict/migemo-dict)を作成もしく
  は入手する必要があります。自分でまったくのゼロから辞書を作成することもできま
  すが、Ruby/Migemoのmigemo-dictを流用したり、SKKの辞書からコンバートすること
  も可能です。詳しくは下記の「辞書について」のセクションを参照してください。

  Ruby/Migemoは高林 哲さんが考案した「ローマ字入力を日本語検索のための正規表現
  に変換する」ツールです。Emacs上でローマ字のまま日本語をインクリメンタル検索
  するために、RubyとEmacs LISPで実装されていました。

  - 本家Ruby/Migemoサイト (Migemoに関する詳細情報)
      http://migemo.namazu.org/
  - C/Migemo製作・配布サイト (C/Migemoの情報)
      http://www.kaoriya.net/

ファイル構成
  C/Migemoのパッケージは現在のところ次のようなファイル・ディレクトリから構成さ
  れています。
  (ディレクトリ)
    compile/            :各プラットホーム用のメイクファイル置き場
    compile/vs6         :VS6用プロジェクト
    compile/vs2003      :VS2003用プロジェクト
    doc/                :ドキュメント置き場
    dict/               :辞書置き場
    tools/              :各種ツール
    src/                :ソースファイル
  (ファイル)
    Makefile            :統合メイクファイル
    README.txt          :各ドキュメントへのポインタ
    VERSION             :バージョン番号
    config.mk           :デフォルトコンフィギュレーション
  (ソースコード) src/内
    depend.mak          :構成ファイルと依存関係
    migemo.h            :ライブラリを利用するのためのインクルードファイル
    main.c              :ライブラリを利用するサンプルプログラム(cmigemo)
    *.c                 :ライブラリのソース
    *.h                 :ライブラリのソース用インクルードファイル

コンパイル方法
  コンパイルには以下の3段階の手順があります。
    1. ライブラリとプログラムのビルド
    2. 辞書ファイルのビルド
    3. インストール
  「辞書ファイルのビルド」には別途以下の4種が必要になります。
    1. Perl
    2. インターネットアクセスプログラム(cURL, wget, fetchの内どれか1つ)
    3. 解凍伸長プログラムgzip
    4. エンコード変換プログラム(nkf, qkc)
  これらを含むC/Migemoのビルドに必要な外部プログラムはconfig.mkを編集すること
  で変更可能です。また以下のプラットホーム別のビルド方法で利用しているように
  UNIXライクな環境では自動的にconfig.mkを修正する仕組み(configure)を利用できま
  す。config.mkのオリジナルファイルはcompile/config_default.mkとして収録してあ
  るのでいつでも復元可能です。

  各プラットホームでのビルド方法の具体例は以下を参照してください。
    1. Windows + VisualC++
    2. Windows + Cygwin
    3. Windows + Borland C++
    4. MacOS X + Developer Tools
    5. GNU/gcc:Linux他
  現在のところ以上5環境で動作の確認をしています。5.のGNU/gccについてはFreeBSD
  5.0です。

  (Windows + VisualC++)
  次のコマンドでRelease/内にmigemo.dllとcmigemo.exeが作成されます。
    > nmake msvc
  必要な外部プログラム、ネットワーク接続が揃っていれば
    > nmake msvc-dict
  で辞書ファイルをビルドできます。migemo.dswをVC++6.0で開き、ビルドする方法も
  あります。以上が終了すれば次のコマンドでテストプログラムが動作します。
    > .\build\cmigemo -d dict/migemo-dict

  (Windows + Cygwin)
  必要な外部プログラム、ネットワーク接続を揃えて以下を実行することでテストプロ
  グラムcmigemoと辞書ファイルがビルドされます:
    $ ./configure
    $ make cyg
    $ make cyg-dict
  実行はcp932(Shift-JIS)で利用するならば:
    $ ./build/cmigemo -d dict/migemo-dict
  euc-jpで利用するならば:
    $ ./build/cmigemo -d dict/euc-jp.d/migemo-dict
  を実行します。インストールとアンインストールはroot権限で次のコマンドを実行す
  ることで行なえます。インストール場所についてはconfig.mkを参照してください。
    # make cyg-install
    # make cyg-uninstall

  (Windows + Borland C++)
  次のコマンドでmigemo.dllとcmigemo.exeが作成されます。
    > make bc
  必要な外部プログラム、ネットワーク接続が揃っていれば
    > nmake bc-dict
  で辞書ファイルをビルドできます。以上が終了すれば次のコマンドでテストプログラ
  ムが動作します。
    > .\build\cmigemo -d dict/migemo-dict

  (MacOS X + Developer Tools)
  必要な外部プログラム、ネットワーク接続を揃えて以下を実行することでテストプロ
  グラムcmigemoと辞書ファイルがビルドされます:
    % ./configure
    % make osx
    % make osx-dict
  実行はcp932(Shift-JIS)で利用するならば:
    % ./build/cmigemo -d dict/migemo-dict
  euc-jpで利用するならば:
    % ./build/cmigemo -d dict/euc-jp.d/migemo-dict
  を実行します。インストールとアンインストールはroot権限で次のコマンドを実行す
  ることで行なえます。インストール場所についてはconfig.mkを参照してください。
    # make osx-install
    # make osx-uninstall
  インストールにより辞書ファイルは
    /usr/local/share/migemo/{エンコード名}
  に置かれます。

  (GNU/gcc:Linux他)
  必要な外部プログラム、ネットワーク接続を揃えて以下を実行することでテストプロ
  グラムcmigemoと辞書ファイルがビルドされます:
    $ ./configure
    $ make gcc
    $ make gcc-dict
  実行はcp932(Shift-JIS)で利用するならば:
    $ ./build/cmigemo -d dict/migemo-dict
  euc-jpで利用するならば:
    $ ./build/cmigemo -d dict/euc-jp.d/migemo-dict
  を実行します。インストールとアンインストールはroot権限で次のコマンドを実行す
  ることで行なえます。インストール場所についてはconfig.mkを参照してください。
    # make gcc-install
    # make gcc-uninstall
  インストールにより辞書ファイルは
    /usr/local/share/migemo/{エンコード名}
  に置かれます。

利用許諾条件
  C/MigemoはMITライセンスと独自ライセンスのデュアルライセンスとして配布されて
  います。利用者は都合に応じて、より適合するライセンスを選択して利用してくださ
  い。

  MITライセンスについてはdoc/LICENSE_MIT.txtを参照してください。
  独自ラインセンスについてはdoc/LICENSE_j.txtを参照してください。

辞書の著作権・利用許諾条件
  C/Migemoで利用する辞書の諸権利、利用許諾条件及び著作権等はその辞書の定めると
  ころに従います。C/MigemoはSKK辞書を用いるように初期設定されていますが、SKK辞
  書の利用許諾条件はC/Migemoのそれとは法的に別であることに注意してください。

質問・連絡先
  C/Migemoに関する質問・要望等は村岡(下記アドレス参照)まで連絡してください。ソ
  フトウェアからC/Migemoを使用したい場合の問い合わせも受け付けます。

謝辞
  Migemoを発案されRuby/Migemoを作成され、C/Migemoについての相談にMLで親切に答
  えていただいた高林 哲さんに感謝いたします。また、この配布パッケージには以下
  の方々によるドキュメントやアイデアが含まれています。ありがとうございます。

  (アルファベット順)
  - AIDA Shinra
    CygwinとLinux用Makefileの基礎
  - FUJISHIMA Hiroshi <pooh@nature.tsukuba.ac.jp>
    Solaris用Makefileの提供
  - MATSUMOTO Yasuhiro
    Borland C++用Makefileの基礎
    migemo.vimをcmigemoに対応
    VB用サンプル
  - SUNAOKA Norifumi
    Solaris用Makefileの不具合報告
  - TASAKA Mamoru
    MITライセンス版のリリースに協力
  - gageas (https://twitter.com/gageas)
    Migemo.csの修正&改良


付録

辞書について {{{1
  C/Migemoではローマ字を日本語へ変換するのに辞書ファイルdict/migemo-dictを必要
  とします。ファイルdict/migemo-dictはこのアーカイブに含まれていませんが、辞書
  ファイルのビルド時にインターネットを通じて自動的にダウンロードする仕組みにな
  っています。また大変ですが自分で0から作成することも可能です。

  辞書に関するツール、その使用法はdict/Makefileを参照してください。

  C/Migemoでは
    1. dict/migemo-dict以外にも、
    2. ローマ字を平仮名に変換するためのファイル(dict/roma2hira.dat)や、
    3. 平仮名を片仮名に変換するためのファイル(dict/hira2kata.dat)や、
    4. 半角文字を全角文字に変換するためのファイル(dict/han2zen.dat)
    5. 全角文字を半角文字に変換するためのファイル(dict/zen2han.dat)
  を使用しています。これらの全てのファイルは単にデータテーブルとして機能してい
  るだけでなく、システムのエンコード(漢字コード)の違いを吸収する役割も担ってい
  ます。つまり先に挙げた4ファイルをWindowsで使う場合にはcp932に、UNIXやLinuxで
  使う場合にはeuc-jpに変換する必要があるのです。変換が必要だと予想される場合に
  は、辞書ファイルのビルドと同時に自動的に行なわれるようになっています。

  - Migemo DLL配布ページ (cp932のmigemo-dictが入手可能)
      http://www.kaoriya.net/
  - Ruby/Migemo (euc-jpのmigemo-dictが入手可能)
      http://migemo.namazu.org/
  - SKK Openlab (最新のSKK-JISYO.Lが入手可能)
      http://openlab.ring.gr.jp/skk/index-j.html

型リファレンス {{{1
  C/Migemoで利用される型について述べる。

- migemo*;
    Migemoオブジェクト。migemo_open()で作成され、migemo_close()で破棄される。

- int (*MIGEMO_PROC_CHAR2INT)(const unsigned char*, unsigned int*);
    バイト列(unsigned char*)を文字コード(unsigned int)に変換するプロシージャ
    型。Shift-JISやEUC-JP以外のエンコードの文字列を扱うとき、もしくは特殊文字
    の処理を行いたいときに定義する。戻り値は文字列のうち処理したバイト数で、0
    を返せばデフォルトのプロシージャが実行される。この仕組みで必要な文字だけに
    処理を施すことが出来る。

- int (*MIGEMO_PROC_INT2CHAR)(unsigned int, unsigned char*);
    コード(unsigned int)をバイト列(unsigned char*)に変換するプロシージャ型。
    Shift-JISやEUC-JP以外のエンコード文字列を扱うとき、もしくは特殊文字の処理
    を行いたいときに定義する。戻り値は出力された文字列のバイト数で、0を返せば
    デフォルトのプロシージャが実行される。この仕組みで必要な文字だけに処理を施
    すことが出来る。

関数リファレンス {{{1
  C/Migemoライブラリで提供されるAPIを以下で解説する。実際の使用例はアーカイブ
  に含まれるmain.cを参照のこと。

- migemo* migemo_open(const char* dict);
    Migemoオブジェクトを作成する。作成に成功するとオブジェクトが戻り値として返
    り、失敗するとNULLが返る。dictで指定したファイルがmigemo-dict辞書としてオ
    ブジェクト作成時に読み込まれる。辞書と同じディレクトリに:
      1. roma2hira.dat  (ローマ字→平仮名変換表)
      2. hira2kata.dat  (平仮名→カタカナ変換表)
      3. han2zen.dat    (半角→全角変換表)
      3. zen2han.dat    (全角→半角変換表)
    という名前のファイルが存在すれば、存在したものだけが読み込まれる。dictに
    NULLを指定した場合には、辞書を含めていかなるファイルも読み込まれない。ファ
    イルはオブジェクト作成後にもmigemo_load()関数を使用することで追加読み込み
    ができる。

- void migemo_close(migemo* object);
    Migemoオブジェクトを破棄し、使用していたリソースを解放する。

- unsigned char* migemo_query(migemo* object, const unsigned char* query);
    queryで与えられた文字列(ローマ字)を日本語検索のための正規表現へ変換する。
    戻り値は変換された結果の文字列(正規表現)で、使用後はmigemo_release()関数へ
    渡すことで解放しなければならない。

- void migemo_release(migemo* object, unsigned char* string);
    使い終わったmigemo_query()関数で得られた正規表現を解放する。

- int migemo_load(migemo* obj, int dict_id, const char* dict_file);
    Migemoオブジェクトに辞書、またはデータファイルを追加読み込みする。
    dict_fileは読み込むファイル名を指定する。dict_idは読み込む辞書・データの種
    類を指定するもので以下のうちどれか一つを指定する:

      MIGEMO_DICTID_MIGEMO      mikgemo-dict辞書
      MIGEMO_DICTID_ROMA2HIRA   ローマ字→平仮名変換表
      MIGEMO_DICTID_HIRA2KATA   平仮名→カタカナ変換表
      MIGEMO_DICTID_HAN2ZEN     半角→全角変換表
      MIGEMO_DICTID_ZEN2HAN     全角→半角変換表

    戻り値は実際に読み込んだ種類を示し、上記の他に読み込みに失敗したことを示す
    次の価が返ることがある。
      MIGEMO_DICTID_INVALID     

- int migemo_is_enable(migemo* obj);
    Migemoオブジェクトにmigemo_dictが読み込めているかをチェックする。有効な
    migemo_dictを読み込めて内部に変換テーブルが構築できていれば0以外(TRUE)を、
    構築できていないときには0(FALSE)を返す。

- int migemo_set_operator(migemo* object, int index, const unsigned char* op);
    Migemoオブジェクトが生成する正規表現に使用するメタ文字(演算子)を指定する。
    indexでどのメタ文字かを指定し、opで置き換える。indexには以下の値が指定可能
    である:

      MIGEMO_OPINDEX_OR
        論理和。デフォルトは "|" 。vimで利用する際は "\|" 。
      MIGEMO_OPINDEX_NEST_IN
        グルーピングに用いる開き括弧。デフォルトは "(" 。vimではレジスタ\1～\9
        に記憶させないようにするために "\%(" を用いる。Perlでも同様のことを目
        論むならば "(?:" が使用可能。
      MIGEMO_OPINDEX_NEST_OUT
        グルーピングの終了を表す閉じ括弧。デフォルトでは ")" 。vimでは "\)" 。
      MIGEMO_OPINDEX_SELECT_IN
        選択の開始を表す開き角括弧。デフォルトでは "[" 。
      MIGEMO_OPINDEX_SELECT_OUT
        選択の終了を表す閉じ角括弧。デフォルトでは "]" 。
      MIGEMO_OPINDEX_NEWLINE
        各文字の間に挿入される「0個以上の空白もしくは改行にマッチする」パター
        ン。デフォルトでは "" であり設定されない。vimでは "\_s*" を指定する。

    デフォルトのメタ文字は特に断りがない限りPerlのそれと同じ意味である。設定に
    成功すると戻り値は1(0以外)となり、失敗すると0になる。

- const unsigned char* migemo_get_operator(migemo* object, int index);
    Migemoオブジェクトが生成する正規表現に使用しているメタ文字(演算子)を取得す
    る。indexについてはmigemo_set_operator()関数を参照。戻り値にはindexの指定
    が正しければメタ文字を格納した文字列へのポインタが、不正であればNULLが返
    る。

- void migemo_setproc_char2int(migemo* object, MIGEMO_PROC_CHAR2INT proc);
    Migemoオブジェクトにコード変換用のプロシージャを設定する。プロシージャにつ
    いての詳細は「型リファレンス」セクションのMIGEMO_PROC_CHAR2INTを参照。

- void migemo_setproc_int2char(migemo* object, MIGEMO_PROC_INT2CHAR proc);
    Migemoオブジェクトにコード変換用のプロシージャを設定する。プロシージャにつ
    いての詳細は「型リファレンス」セクションのMIGEMO_PROC_INT2CHARを参照。

コーディングサンプル {{{1
  C/Migemoを利用したコーディング例を示す。以下のサンプルは一切のエラー処理を行
  なっていないので、実際の利用時には注意が必要。
    #include <stdio.h>
    #include "migemo.h"
    int main(int argc, char** argv)
    {
        migemo *m;
        /* C/Migemoの準備: 辞書読込にはエラー検出のためloadを推奨 */
        m = migemo_open(NULL);
        migemo_load(m, MIGEMO_DICTID_MIGEMO, "./dict/migemo-dict");
        /* 必要な回数だけqueryを行なう */
        {
            unsigned char* p;
            p = migemo_query(m, "nezu");
            printf("C/Migemo: %s\n", p);
            migemo_release(m, p); /* queryの結果は必ずreleaseする */
        }
        /* 利用し終わったmigemoオブジェクトはcloseする */
        migemo_close(m);
        return 0;
    }

更新箇所 {{{1
  ● (1.3 開発版)
    VC9(VisualStudio2008)での64bit版のコンパイルに対応
    VC9(VisualStudio2008)でのコンパイルに対応
    ローマ字が子音で終わる際に「xtu{子音}{母音}」を加えるように変更
    configureの--prefixが機能していなかった問題の修正
    生queryでの辞書検索をignore caseに
    英単語のエントリを小文字で正規化
    VB用サンプル(tools/clsMigemo.cls)を追加
    パターンの生成を高速化(rxget.c:rxgen_add)
    VS2003用プロジェクトファイル追加
    全角→半角変換を追加
    全角→半角辞書を追加
    ローマ字変換でマルチバイト文字を考慮(Vim掲示板:1281)
    CP932/EUCJP/UTF8のエンコード自動判別機能を追加
    migemo.vimをcmigemo.exe対応の改良版に差し替え
    ヘルプメッセージ内の余分な空白を削除
  ● 29-Dec-2003 (1.2 正式版)
    半角大文字を全角大文字に変換できないバグを修正
    リリース用にドキュメントを修正
    configureスクリプトを導入
    Doxygenを導入する
    queryそのもので辞書を引くのを忘れていたのを修正
    tools/Migemo.cs:体裁修正
    ビルドディレクトリを変更(動作確認済:mvc,cyg,gcc,bc5)
    (1.1.023)migemo.c:EXPORTSの削除他
    (1.1.022)dict/roma2hira.datに~→～の変換を追加
    (1.1.021)Windows用Makefileを修正
    (1.1.020)romaji.cのテストコードを分離
    (1.1.019)辞書読み込み時のエラーを不完全ながら厳密化
    (1.1.018)roma2hira.datにヘボン式のm[bmp]及びtch[aiueo]を追加
    (1.1.017)利用許諾条件を整理
    (1.1.016)cmigemoをサブ辞書(ユーザ辞書)に対応
    (1.1.015)cmigemo出力後にfflush()を追加
    (1.1.014)MSVC用プロジェクトファイルをmsvc/以下へ移動
    (1.1.013)cmigemoに--emacs/-e及び--nonewline/-nを追加
    (1.1.012)mkpkgを改良
    (1.1.011)dict/roma2hira.datに[bp]y[aiueo]のエントリを追加
    (1.1.010)migemo.vimをcmigemoに対応(by.まっつんさん)
    (1.1.009)Solarisでコンパイルできるようにするための変更
    (1.1.008)SKK辞書の配布形態変更に追従
    (1.1.007)デバッグメッセージをコメントにより削除
    (1.1.006)config.mkに設定例をコメントとして追加
    (1.1.005)C#用サンプルtools/*.csを追加
    (1.1.004)連文節に対応(β版扱い)
    (1.1.003)Solaris用メイクファイルMake_sun.makを追加、未動作チェック
    (1.1.002)辞書の1行に複数エントリを出力できなくなっていた問題を修正
    (1.1.001)migemo.vimをUNIX系に対応
  ● 27-May-2002 (1.1 正式版)
    正式版リリース
    ドキュメントブラッシュアップ
    Makefile修正:releaseが./Releaseのためにビルドできなかった問題
  ● 16-May-2002 (1.1-beta2)
    BC5対応のためブチ切れ寸前…(_ _;;;
    Makefileの構造変更(BC5対応のため)
    謝辞追加
    パッケージ作成用スクリプトの追加
    cmigemoをプログラムから使いやすく。
  ● 15-May-2002 (1.1-beta1)
    migemo_set_operator()の戻り値の意味を変更
    ドキュメントブラッシュアップ
    Cygwin/MacOS X/Linux用にMakefileを作成
    strip.plとlensort.plをtool/optimize-dict.plに統合
    tool/conv.plをtool/skk2migemodict.plに名称変更
    ドキュメント修正
    cacheを最適化して高速化 (mnode_load())
    wordbuf_add()を最適化して高速化 (wordbuf.c)
    migemo辞書読み込み失敗を検出できなくなっていたバグ修正
    mnodeをまとめて確保することで高速化 (mnode.c他)
    辞書を長さ降順にソート (tools/lensort.pl)
    起動を1割から2割高速化 (mnode.c)
  ● 21-Aug-2001
    main.cのgets()をfgets()に変更
}}}

-------------------------------------------------------------------------------
                  生きる事への強い意志が同時に自分と異なる生命をも尊ぶ心となる
                                    MURAOKA Taro/村岡太郎<koron@tka.att.ne.jp>

 vi:set ts=8 sts=2 sw=2 tw=78 et fdm=marker ft=memo:
