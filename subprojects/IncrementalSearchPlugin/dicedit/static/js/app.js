var NUM_COLS = [1, 2, 3];
var cols = [];
var curPage = 1;
var totalPages = 1;
var totalCount = 0;
var perPage = 25;
var curQuery = '';
var searchTimer = null;
var headBuilt = false;
var pageRequest = 0;
var editingRecord = null;
var editorSourceFields = [];
var editorBusy = false;
var parts = null;
var partsPromise = null;
var partByID = Object.create(null);
var currentFile = new URLSearchParams(location.search).get('file') || '';
var recordDialog = document.getElementById('record-dialog');

document.getElementById('record-form').addEventListener('submit', submitEditor);
recordDialog.addEventListener('cancel', function (event) {
  if (editorBusy) event.preventDefault();
});
document.addEventListener('keydown', function (event) {
  if (recordDialog.open && editorBusy && event.key === 'Escape') event.preventDefault();
});
recordDialog.addEventListener('close', function () {
  // close イベントが届く前に開き直した場合は、新しい編集状態を消さない。
  if (recordDialog.open) return;
  editingRecord = null;
  editorSourceFields = [];
});

// ファイル名をヘッダーに表示
if (currentFile) {
  var titleEl = document.getElementById('title');
  if (titleEl) titleEl.textContent = currentFile;
}

// ファイルが指定されていなければトップへ
if (!currentFile) {
  location.href = '/';
}

function recordsURL(params) {
  var q = new URLSearchParams(params || {});
  q.set('file', currentFile);
  return '/api/records?' + q.toString();
}

function recordURL(id) {
  var q = new URLSearchParams();
  q.set('file', currentFile);
  return '/api/records/' + encodeURIComponent(id) + '?' + q.toString();
}

function saveURL() {
  var q = new URLSearchParams();
  q.set('file', currentFile);
  return '/api/save?' + q.toString();
}

function fetchPage(page, query) {
  if (page < 1) page = 1;
  curPage = page;
  curQuery = (query === undefined) ? curQuery : query;
  var request = ++pageRequest;
  var url = recordsURL({ page: curPage, per_page: perPage, q: curQuery });
  return fetch(url).then(responseJSON).then(function (data) {
    // 検索や編集後の再取得より前に開始した応答は、現在の一覧へ適用しない。
    if (request !== pageRequest) return;
    if (data.page > data.total_pages) return fetchPage(data.total_pages);
    if (!headBuilt) {
      cols = data.columns || [];
      buildHead();
      headBuilt = true;
      document.getElementById('btn-add').disabled = false;
    }
    totalCount = data.total || 0;
    totalPages = data.total_pages || 1;
    curPage = data.page || 1;
    renderPage(data.records || []);
    updatePager();
    updateInfo();
  }).catch(function (err) {
    if (request === pageRequest) setStatus('一覧取得エラー: ' + err.message, true);
  });
}

function buildHead() {
  var tr = document.createElement('tr');
  var editHead = document.createElement('th');
  editHead.textContent = '編集';
  tr.appendChild(editHead);
  cols.forEach(function (c) {
    var th = document.createElement('th');
    th.textContent = c;
    tr.appendChild(th);
  });
  var th = document.createElement('th');
  th.textContent = '削除';
  tr.appendChild(th);
  var thead = document.getElementById('thead');
  thead.innerHTML = '';
  thead.appendChild(tr);
}

function renderPage(recs) {
  var tbody = document.getElementById('tbody');
  tbody.innerHTML = '';
  for (var i = 0; i < recs.length; i++) {
    tbody.appendChild(mkRow(recs[i]));
  }
  document.getElementById('wrap').scrollTop = 0;
}

