package main

import (
	"bytes"
	_ "embed"
	"encoding/csv"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"

	"golang.org/x/text/encoding/japanese"
	"golang.org/x/text/transform"
)

//go:embed templates/index.html
var indexHTML string

//go:embed templates/edit.html
var editHTML string

const dicsDir = "dics"

// Record は MeCab 辞書の1エントリを表す
type Record struct {
	ID     int      `json:"id"`
	Fields []string `json:"fields"`
}

// fileData はファイルごとのインメモリキャッシュ
type fileData struct {
	mu      sync.RWMutex
	records []Record
	nextID  int
}

var (
	cacheMu   sync.RWMutex
	fileCache = map[string]*fileData{}
)

// MeCab IPAdic 形式の列名
var columnNames = []string{
	"表層形", "左文脈ID", "右文脈ID", "コスト",
	"品詞", "品詞細分類1", "品詞細分類2", "品詞細分類3",
	"活用型", "活用形", "原形", "読み", "発音",
}

// validateFile はファイル名がパストラバーサルを含まないか検証する
func validateFile(name string) bool {
	return name != "" && filepath.Base(name) == name && strings.HasSuffix(name, ".csv")
}

// getFileData はキャッシュからファイルデータを返すか、なければ dics/ から読み込む
func getFileData(name string) (*fileData, error) {
	cacheMu.RLock()
	fd, ok := fileCache[name]
	cacheMu.RUnlock()
	if ok {
		return fd, nil
	}

	path := filepath.Join(dicsDir, name)
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	reader := csv.NewReader(transform.NewReader(f, japanese.EUCJP.NewDecoder()))
	reader.FieldsPerRecord = -1

	fd = &fileData{nextID: 1}
	for {
		row, err := reader.Read()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
		fd.records = append(fd.records, Record{ID: fd.nextID, Fields: row})
		fd.nextID++
	}

	cacheMu.Lock()
	if existing, ok := fileCache[name]; ok {
		cacheMu.Unlock()
		return existing, nil
	}
	fileCache[name] = fd
	cacheMu.Unlock()
	return fd, nil
}

func saveCSV(name string, fd *fileData) error {
	var buf bytes.Buffer
	enc := transform.NewWriter(&buf, japanese.EUCJP.NewEncoder())
	writer := csv.NewWriter(enc)

	fd.mu.RLock()
	for _, r := range fd.records {
		if err := writer.Write(r.Fields); err != nil {
			fd.mu.RUnlock()
			return err
		}
	}
	writer.Flush()
	writeErr := writer.Error()
	fd.mu.RUnlock()

	if writeErr != nil {
		return writeErr
	}
	if err := enc.Close(); err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(dicsDir, name), buf.Bytes(), 0644)
}

// apiDics は dics/ 内の CSV ファイル一覧を返す
func apiDics(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	entries, err := os.ReadDir(dicsDir)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	var names []string
	for _, e := range entries {
		if !e.IsDir() && strings.HasSuffix(e.Name(), ".csv") {
			names = append(names, e.Name())
		}
	}
	sort.Strings(names)
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(names)
}

// apiRecords は /api/records および /api/records/{id} を処理する
func apiRecords(w http.ResponseWriter, r *http.Request) {
	file := r.URL.Query().Get("file")
	if !validateFile(file) {
		http.Error(w, "invalid file parameter", http.StatusBadRequest)
		return
	}
	fd, err := getFileData(file)
	if err != nil {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}

	suffix := strings.Trim(strings.TrimPrefix(r.URL.Path, "/api/records"), "/")

	if suffix != "" {
		// /api/records/{id}
		id, err := strconv.Atoi(suffix)
		if err != nil {
			http.Error(w, "invalid id", http.StatusBadRequest)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		switch r.Method {
		case http.MethodPut:
			var req struct {
				Fields []string `json:"fields"`
			}
			if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
				http.Error(w, err.Error(), http.StatusBadRequest)
				return
			}
			if req.Fields == nil {
				req.Fields = []string{}
			}
			fd.mu.Lock()
			defer fd.mu.Unlock()
			for i := range fd.records {
				if fd.records[i].ID == id {
					fd.records[i].Fields = req.Fields
					json.NewEncoder(w).Encode(fd.records[i])
					return
				}
			}
			http.Error(w, "not found", http.StatusNotFound)
		case http.MethodDelete:
			fd.mu.Lock()
			defer fd.mu.Unlock()
			for i := range fd.records {
				if fd.records[i].ID == id {
					fd.records = append(fd.records[:i], fd.records[i+1:]...)
					w.WriteHeader(http.StatusNoContent)
					return
				}
			}
			http.Error(w, "not found", http.StatusNotFound)
		default:
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		}
		return
	}

	// /api/records
	w.Header().Set("Content-Type", "application/json")
	switch r.Method {
	case http.MethodGet:
		q := strings.ToLower(r.URL.Query().Get("q"))
		page, _ := strconv.Atoi(r.URL.Query().Get("page"))
		perPage, _ := strconv.Atoi(r.URL.Query().Get("per_page"))
		if page < 1 {
			page = 1
		}
		if perPage < 1 {
			perPage = 25
		}
		fd.mu.RLock()
		var filtered []Record
		for _, rec := range fd.records {
			if q == "" {
				filtered = append(filtered, rec)
			} else {
				for _, f := range rec.Fields {
					if strings.Contains(strings.ToLower(f), q) {
						filtered = append(filtered, rec)
						break
					}
				}
			}
		}
		total := len(filtered)
		totalPages := (total + perPage - 1) / perPage
		if totalPages < 1 {
			totalPages = 1
		}
		start := (page - 1) * perPage
		end := start + perPage
		if start > total {
			start = total
		}
		if end > total {
			end = total
		}
		pageRecs := filtered[start:end]
		fd.mu.RUnlock()
		data := struct {
			Columns    []string `json:"columns"`
			Records    []Record `json:"records"`
			Total      int      `json:"total"`
			Page       int      `json:"page"`
			PerPage    int      `json:"per_page"`
			TotalPages int      `json:"total_pages"`
		}{columnNames, pageRecs, total, page, perPage, totalPages}
		json.NewEncoder(w).Encode(data)
	case http.MethodPost:
		var req struct {
			Fields []string `json:"fields"`
		}
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		if req.Fields == nil {
			req.Fields = []string{}
		}
		fd.mu.Lock()
		rec := Record{ID: fd.nextID, Fields: req.Fields}
		fd.nextID++
		fd.records = append(fd.records, rec)
		fd.mu.Unlock()
		w.WriteHeader(http.StatusCreated)
		json.NewEncoder(w).Encode(rec)
	default:
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
	}
}

func apiSave(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	file := r.URL.Query().Get("file")
	if !validateFile(file) {
		http.Error(w, "invalid file parameter", http.StatusBadRequest)
		return
	}
	fd, err := getFileData(file)
	if err != nil {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}
	if err := saveCSV(file, fd); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	fmt.Fprint(w, `{"status":"saved"}`)
}

func main() {
	http.HandleFunc("/api/dics", apiDics)
	http.HandleFunc("/api/records/", apiRecords)
	http.HandleFunc("/api/records", apiRecords)
	http.HandleFunc("/api/save", apiSave)
	http.Handle("/static/", http.StripPrefix("/static/", http.FileServer(http.Dir("static"))))
	http.HandleFunc("/edit", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		fmt.Fprint(w, editHTML)
	})
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		fmt.Fprint(w, indexHTML)
	})

	addr := ":1080"
	log.Printf("Starting server at http://localhost%s", addr)
	log.Fatal(http.ListenAndServe(addr, nil))
}
