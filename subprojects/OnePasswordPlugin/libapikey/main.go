// libapikey パッケージ
//
// このパッケージは 1Password SDK を使ってシークレット（APIキー等）を取得し、
// CGo 経由でC/C++から呼び出せる共有ライブラリ（libapikey.so）としてビルドされる。
//
// ビルド方法:
//
//	go build -buildmode=c-shared -o libapikey.so
package main

/*
// C の free() を使用するために stdlib.h をインクルードする
#include <stdlib.h>
*/
import "C"

import (
	"context"
	"unsafe"

	// 1Password 公式 Go SDK
	"github.com/1password/onepassword-sdk-go"
)

// GetAPIKEY は 1Password Desktop App Integration を使ってシークレットを取得し、
// C文字列として返す。
//
// 引数:
//
//	account_name   - 1Password のアカウント表示名（例: "John Doe"）
//	secret_ref_uri - シークレット参照 URI（例: "op://API_KEY/Example/credential"）
//	                 URI の形式: op://<vault名>/<アイテム名>/<フィールド名>
//
// 戻り値:
//
//	C.malloc で確保されたシークレット文字列のポインタ。
//	エラー時は空文字列（""）のポインタを返す。
//	呼び出し側は使用後に必ず FreeString() で解放すること。
//
//export GetAPIKEY
func GetAPIKEY(account_name *C.char, secret_ref_uri *C.char) *C.char {

	// C文字列をGoの文字列型に変換する
	accname := C.GoString(account_name)
	uri := C.GoString(secret_ref_uri)

	// 1Password クライアントを初期化する
	// WithDesktopAppIntegration: ローカルで起動している 1Password デスクトップアプリと連携する
	// WithIntegrationInfo: 1Password 管理コンソール上でのインテグレーション識別情報
	client, err := onepassword.NewClient(context.Background(),
		onepassword.WithDesktopAppIntegration(accname),
		onepassword.WithIntegrationInfo("My 1Password Integration", "v1.0.0"),
	)
	if err != nil {
		// クライアントの初期化に失敗した場合は空文字列を返す
		return C.CString("")
	}

	// シークレット参照 URI を使って 1Password Vault からシークレットを取得する
	secret, err := client.Secrets().Resolve(context.Background(), uri)
	if err != nil {
		// シークレットの解決に失敗した場合は空文字列を返す
		return C.CString("")
	}

	// GoのStringをC.mallocで確保したメモリにコピーして返す
	// 呼び出し側（C/C++）は使用後に FreeString() で解放する責任がある
	return C.CString(secret)
}

// FreeString は GetAPIKEY が返したC文字列のメモリを解放する。
//
// GoのCGOが C.CString() で確保したメモリは C.free() で解放する必要がある。
// C/C++側で直接 free() を呼ぶことも可能だが、本関数を使うことで
// メモリ管理の責務を明確にしている。
//
//export FreeString
func FreeString(s *C.char) {
	C.free(unsafe.Pointer(s))
}

// main は共有ライブラリとしてビルドする際に必要なダミーエントリポイント。
// -buildmode=c-shared ではこの関数は呼び出されない。
func main() {}
