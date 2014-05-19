#ifndef __CRB_LOG_H__
#define __CRB_LOG_H__ 1

typedef enum {
     HK_LOG_NONE = 0,
     HK_LOG_MARK = 1 << 1,
     HK_LOG_INFO = 1 << 2,
     HK_LOG_DEBUG = 1 << 3,
     HK_LOG_ERROR = 1 << 4,
     HK_LOG_ALL = 0xF
} hk_log_level_e;

typedef enum {
     HK_LOG_STDERR = 1 << 1,
     HK_LOG_FILE = 1 << 2
} hk_log_destination_e;

int hk_log_init(hk_log_level_e level, hk_log_destination_e destination, const char *file_path);
void hk_log_info(char *msg);
void hk_log_debug(char *msg);
void hk_log_error(char *msg);
void hk_log_mark(char *msg);

#endif /* __CRB_LOG_H__ */
