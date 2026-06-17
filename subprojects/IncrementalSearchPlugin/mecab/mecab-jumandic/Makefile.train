CORPUS  = kc.train
TEST    = kc.test
MODEL   = model-jumandic-5.1
SEEDDIR = ../
ALGO = crf
FREQ   = 1
C      = 1.0
EVAL   = 0 1 2 4

PREFIX = /usr/local
LIBEXECDIR  = $(PREFIX)/libexec/mecab

BINDIR      = $(PREFIX)/bin
RMODEL      = $(MODEL).c$(C).f$(FREQ)
DICDIR      = $(RMODEL).dic

mkseed:
	$(LIBEXECDIR)/mecab-dict-index -d $(SEEDDIR) -o $(SEEDDIR)

train:
	$(LIBEXECDIR)/mecab-cost-train -a $(ALGO) -y -c $(C) -d $(SEEDDIR) -f $(FREQ) $(CORPUS) $(RMODEL).model
	$(LIBEXECDIR)/mecab-cost-train -d $(SEEDDIR) -b $(RMODEL).model.txt $(RMODEL).model

mkdic:
	mkdir -p $(DICDIR)
	$(LIBEXECDIR)/mecab-dict-gen   -a $(ALGO) -d $(SEEDDIR) -m $(RMODEL).model -o $(DICDIR)
	$(LIBEXECDIR)/mecab-dict-index -d $(DICDIR) -o $(DICDIR)

test:
	$(LIBEXECDIR)/mecab-test-gen < $(TEST) | $(BINDIR)/mecab -r /dev/null -d $(DICDIR)  > $(RMODEL).result
	$(LIBEXECDIR)/mecab-system-eval -l "$(EVAL)" $(RMODEL).result $(TEST) | tee $(RMODEL).score
	
test-tool:	
	$(LIBEXECDIR)/mecab-test-gen < $(TEST) | $(LIBEXECDIR)/mecab-tool --test -m $(RMODEL).model > $(RMODEL).tool.result
	$(LIBEXECDIR)/mecab-system-eval -l "($EVAL)" $(RMODEL).tool.result $(TEST) | tee $(RMODEL).tool.score
	
clean:
	rm -f $(RMODEL)* $(DICDIR)