function mkRow(rec) {
  var tr = document.createElement('tr');
  tr.dataset.id = rec.id;
  var fields = rec.fields || [];
  var tdEdit = document.createElement('td');
  tdEdit.className = 'edit-act';
  var edit = document.createElement('button');
  edit.className = 'btn-edit';
  edit.textContent = '編集';
  edit.setAttribute('aria-label', (fields[0] || 'エントリ') + 'を編集');
  edit.onclick = function () { openEditor(rec); };
  tdEdit.appendChild(edit);
  tr.appendChild(tdEdit);
  for (var i = 0; i < cols.length; i++) {
    var td = document.createElement('td');
    if (NUM_COLS.indexOf(i) >= 0) td.className = 'num';
    td.textContent = fields[i] !== undefined ? fields[i] : '';
    tr.appendChild(td);
  }
  var tdDel = document.createElement('td');
  tdDel.className = 'act';
  var btn = document.createElement('button');
  btn.className = 'btn-del';
  btn.textContent = '削除';
  btn.onclick = mkDel(rec.id);
  tdDel.appendChild(btn);
  tr.appendChild(tdDel);
  return tr;
}

function openEditor(rec) {
  if (recordDialog.open || editorBusy) return;
  loadParts().then(function () {
    if (!recordDialog.open && !editorBusy) buildEditor(rec);
  }).catch(function (err) {
    setStatus('品詞リスト取得エラー: ' + err.message, true);
  });
}

function loadParts() {
  if (partsPromise) return partsPromise;
  partsPromise = fetch('/api/parts').then(responseJSON).then(function (data) {
    parts = data;
    parts.forEach(function (part) { partByID[String(part.id)] = part; });
  }).catch(function (err) {
    partsPromise = null;
    throw err;
  });
  return partsPromise;
}

function buildEditor(rec) {
  setStatus('', false);
  editingRecord = rec || null;
  editorSourceFields = rec && rec.fields ? rec.fields.slice() : [];
  document.getElementById('editor-title').textContent = rec ? 'エントリを編集' : 'エントリを追加';
  document.getElementById('editor-submit').textContent = rec ? '適用' : '追加';
  document.getElementById('editor-error').textContent = '';
  var container = document.getElementById('editor-fields');
  container.innerHTML = '';
  cols.forEach(function (name, i) {
    if (i === 4) {
      container.appendChild(buildPartSelector());
      return;
    }
    var label = document.createElement('label');
    label.className = 'editor-field';
    var caption = document.createElement('span');
    caption.textContent = name;
    var input = document.createElement('input');
    input.type = 'text';
    input.name = 'field-' + i;
    input.dataset.column = i;
    input.value = editorSourceFields[i] !== undefined ? editorSourceFields[i] : '';
    if (NUM_COLS.indexOf(i) >= 0) input.inputMode = 'numeric';
    if (i >= 5 && i <= 9) input.readOnly = true;
    label.appendChild(caption);
    label.appendChild(input);
    container.appendChild(label);
  });
  setEditorBusy(false);
  recordDialog.showModal();
  var first = container.querySelector('input');
  if (first) first.focus();
}

function partLabel(part) {
  var labels = part.fields.slice(0, 6).filter(function (value) { return value !== '*'; });
  if (part.fields[6] !== '*') labels.push('原形区分: ' + part.fields[6]);
  return labels.join(' / ') + '（ID: ' + part.id + '）';
}

