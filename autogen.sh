#! /bin/sh
touch ChangeLog
aclocal \
&& automake --add-missing \
&& autoconf
