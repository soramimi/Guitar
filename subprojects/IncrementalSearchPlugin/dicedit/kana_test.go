package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"reflect"
	"strings"
	"testing"
)

func TestNormalizeKana(t *testing.T) {
	valid := []struct{ input, want string }{
		{"", ""},
		{"ミミノ", "ミミノ"},
		{"ひらがなゔぁゕゖゝゞ", "ヒラガナヴァヵヶヽヾ"},
		{"こーひーアイス", "コーヒーアイス"},
		{"ｺｰﾋｰｶﾞｯﾂﾎﾟｰｽﾞ", "ｺｰﾋｰｶﾞｯﾂﾎﾟｰｽﾞ"},
		{"か\u3099は\u309a", "カ\u3099ハ\u309a"},
		{"ㇰヿ", "ㇰヿ"},
	}
	for _, test := range valid {
		t.Run(test.input, func(t *testing.T) {
			got, err := normalizeKana(test.input)
			if err != nil || got != test.want {
				t.Fatalf("normalizeKana(%q) = %q, %v; want %q", test.input, got, err, test.want)
			}
		})
	}
	for _, input := range []string{
		"漢字", "かなABC", "カナ123", "ＡＢＣ", "かな カナ", "カナ\n", "カナ\t",
		"かな*", "カナ・カナ", "カナ。", "カナ😀", "カナ\u200b", "゛", "\u3099", "ﾞ", "ー\u3099", "カ\u3099\u3099",
	} {
		t.Run("reject/"+input, func(t *testing.T) {
			if _, err := normalizeKana(input); err == nil {
				t.Fatalf("normalizeKana(%q) accepted a non-kana value", input)
			}
		})
	}
}

func TestRecordKanaValidation(t *testing.T) {
	const name = "kana-validation-test.csv"
	base := []string{"表層形", "1285", "1285", "100", "名詞", "一般", "*", "*", "*", "*", "原形", "ヨミ", "ハツオン", "追加列"}
	for _, method := range []string{http.MethodPost, http.MethodPut} {
		for _, invalidColumn := range []int{-1, 11, 12} {
			label := "valid"
			if invalidColumn >= 0 {
				label = columnNames[invalidColumn]
			}
			t.Run(method+"/"+label, func(t *testing.T) {
				fd := &fileData{records: []Record{{ID: 1, Fields: append([]string(nil), base...)}}, nextID: 2}
				cacheMu.Lock()
				previous, existed := fileCache[name]
				fileCache[name] = fd
				cacheMu.Unlock()
				t.Cleanup(func() {
					cacheMu.Lock()
					defer cacheMu.Unlock()
					if existed {
						fileCache[name] = previous
					} else {
						delete(fileCache, name)
					}
				})

				fields := append([]string(nil), base...)
				fields[3] = "200"
				fields[11], fields[12] = "ひらがな", "こーひー"
				if invalidColumn >= 0 {
					fields[invalidColumn] = "かな漢字"
				}
				body, err := json.Marshal(map[string]any{"fields": fields})
				if err != nil {
					t.Fatal(err)
				}
				path := "/api/records"
				if method == http.MethodPut {
					path += "/1"
				}
				request := httptest.NewRequest(method, path+"?file="+name, bytes.NewReader(body))
				response := httptest.NewRecorder()
				apiRecords(response, request)

				if invalidColumn >= 0 {
					if response.Code != http.StatusBadRequest || !strings.Contains(response.Body.String(), columnNames[invalidColumn]) {
						t.Fatalf("invalid %s: status=%d, body=%s", label, response.Code, response.Body)
					}
					if fd.nextID != 2 || len(fd.records) != 1 || !reflect.DeepEqual(fd.records[0].Fields, base) {
						t.Fatal("rejected request changed a record or consumed an ID")
					}
					return
				}

				wantStatus := http.StatusOK
				if method == http.MethodPost {
					wantStatus = http.StatusCreated
				}
				if response.Code != wantStatus {
					t.Fatalf("status=%d, body=%s", response.Code, response.Body)
				}
				var record Record
				if err := json.Unmarshal(response.Body.Bytes(), &record); err != nil {
					t.Fatal(err)
				}
				fields[11], fields[12] = "ヒラガナ", "コーヒー"
				if !reflect.DeepEqual(record.Fields, fields) || !reflect.DeepEqual(fd.records[len(fd.records)-1].Fields, fields) {
					t.Fatal("normalized values or unrelated CSV fields were not preserved")
				}
			})
		}
	}
}
