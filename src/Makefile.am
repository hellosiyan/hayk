SUBDIRS = lib

bin_PROGRAMS = hayk

hayk_SOURCES =      \
    main.c          \
    hk_server.c     \
    hk_server.h

hayk_CFLAGS =       \
    -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"\
    -DPACKAGE_CONF_FILE=\""$(sysconfdir)/hayk.cfg"\"

hayk_LDADD = \
    lib/libhayk.a \
	-lssl    \
	-lcrypto \
	-lconfig 
