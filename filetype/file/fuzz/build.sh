#!/bin/bash -eu
# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

: ${SRC:=.}
: ${OUT:=.}
: ${CC:=cc}
: ${CFLAGS:=-O -DHAVE_CONFIG_H -Wall}

#(cd .. && autoreconf -i && ./configure --enable-static && make V=1 all)

"$CC" $CFLAGS -I../src/ -I.. \
     "$SRC/magic_fuzzer.c" -o "$OUT/magic_fuzzer" \
     -lFuzzingEngine ../src/.libs/libmagic.a

cp ../magic/magic.mgc "$OUT/magic.mgc"