function buildPartSelector() {
  var wrapper = document.createElement('div');
  wrapper.className = 'editor-field editor-part';
  var caption = document.createElement('label');
  caption.htmlFor = 'editor-part-select';
  caption.textContent = '品詞・活用';
  wrapper.appendChild(caption);
  var filter = document.createElement('input');
  filter.type = 'search';
  filter.placeholder = '品詞・細分類・活用・IDで絞り込み';
  filter.setAttribute('aria-label', '品詞の選択肢を絞り込み');
  wrapper.appendChild(filter);
  var select = document.createElement('select');
  select.id = 'editor-part-select';
  select.required = true;
  wrapper.appendChild(select);
  var hint = document.createElement('p');
  hint.id = 'editor-part-hint';
  hint.className = 'editor-field-hint';
  hint.textContent = '選択すると左右の文脈ID・細分類・活用情報を適用します。原形・読み・発音は保持します。';
  select.setAttribute('aria-describedby', hint.id);
  wrapper.appendChild(hint);
  var count = document.createElement('p');
  count.className = 'editor-field-hint';
  count.setAttribute('aria-live', 'polite');
  wrapper.appendChild(count);

  var current = parts.find(function (part) {
    return String(part.id) === editorSourceFields[1] && String(part.id) === editorSourceFields[2]
      && part.fields.slice(0, 6).every(function (value, i) { return value === editorSourceFields[i + 4]; });
  });
  var initialValue = current ? String(current.id) : (editingRecord ? 'current' : '');

  function renderOptions() {
    var selected = select.value || initialValue;
    var words = filter.value.trim().toLowerCase().split(/\s+/).filter(Boolean);
    select.innerHTML = '';
    var prompt = document.createElement('option');
    prompt.value = '';
    prompt.textContent = '品詞・活用を選択してください';
    prompt.disabled = true;
    select.appendChild(prompt);
    if (!current && editingRecord) {
      var original = document.createElement('option');
      original.value = 'current';
      original.textContent = '現在の値を保持（定義と未一致）: ' + editorSourceFields.slice(4, 10).join(' / ');
      select.appendChild(original);
    }
    var matches = 0;
    parts.forEach(function (part) {
      var label = partLabel(part);
      var match = words.every(function (word) { return label.toLowerCase().includes(word); });
      if (match) matches++;
      // 絞り込みだけでは現在の選択や編集内容を変更しない。
      if (!match && String(part.id) !== selected) return;
      var option = document.createElement('option');
      option.value = String(part.id);
      option.textContent = label;
      select.appendChild(option);
    });
    select.value = selected;
    count.textContent = '一致する候補: ' + matches + '件';
  }
  filter.addEventListener('input', renderOptions);
  filter.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      select.focus();
    }
  });
  select.addEventListener('change', function () {
    if (select.value === 'current') {
      [1, 2, 5, 6, 7, 8, 9].forEach(function (column) {
        document.querySelector('#editor-fields [data-column="' + column + '"]').value = editorSourceFields[column] || '';
      });
      return;
    }
    var part = partByID[select.value];
    if (!part) return;
    [1, 2].forEach(function (column) {
      document.querySelector('#editor-fields [data-column="' + column + '"]').value = part.id;
    });
    for (var column = 5; column <= 9; column++) {
      document.querySelector('#editor-fields [data-column="' + column + '"]').value = part.fields[column - 4];
    }
  });
  renderOptions();
  return wrapper;
}

function closeEditor() {
  if (!editorBusy) recordDialog.close();
}

function setEditorBusy(busy) {
  editorBusy = busy;
  document.getElementById('editor-fields').disabled = busy;
  document.getElementById('editor-cancel').disabled = busy;
  document.getElementById('editor-submit').disabled = busy;
  document.getElementById('record-form').setAttribute('aria-busy', String(busy));
}

function normalizeKana(value) {
  var normalized = '';
  var canAddMark = false;
  for (var character of value) {
    var code = character.codePointAt(0);
    if ((code >= 0x3041 && code <= 0x3096) || code === 0x309d || code === 0x309e) {
      code += 0x60;
      character = String.fromCodePoint(code);
    }
    if ((code >= 0x30a1 && code <= 0x30fa) || code === 0x30fd || code === 0x30fe || code === 0x30ff
        || (code >= 0x31f0 && code <= 0x31ff) || (code >= 0xff66 && code <= 0xff6f)
        || (code >= 0xff71 && code <= 0xff9d)) {
      canAddMark = true;
    } else if (code === 0x30fc || code === 0xff70) {
      canAddMark = false;
    } else if (code === 0x3099 || code === 0x309a || code === 0xff9e || code === 0xff9f) {
      if (!canAddMark) throw new Error('濁点・半濁点は仮名の直後に入力してください');
      canAddMark = false;
    } else {
      throw new Error('仮名以外の文字「' + character + '」は使用できません');
    }
    normalized += character;
  }
  return normalized;
}

