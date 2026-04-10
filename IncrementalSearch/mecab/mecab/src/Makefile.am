AUTOMAKE_OPTIONS = no-dependencies
lib_LTLIBRARIES = libmecab.la
EXTRA_DIST = Makefile.msvc.in make.bat
pkglibexecdir = ${libexecdir}/mecab
INCLUDES = -DDIC_VERSION=$(DIC_VERSION) $(MECAB_NO_TLS) $(MECAB_USE_UTF8_ONLY) -DMECAB_DEFAULT_RC="\"$(MECAB_DEFAULT_RC)\""
libmecab_la_LDFLAGS = -no-undefined -version-info $(LTVERSION)
libmecab_la_SOURCES = viterbi.cpp tagger.cpp  utils.cpp utils.h eval.cpp iconv_utils.cpp iconv_utils.h \
                      dictionary_rewriter.h dictionary_rewriter.cpp dictionary_generator.cpp \
	              dictionary_compiler.cpp context_id.h context_id.cpp \
                      winmain.h thread.h connector.cpp nbest_generator.h nbest_generator.cpp connector.h \
                      writer.h writer.cpp mmap.h ucs.h  \
	              string_buffer.h string_buffer.cpp \
		      tokenizer.h stream_wrapper.h common.h darts.h char_property.h ucstable.h \
			freelist.h viterbi.h param.cpp tokenizer.cpp \
			ucstable.h char_property.cpp dictionary.h scoped_ptr.h \
			param.h mecab.h dictionary.cpp \
			feature_index.cpp  feature_index.h  lbfgs.cpp \
			lbfgs.h  learner_tagger.cpp  learner_tagger.h  learner.cpp  \
			learner_node.h libmecab.cpp

include_HEADERS = mecab.h
bin_PROGRAMS    = mecab
pkglibexec_PROGRAMS = mecab-dict-index mecab-dict-gen mecab-cost-train mecab-system-eval mecab-test-gen

mecab_dict_index_SOURCES = mecab-dict-index.cpp
mecab_dict_index_LDADD = libmecab.la

mecab_dict_gen_SOURCES = mecab-dict-gen.cpp
mecab_dict_gen_LDADD = libmecab.la

mecab_system_eval_SOURCES = mecab-system-eval.cpp
mecab_system_eval_LDADD = libmecab.la

mecab_cost_train_SOURCES = mecab-cost-train.cpp
mecab_cost_train_LDADD = libmecab.la

mecab_test_gen_SOURCES = mecab-test-gen.cpp
mecab_test_gen_LDADD = libmecab.la

mecab_SOURCES = mecab.cpp
mecab_LDADD = libmecab.la
