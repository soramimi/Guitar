#include <mecab.h>
#include <stdio.h>

#define CHECK(eval) if (! eval) { \
    fprintf (stderr, "Exception:%s\n", mecab_strerror (mecab)); \
    mecab_destroy(mecab); \
    return -1; }

int main (int argc, char **argv)  {
  char input[] = "太郎は次郎が持っている本を花子に渡した。";
  mecab_t *mecab;
  const mecab_node_t *node;
  const char *result;
  int i;
  size_t len;

  // Create tagger object
  mecab = mecab_new(argc, argv);
  CHECK(mecab);

  // Gets tagged result in string.
  result = mecab_sparse_tostr(mecab, input);
  CHECK(result)
  printf ("INPUT: %s\n", input);
  printf ("RESULT:\n%s", result);

  // Gets N best results
  result = mecab_nbest_sparse_tostr (mecab, 3, input);
  CHECK(result);
  fprintf (stdout, "NBEST:\n%s", result);

  CHECK(mecab_nbest_init(mecab, input));
  for (i = 0; i < 3; ++i) {
    printf ("%d:\n%s", i, mecab_nbest_next_tostr (mecab));
  }

  // Gets node object
  node = mecab_sparse_tonode(mecab, input);
  CHECK(node);
  for (; node; node = node->next) {
    if (node->stat == MECAB_NOR_NODE || node->stat == MECAB_UNK_NODE) {
      fwrite (node->surface, sizeof(char), node->length, stdout);
      printf("\t%s\n", node->feature);
    }
  }

  // Dictionary info
  const mecab_dictionary_info_t *d = mecab_dictionary_info(mecab);
  for (; d; d = d->next) {
    printf("filename: %s\n", d->filename);
    printf("charset: %s\n", d->charset);
    printf("size: %d\n", d->size);
    printf("type: %d\n", d->type);
    printf("lsize: %d\n", d->lsize);
    printf("rsize: %d\n", d->rsize);
    printf("version: %d\n", d->version);
  }

  mecab_destroy(mecab);

  return 0;
}
