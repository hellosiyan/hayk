bin_PROGRAMS = caribou

caribou_SOURCES =      \
    main.c             \
    crb_buffer.c       \
    crb_buffer.h       \
    crb_server.c       \
    crb_server.h       \
    crb_task.c         \
    crb_task.h         \
    crb_client.c       \
    crb_client.h       \
    crb_reader.c       \
    crb_reader.h       \
    crb_worker.c       \
    crb_worker.h       \
    crb_channel.c      \
    crb_channel.h      \
    crb_sender.c       \
    crb_sender.h       \
    crb_hash.c         \
    crb_hash.h         \
    crb_list.c         \
    crb_list.h         \
    crb_request.c      \
    crb_request.h      \
    crb_crypt.c	       \
    crb_crypt.h        \
    crb_config.c	   \
    crb_config.h       \
    crb_ws.c	       \
    crb_ws.h       	   \
    crb_atomic.h        

caribou_CFLAGS =       \
    -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
    -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"\
    -DPACKAGE_CONF_FILE=\""$(sysconfdir)/caribou.cfg"\"

caribou_LDADD = \
	-lssl \
	-lcrypto\
	-lconfig
