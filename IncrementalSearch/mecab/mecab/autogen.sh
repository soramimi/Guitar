#!/bin/sh

[ -f configure.in ] || {
  echo "autogen.sh: run this command only at the top of a MeCab source tree."
  exit 1
}

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have autoconf installed to compile MeCab."
  echo "Get ftp://ftp.gnu.org/pub/gnu/autoconf/autoconf-2.13.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
  NO_AUTOCONF=yes
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have automake installed to compile MeCab."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.4.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
  NO_AUTOMAKE=yes
}

# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing \`aclocal'.  The version of \`automake'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.4.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
}

# if no autoconf, don't bother testing for autoheader
test -n "$NO_AUTOCONF" || (autoheader --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing \`autoheader'.  The version of \`autoheader'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/autoconf/autoconf-2.13.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
}

(grep "^AM_PROG_LIBTOOL" configure.in >/dev/null) && {
  (libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed to compile Gnome."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.3.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

if test "$DIE" -eq 1; then
        exit 1
fi

if test -z "$*"; then
  echo "**Warning**: I am going to run \`configure' with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo \`$0\'" command line."
  echo
fi

echo "Generating configure script and Makefiles for MeCab."

if grep "^AM_PROG_LIBTOOL" configure.in >/dev/null; then
  echo "Running libtoolize..."
  libtoolize --force --copy
fi
echo "Running aclocal ..."
aclocal -I .
echo "Running autoheader..."
autoheader
echo "Running automake ..."
automake --add-missing
echo "Running autoconf ..."
autoconf

echo "Configuring MeCab."
conf_flags="--enable-maintainer-mode"
echo Running $./configure $conf_flags "$@" ...
./configure $conf_flags "$@"
echo "Now type 'make' to compile MeCab."
