" vim:set ts=8 sts=2 sw=2 tw=0:
"
" migemo.vim
"   Direct search for Japanese with Romaji --- Migemo support script.
"
" Maintainer:  MURAOKA Taro <koron@tka.att.ne.jp>
" Modified:    Yasuhiro Matsumoto <mattn_jp@hotmail.com>
" Last Change: 15-Dec-2013.

" Japanese Description:

if exists('plugin_migemo_disable')
  finish
endif

let s:save_cpo = &cpo
set cpo&vim

function! s:SearchDict2(name)
  let path = $VIM . ',' . &runtimepath
  let dict = globpath(path, "dict/".a:name)
  if dict == ''
    let dict = globpath(path, a:name)
  endif
  if dict == ''
    for path in [
          \ '/usr/local/share/migemo/',
          \ '/usr/local/share/cmigemo/',
          \ '/usr/local/share/',
          \ '/usr/share/cmigemo/',
          \ '/usr/share/',
          \ ]
      let path = path . a:name
      if filereadable(path)
        let dict = path
        break
      endif
    endfor
  endif
  let dict = matchstr(dict, "^[^\<NL>]*")
  return dict
endfunction

function! s:SearchDict()
  let dict = ''
  if dict == ''
    let dict = s:SearchDict2('migemo/'.&encoding.'/migemo-dict')
  endif
  if dict == ''
    let dict = s:SearchDict2(&encoding.'/migemo-dict')
  endif
  if dict == ''
    let dict = s:SearchDict2('migemo-dict')
  endif
  return dict
endfunction

if has('migemo')
  if &migemodict == '' || !filereadable(&migemodict)
    let &migemodict = s:SearchDict()
  endif

  " ƒeƒXƒg
  function! s:SearchChar(dir)
    let input = nr2char(getchar())
    let pat = migemo(input)
    call search('\%(\%#.\{-\}\)\@<='.pat)
    noh
  endfunction
  nnoremap <Leader>f :call <SID>SearchChar(0)<CR>
else
  " non-builtin version
  let g:migemodict = s:SearchDict()
  command! -nargs=* Migemo :call <SID>MigemoSearch(<q-args>)
  nnoremap <silent> <leader>mi :call <SID>MigemoSearch('')<cr>

  function! s:MigemoSearch(word)
    if executable('cmigemo') == ''
      echohl ErrorMsg
      echo 'Error: cmigemo is not installed'
      echohl None
      return
    endif
  
    let retval = a:word != '' ? a:word : input('MIGEMO:')
    if retval == ''
      return
    endif
    let retval = system('cmigemo -v -w "'.retval.'" -d "'.g:migemodict.'"')
    if retval == ''
      return
    endif
  
    let @/ = retval
    let v:errmsg = ''
    silent! normal n
    if v:errmsg != ''
      echohl ErrorMsg
      echo v:errmsg
      echohl None
    endif
  endfunction
endif

let &cpo = s:save_cpo
unlet s:save_cpo
