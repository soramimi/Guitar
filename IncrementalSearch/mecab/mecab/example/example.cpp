#include <iostream>
#include <mecab.h>

#define CHECK(eval) if (! eval) { \
   const char *e = tagger ? tagger->what() : MeCab::getTaggerError(); \
   std::cerr << "Exception:" << e << std::endl; \
   delete tagger; \
   return -1; }

// Sample of MeCab::Tagger class.
int main (int argc, char **argv) {
  char input[] = "太郎は次郎が持っている本を花子に渡した。";

  MeCab::Tagger *tagger = MeCab::createTagger("");
  CHECK(tagger);

  // Gets tagged result in string format.
  const char *result = tagger->parse(input);
  CHECK(result);
  std::cout << "INPUT: " << input << std::endl;
  std::cout << "RESULT: " << result << std::endl;

  // Gets N best results in string format.
  result = tagger->parseNBest(3, input);
  CHECK(result);
  std::cout << "NBEST: " << std::endl << result;

  // Gets N best results in sequence.
  CHECK(tagger->parseNBestInit(input));
  for (int i = 0; i < 3; ++i) {
    std::cout << i << ":" << std::endl << tagger->next();
  }

  // Gets Node object.
  const MeCab::Node* node = tagger->parseToNode(input);
  CHECK(node);
  for (; node; node = node->next) {
    std::cout << node->id << ' ';
    if (node->stat == MECAB_BOS_NODE)
      std::cout << "BOS";
    else if (node->stat == MECAB_EOS_NODE)
      std::cout << "EOS";
    else
      std::cout.write (node->surface, node->length);

    std::cout << ' ' << node->feature
	      << ' ' << (int)(node->surface - input)
	      << ' ' << (int)(node->surface - input + node->length)
	      << ' ' << node->rcAttr
	      << ' ' << node->lcAttr
	      << ' ' << node->posid
	      << ' ' << (int)node->char_type
	      << ' ' << (int)node->stat
	      << ' ' << (int)node->isbest
	      << ' ' << node->alpha
	      << ' ' << node->beta
	      << ' ' << node->prob
	      << ' ' << node->cost << std::endl;
  }

  // Dictionary info.
  const MeCab::DictionaryInfo *d = tagger->dictionary_info();
  for (; d; d = d->next) {
    std::cout << "filename: " <<  d->filename << std::endl;
    std::cout << "charset: " <<  d->charset << std::endl;
    std::cout << "size: " <<  d->size << std::endl;
    std::cout << "type: " <<  d->type << std::endl;
    std::cout << "lsize: " <<  d->lsize << std::endl;
    std::cout << "rsize: " <<  d->rsize << std::endl;
    std::cout << "version: " <<  d->version << std::endl;
  }

  delete tagger;

  return 0;
}
