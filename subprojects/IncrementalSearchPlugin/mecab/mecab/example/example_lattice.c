#include <mecab.h>
#include <stdio.h>

#define CHECK(eval) if (! eval) { \
    fprintf (stderr, "Exception:%s\n", mecab_strerror (mecab)); \
    mecab_destroy(mecab); \
    return -1; }

int main (int argc, char **argv)  {
  char input[] = "太郎は次郎が持っている本を花子に渡した。";
  mecab_model_t *model, *another_model;
  mecab_t *mecab;
  mecab_lattice_t *lattice;
  const mecab_node_t *node;
  const char *result;
  int i;
  size_t len;

  model = mecab_model_new(argc, argv);
  CHECK(model);

  mecab = mecab_model_new_tagger(model);
  CHECK(mecab);

  lattice = mecab_model_new_lattice(model);
  CHECK(lattice);

  mecab_lattice_set_sentence(lattice, input);
  mecab_parse_lattice(mecab, lattice);

  printf("RESULT: %s\n", mecab_lattice_tostr(lattice));

  node = mecab_lattice_get_bos_node(lattice);
  for (;  node; node = node->next) {
    printf("%d ", node->id);

    if (node->stat == MECAB_BOS_NODE)
      printf("BOS");
    else if (node->stat == MECAB_EOS_NODE)
      printf("EOS");
    else
      fwrite (node->surface, sizeof(char), node->length, stdout);

    printf(" %s %d %d %d %d %d %d %d %d %f %f %f %ld\n",
	   node->feature,
	   (int)(node->surface - input),
	   (int)(node->surface - input + node->length),
	   node->rcAttr,
	   node->lcAttr,
	   node->posid,
	   (int)node->char_type,
	   (int)node->stat,
	   (int)node->isbest,
	   node->alpha,
	   node->beta,
	   node->prob,
	   node->cost);
  }

  len = mecab_lattice_get_size(lattice);
  for (i = 0; i <= len; ++i) {
    mecab_node_t *b, *e;
    b = mecab_lattice_get_begin_nodes(lattice, (size_t)i);
    e = mecab_lattice_get_end_nodes(lattice, (size_t)i);
    for (; b; b = b->bnext) {
        printf("B[%d] %s\t%s\n", i, b->surface, b->feature);
    }
    for (; e; e = e->enext) {
        printf("E[%d] %s\t%s\n", i, e->surface, e->feature);
    }
  }

  mecab_lattice_set_sentence(lattice, input);
  mecab_lattice_set_request_type(lattice, MECAB_NBEST);
  mecab_parse_lattice(mecab, lattice);
  for (i = 0; i < 10; ++i) {
    fprintf(stdout, "%s", mecab_lattice_tostr(lattice));
    if (!mecab_lattice_next(lattice)) {
      break;
    }
  }

  mecab_lattice_set_sentence(lattice, input);
  mecab_lattice_set_request_type(lattice, MECAB_MARGINAL_PROB);
  mecab_lattice_set_theta(lattice, 0.001);
  mecab_parse_lattice(mecab, lattice);
  node = mecab_lattice_get_bos_node(lattice);
  for (;  node; node = node->next) {
    fwrite(node->surface, sizeof(char), node->length, stdout);
    fprintf(stdout, "\t%s\t%f\n", node->feature, node->prob);
  }

  mecab_set_lattice_level(mecab, 0);
  mecab_set_all_morphs(mecab, 1);
  node = mecab_sparse_tonode(mecab, input);
  CHECK(node);
  for (; node; node = node->next) {
    fwrite (node->surface, sizeof(char), node->length, stdout);
    printf("\t%s\n", node->feature);
  }

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
  mecab_lattice_destroy(lattice);
  mecab_model_destroy(model);

  return 0;
}
