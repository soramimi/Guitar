var NUM_COLS = [1, 2, 3];
var cols = [];
var curPage = 1;
var totalPages = 1;
var totalCount = 0;
var perPage = 25;
var curQuery = '';
var searchTimer = null;
var headBuilt = false;
var currentFile = new URLSearchParams(location.search).get('file') || '';

// ファイル名をヘッダーに表示
if (currentFile) {
  var titleEl = document.getElementById('title');
  if (titleEl) titleEl.textContent = currentFile;
}

// ファイルが指定されていなければトップへ
if (!currentFile) {
  location.href = '/';
}

function fileParam() {
  return '&file=' + encodeURIComponent(currentFile);
}

function fetchPage(page, query) {
  if (page < 1) page = 1;
  if (totalPages > 0 && page > totalPages) page = totalPages;
  curPage = page;
  curQuery = (query === undefined) ? curQuery : query;
  var url = '/api/records?page=' + curPage + '&per_page=' + perPage + '&q=' + encodeURIComponent(curQuery) + fileParam();
  fetch(url).then(function(r) { return r.json(); }).then(function(data) {
    if (!headBuilt) {
      cols = data.columns || [];
      buildHead();
      headBuilt = true;
    }
    totalCount = data.total || 0;
    totalPages = data.total_pages || 1;
    curPage = data.page || 1;
    renderPage(data.records || []);
    updatePager();
    updateInfo();
  });
}

function buildHead() {
  var tr = document.createElement('tr');
  cols.forEach(function(c) {
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
  for (var i = 0; i < cols.length; i++) {
    var td = document.createElement('td');
    if (NUM_COLS.indexOf(i) >= 0) td.className = 'num';
    var inp = document.createElement('input');
    inp.type = 'text';
    inp.value = fields[i] !== undefined ? fields[i] : '';
    inp.addEventListener('change', mkFlush(rec.id, tr));
    td.appendChild(inp);
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

function mkFlush(id, tr) {
  return function() {
    var fields = getFields(tr);
    fetch('/api/records/' + id + '?' + fileParam().slice(1), {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ fields: fields })
    }).then(function(r) {
      if (!r.ok) setStatus('更新エラー', true);
    });
  };
}

function mkDel(id) {
  return function() {
    if (!confirm('この行を削除しますか？')) return;
    fetch('/api/records/' + id + '?' + fileParam().slice(1), { method: 'DELETE' }).then(function(r) {
      if (r.ok) {
        fetchPage(curPage);
      } else {
        setStatus('削除エラー', true);
      }
    });
  };
}

function getFields(tr) {
  var inputs = tr.querySelectorAll('input');
  var arr = [];
  for (var i = 0; i < inputs.length; i++) arr.push(inputs[i].value);
  return arr;
}

function addRow() {
  var fields = [];
  for (var i = 0; i < cols.length; i++) fields.push('');
  fetch('/api/records?' + fileParam().slice(1), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ fields: fields })
  }).then(function(r) {
    if (!r.ok) { setStatus('追加エラー', true); return null; }
    return r.json();
  }).then(function(rec) {
    if (!rec) return;
    var newTotalPages = Math.ceil((totalCount + 1) / perPage);
    fetchPage(newTotalPages, '');
    document.getElementById('search').value = '';
  });
}

function saveFile() {
  setStatus('保存中...', false);
  fetch('/api/save?' + fileParam().slice(1), { method: 'POST' }).then(function(r) {
    if (r.ok) {
      location.href = '/';
    } else {
      setStatus('保存エラー', true);
    }
  });
}

function onSearchInput() {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(function() {
    fetchPage(1, document.getElementById('search').value);
  }, 300);
}

function setStatus(msg, isErr) {
  var el = document.getElementById('status');
  el.textContent = msg;
  el.className = isErr ? 'err' : 'ok';
  if (!isErr && msg !== '保存中...') {
    setTimeout(function() { el.textContent = ''; }, 3000);
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
function goPrev()  { fetchPage(curPage - 1); }
function goNext()  { fetchPage(curPage + 1); }
function goLast()  { fetchPage(totalPages); }
function goPageInput() {
  var v = parseInt(document.getElementById('page-input').value, 10);
  if (!isNaN(v)) fetchPage(v);
}

fetchPage(1, '');


function buildHead() {
  var tr = document.createElement('tr');
  cols.forEach(function(c) {
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
  for (var i = 0; i < cols.length; i++) {
    var td = document.createElement('td');
    if (NUM_COLS.indexOf(i) >= 0) td.className = 'num';
    var inp = document.createElement('input');
    inp.type = 'text';
    inp.value = fields[i] !== undefined ? fields[i] : '';
    inp.addEventListener('change', mkFlush(rec.id, tr));
    td.appendChild(inp);
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

function mkFlush(id, tr) {
  return function() {
    var fields = getFields(tr);
    fetch('/api/records/' + id, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ fields: fields })
    }).then(function(r) {
      if (!r.ok) setStatus('更新エラー', true);
    });
  };
}

function mkDel(id) {
  return function() {
    if (!confirm('この行を削除しますか？')) return;
    fetch('/api/records/' + id, { method: 'DELETE' }).then(function(r) {
      if (r.ok) {
        fetchPage(curPage);
      } else {
        setStatus('削除エラー', true);
      }
    });
  };
}

function getFields(tr) {
  var inputs = tr.querySelectorAll('input');
  var arr = [];
  for (var i = 0; i < inputs.length; i++) arr.push(inputs[i].value);
  return arr;
}

function addRow() {
  var fields = [];
  for (var i = 0; i < cols.length; i++) fields.push('');
  fetch('/api/records', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ fields: fields })
  }).then(function(r) {
    if (!r.ok) { setStatus('追加エラー', true); return null; }
    return r.json();
  }).then(function(rec) {
    if (!rec) return;
    var newTotalPages = Math.ceil((totalCount + 1) / perPage);
    fetchPage(newTotalPages, '');
    document.getElementById('search').value = '';
  });
}

function saveFile() {
  setStatus('保存中...', false);
  fetch('/api/save', { method: 'POST' }).then(function(r) {
    if (r.ok) setStatus('保存しました', false);
    else setStatus('保存エラー', true);
  });
}

function onSearchInput() {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(function() {
    fetchPage(1, document.getElementById('search').value);
  }, 300);
}

function setStatus(msg, isErr) {
  var el = document.getElementById('status');
  el.textContent = msg;
  el.className = isErr ? 'err' : 'ok';
  if (!isErr && msg !== '保存中...') {
    setTimeout(function() { el.textContent = ''; }, 3000);
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
function goPrev()  { fetchPage(curPage - 1); }
function goNext()  { fetchPage(curPage + 1); }
function goLast()  { fetchPage(totalPages); }
function goPageInput() {
  var v = parseInt(document.getElementById('page-input').value, 10);
  if (!isNaN(v)) fetchPage(v);
}

fetchPage(1, '');
