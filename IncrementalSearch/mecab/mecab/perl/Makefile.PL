use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'		=> 'MeCab',
    'CC'                => `mecab-config --cxx`,
    'LD'                => `mecab-config --cxx`,
    'INC'               => `mecab-config --cflags`,
    'LIBS'              => `mecab-config --libs`,
    'VERSION'	        => `mecab-config --version`,
    'OBJECT'            => 'MeCab_wrap.o' 
);
