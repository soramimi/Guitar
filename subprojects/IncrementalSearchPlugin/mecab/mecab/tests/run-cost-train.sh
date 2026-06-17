#!/bin/sh

cd cost-train

CORPUS=ipa.train
TEST=ipa.test
MODEL=model-ipadic
SEEDDIR=seed
FREQ=1
C=1.0
EVAL="0 1 2 4"

DIR=../../src
#DIR=/usr/local/libexec/mecab
RMODEL=${MODEL}.c${C}.f${FREQ}
DICDIR=${RMODEL}.dic

mkdir ${DICDIR}
${DIR}/mecab-dict-index -d ${SEEDDIR} -o ${SEEDDIR}
${DIR}/mecab-cost-train -c ${C} -d ${SEEDDIR} -f ${FREQ} ${CORPUS} ${RMODEL}.model
${DIR}/mecab-dict-gen   -d ${SEEDDIR} -m ${RMODEL}.model -o ${DICDIR}
${DIR}/mecab-dict-index -d ${DICDIR} -o ${DICDIR}
${DIR}/mecab-test-gen < ${TEST} | ${DIR}/mecab -r /dev/null -d ${DICDIR}  > ${RMODEL}.result
${DIR}/mecab-system-eval -l "${EVAL}" ${RMODEL}.result ${TEST} | tee ${RMODEL}.score

rm -fr ${DICDIR}
rm -fr ${RMODEL}*
rm -fr ${SEEDDIR}/*.dic
rm -fr ${SEEDDIR}/*.bin
rm -fr ${SEEDDIR}/*.dic

exit 0