function submitEditor(event) {
  event.preventDefault();
  if (editorBusy) return;
  var isNew = editingRecord === null;
  // 表示対象外の追加列がある CSV でも、その値を保持する。
  var fields = editorSourceFields.slice();
  var inputs = document.getElementById('editor-fields').querySelectorAll('[data-column]');
  for (var i = 0; i < inputs.length; i++) fields[Number(inputs[i].dataset.column)] = inputs[i].value;
  var selectedPart = partByID[document.getElementById('editor-part-select').value];
  if (selectedPart) fields[4] = selectedPart.fields[0];
  document.getElementById('editor-error').textContent = '';
  var normalizedKana = [];
  var kanaColumns = [11, 12];
  kanaColumns.forEach(function (column) {
    document.querySelector('#editor-fields [data-column="' + column + '"]').removeAttribute('aria-invalid');
  });
  for (var i = 0; i < kanaColumns.length; i++) {
    var column = kanaColumns[i];
    try {
      normalizedKana[i] = normalizeKana(fields[column]);
    } catch (err) {
      var invalidInput = document.querySelector('#editor-fields [data-column="' + column + '"]');
      invalidInput.setAttribute('aria-invalid', 'true');
      invalidInput.focus();
      document.getElementById('editor-error').textContent = cols[column] + ': ' + err.message;
      return;
    }
  }
  // 両方が有効な場合だけ入力欄を変換し、1エントリ全体を適用する。
  kanaColumns.forEach(function (column, i) {
    fields[column] = normalizedKana[i];
    document.querySelector('#editor-fields [data-column="' + column + '"]').value = normalizedKana[i];
  });
  setEditorBusy(true);
  fetch(isNew ? recordsURL() : recordURL(editingRecord.id), {
    method: isNew ? 'POST' : 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ fields: fields })
  }).then(responseJSON).then(function () {
    setEditorBusy(false);
    closeEditor();
    if (isNew) {
      document.getElementById('search').value = '';
      fetchPage(Math.ceil((totalCount + 1) / perPage), '');
    } else {
      fetchPage(curPage);
    }
    setStatus('一覧に適用しました。CSVへの保存は「保存」を押してください。', false);
  }).catch(function (err) {
    setEditorBusy(false);
    document.getElementById('editor-error').textContent = (isNew ? '追加' : '更新') + 'エラー: ' + err.message;
  });
}

function mkDel(id) {
  return function () {
    if (!confirm('この行を削除しますか？')) return;
    fetch(recordURL(id), { method: 'DELETE' }).then(function (r) {
      if (r.ok) {
        fetchPage(curPage);
      } else {
        setStatus('削除エラー', true);
      }
    });
  };
}

function responseJSON(r) {
  if (r.ok) return r.json();
  return r.text().then(function (text) {
    throw new Error('HTTP ' + r.status + (text ? ': ' + text.trim() : ''));
  });
}

function addRow() {
  if (headBuilt) openEditor(null);
}

function saveFile() {
  setStatus('保存中...', false);
  fetch(saveURL(), { method: 'POST' }).then(function (r) {
    if (r.ok) {
      location.href = '/';
    } else {
      setStatus('保存エラー', true);
    }
  });
}

function onSearchInput() {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(function () {
    fetchPage(1, document.getElementById('search').value);
  }, 300);
}

function setStatus(msg, isErr) {
  var el = document.getElementById('status');
  el.textContent = msg;
  el.className = isErr ? 'err' : 'ok';
  if (!isErr && msg !== '保存中...') {
    setTimeout(function () { el.textContent = ''; }, 3000);
  }
}

function updateInfo() {
  var el = document.getElementById('info');
  if (curQuery) {
    el.textContent = totalCount + ' 件ヒット';
  } else {
    el.textContent = '全 ' + totalCount + ' 件';
  }
}

function updatePager() {
  document.getElementById('page-input').value = curPage;
  document.getElementById('page-input').max = totalPages;
  document.getElementById('page-of').textContent = '/ ' + totalPages + ' ページ';
  document.getElementById('btn-first').disabled = curPage <= 1;
  document.getElementById('btn-prev').disabled = curPage <= 1;
  document.getElementById('btn-next').disabled = curPage >= totalPages;
  document.getElementById('btn-last').disabled = curPage >= totalPages;
}

function goFirst() { fetchPage(1); }
function goPrev() { fetchPage(curPage - 1); }
function goNext() { fetchPage(curPage + 1); }
function goLast() { fetchPage(totalPages); }
function goPageInput() {
  var v = parseInt(document.getElementById('page-input').value, 10);
  if (!isNaN(v)) fetchPage(v);
}

fetchPage(1, '');
