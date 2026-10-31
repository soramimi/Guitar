TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += worker tests
worker.file = ../conpty-worker.pro
tests.file = tests.pro
tests.depends = worker
