%module MeCab
%include exception.i
%{
#include "mecab.h"

/* Workaround for ruby1.9.x */
#if defined SWIGRUBY
#include "ruby/version.h"
#if RUBY_API_VERSION_CODE >= 10900
#include "ruby/encoding.h"
#define rb_str_new rb_external_str_new
#endif
#endif
%}

%newobject surface;

%exception {
  try { $action }
  catch (char *e) { SWIG_exception (SWIG_RuntimeError, e); }
  catch (const char *e) { SWIG_exception (SWIG_RuntimeError, (char*)e); }
}

%rename(Node) mecab_node_t;
%rename(Path) mecab_path_t;
%rename(DictionaryInfo) mecab_dictionary_info_t;
%ignore    mecab_model_t;
%ignore    mecab_lattice_t;
%nodefault mecab_path_t;
%nodefault mecab_node_t;

%feature("notabstract") MeCab::Tagger;
%feature("notabstract") MeCab::Lattice;
%feature("notabstract") MeCab::Model;

%immutable mecab_dictionary_info_t::filename;
%immutable mecab_dictionary_info_t::charset;
%immutable mecab_dictionary_info_t::size;
%immutable mecab_dictionary_info_t::lsize;
%immutable mecab_dictionary_info_t::rsize;
%immutable mecab_dictionary_info_t::type;
%immutable mecab_dictionary_info_t::version;
%immutable mecab_dictionary_info_t::next;

%immutable mecab_path_t::rnode;
%immutable mecab_path_t::lnode;
%immutable mecab_path_t::rnext;
%immutable mecab_path_t::lnext;
%immutable mecab_path_t::cost;

%immutable mecab_node_t::prev;
%immutable mecab_node_t::next;
%immutable mecab_node_t::enext;
%immutable mecab_node_t::bnext;
%immutable mecab_node_t::lpath;
%immutable mecab_node_t::rpath;
%immutable mecab_node_t::feature;
%immutable mecab_node_t::length;
%immutable mecab_node_t::rlength;
%immutable mecab_node_t::id;
%immutable mecab_node_t::rcAttr;
%immutable mecab_node_t::lcAttr;
%immutable mecab_node_t::posid;
%immutable mecab_node_t::char_type;
%immutable mecab_node_t::stat;
%immutable mecab_node_t::isbest;
%immutable mecab_node_t::alpha;
%immutable mecab_node_t::beta;
%immutable mecab_node_t::wcost;
%immutable mecab_node_t::cost;
%immutable mecab_node_t::surface;

%extend mecab_node_t {
  char *surface;
}

%extend MeCab::Tagger {
   Tagger(const char *argc);
   Tagger();
   const char* parseToString(const char* str, size_t length = 0) {
     return self->parse(str, length);
   }
}

%extend MeCab::Model {
   Model(const char *argc);
   Model();
}

%extend MeCab::Lattice {
  Lattice();
  void set_sentence(const char *sentence) {
    self->add_request_type(MECAB_ALLOCATE_SENTENCE);
    self->set_sentence(sentence);
  }
}

%{

MeCab::Tagger* new_MeCab_Tagger (const char *arg) {
  char *p = new char [strlen(arg) + 4];
  strcpy(p, "-C ");
  strcat(p, arg);
  MeCab::Tagger *tagger = MeCab::createTagger(p);
  delete [] p;
  if (! tagger) throw MeCab::getLastError();
  return tagger;
}

MeCab::Tagger* new_MeCab_Tagger () {
  MeCab::Tagger *tagger = MeCab::createTagger("-C");
  if (! tagger) throw MeCab::getLastError();
  return tagger;
}

void delete_MeCab_Tagger (MeCab::Tagger *t) {
  delete t;
  t = 0;
}

MeCab::Model* new_MeCab_Model (const char *arg) {
  char *p = new char [strlen(arg) + 4];
  strcpy(p, "-C ");
  strcat(p, arg);
  MeCab::Model *model = MeCab::createModel(p);
  delete [] p;
  if (! model) throw MeCab::getLastError();
  return model;
}

MeCab::Model* new_MeCab_Model () {
  MeCab::Model *model = MeCab::createModel("-C");
  if (! model) throw MeCab::getLastError();
  return model;
}

void delete_MeCab_Model (MeCab::Model *t) {
  delete t;
  t = 0;
}

MeCab::Lattice* new_MeCab_Lattice () {
  return MeCab::createLattice();
}

void delete_MeCab_Lattice (MeCab::Lattice *t) {
  delete t;
  t = 0;
}

char* mecab_node_t_surface_get(mecab_node_t *n) {
  char *s = new char [n->length + 1];
  memcpy (s, n->surface, n->length);
  s[n->length] = '\0';
  return s;
}
%}

%include ../src/mecab.h
%include version.h
