#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <mecab.h>
#include <stdlib.h>
#include "../src/thread.h"

namespace MeCab {

class TaggerThread : public thread {
 public:
  void run() {
    int n = 0;
    while (true) {
      for (size_t i = 0; i < sentences_->size(); ++i) {
        lattice_->set_sentence((*sentences_)[i].c_str());
        tagger_->parse(lattice_);
        if (n % 100141 == 0) {
          std::cout << id_ << " " << n << " parsed" << std::endl;
//	  std::cout << lattice_->toString();
        }
        ++n;
      }
    }
  }

  TaggerThread(std::vector<std::string> *sentences,
               MeCab::Model *model,
               int id)
      : sentences_(sentences), tagger_(0), id_(id) {
    tagger_ = model->createTagger();
    lattice_ = model->createLattice();
  }

  ~TaggerThread() {
    delete tagger_;
    delete lattice_;
  }

 private:
  std::vector<std::string> *sentences_;
  MeCab::Tagger *tagger_;
  MeCab::Lattice *lattice_;
  int id_;
};

class ModelUpdater : public thread {
 public:
  void run() {
    const char *kParams[] = {
#ifdef _WIN32
      "-d unidic",
      "-d ipadic",
      "-d jumandic"
#else
      "-d /usr/local/lib/mecab/dic/unidic/",
      "-d /usr/local/lib/mecab/dic/ipadic/",
      "-d /usr/local/lib/mecab/dic/jumandic/"
#endif
    };

    int i = 0;
    while (true) {
#ifdef _WIN32
      ::Sleep(4000);
#else
      sleep(4);
#endif
      MeCab::Model *model = MeCab::createModel(kParams[i % 3]);
      std::cout << "Updating..." << std::endl;
      if (!model_->swap(model)) {
        std::cerr << "cannot swap" << std::endl;
        exit(-1);
      }
      std::cout << "Done!" << std::endl;
      ++i;
    }
  }

  ModelUpdater(MeCab::Model *model) : model_(model) {}

 private:
  MeCab::Model *model_;
};
}

int main (int argc, char **argv) {
  std::ifstream ifs("japanese_sentences.txt");
  std::string line;
  std::vector<std::string> sentences;
  while (std::getline(ifs, line)) {
    sentences.push_back(line);
    if (sentences.size() == 10000) {
      break;
    }
  }

  MeCab::Model *model = MeCab::createModel(argc, argv);
  if (!model) {
    std::cerr << "model is NULL" << std::endl;
    return -1;
  }
  MeCab::ModelUpdater updater(model);
  updater.start();

  const int kMaxThreadSize = 8;

  std::vector<MeCab::TaggerThread *> threads(kMaxThreadSize);
  for (int i = 0; i < kMaxThreadSize; ++i) {
    threads[i] = new MeCab::TaggerThread(&sentences, model, i);
  }

  for (int i = 0; i < kMaxThreadSize; ++i) {
    threads[i]->start();
  }

  for (int i = 0; i < kMaxThreadSize; ++i) {
    threads[i]->join();
    delete threads[i];
  }

  updater.join();
   
  delete model;

  return 0;
}
