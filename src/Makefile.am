bin_PROGRAMS = hayk

hayk_SOURCES =      \
    main.c             \
    hk_buffer.c       \
    hk_buffer.h       \
    hk_server.c       \
    hk_server.h       \
    hk_task.c         \
    hk_task.h         \
    hk_client.c       \
    hk_client.h       \
    hk_reader.c       \
    hk_reader.h       \
    hk_worker.c       \
    hk_worker.h       \
    hk_channel.c      \
    hk_channel.h      \
    hk_sender.c       \
    hk_sender.h       \
    hk_hash.c         \
    hk_hash.h         \
    hk_list.c         \
    hk_list.h         \
    hk_request.c      \
    hk_request.h      \
    hk_crypt.c	       \
    hk_crypt.h        \
    hk_config.c	   \
    hk_config.h       \
    hk_ws.c	       \
    hk_ws.h       	   \
    hk_log.c	       \
    hk_log.h		   \
    hk_atomic.h        

hayk_CFLAGS =       \
    -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"\
    -DPACKAGE_CONF_FILE=\""$(sysconfdir)/hayk.cfg"\"

hayk_LDADD = \
	-lssl \
	-lcrypto\
	-lconfig
