#ifndef __HK_WS_H__
#define __HK_WS_H__ 1

#include "hk_buffer.h"

#define HK_WS_KEY "SEC-WEBSOCKET-KEY"
#define HK_WS_VERSION "SEC-WEBSOCKET-VERSION"
#define HK_WS_PROTOCOL "SEC-WEBSOCKET-PROTOCOL"
#define HK_WS_EXTENSIONS "SEC-WEBSOCKET-EXTENSIONS"
#define HK_WS_ORIGIN "ORIGIN"

#define HK_PARSE_DONE					1
#define HK_VALIDATE_DONE				1
#define HK_UNMASK_DONE					1
#define HK_PARSE_INCOMPLETE			10
#define HK_ERROR_INVALID_METHOD		11
#define HK_ERROR_INVALID_REQUEST		12
#define HK_ERROR_INVALID_OPCODE		13
#define HK_ERROR_CRITICAL				14

#define HK_WS_CONT_FRAME		0
#define HK_WS_TEXT_FRAME		1
#define HK_WS_BIN_FRAME		2
#define HK_WS_CLOSE_FRAME		8
#define HK_WS_PING_FRAME		9
#define HK_WS_PONG_FRAME		10
#define HK_WS_IS_CONTROL_FRAME	8

#define HK_WS_TYPE_DATA	0
#define HK_WS_TYPE_CONTROL	1

typedef enum {
	HK_UTF8_NONE,
	HK_UTF8_CHAR,
	HK_UTF8_3_OCTETS_1,
	HK_UTF8_3_OCTETS_2,
	HK_UTF8_4_OCTETS_1,
	HK_UTF8_4_OCTETS_2,
	HK_UTF8_TAIL,
	HK_UTF8_TAIL_X2,
	HK_UTF8_TAIL_X3,
} hk_utf8_state_e;

typedef struct hk_ws_frame_s hk_ws_frame_t;
struct hk_ws_frame_s {
	uint8_t rsv;
	uint64_t payload_len;
	uint8_t opcode;
	uint8_t hk_type;
	union {
		uint32_t raw;
		uint8_t octets[4];
	} mask;
	char *data;
	uint64_t data_length;
	hk_utf8_state_e utf8_state;
	
	unsigned is_masked:1;
	unsigned is_fin:1;
};

hk_ws_frame_t *hk_ws_frame_init();
int hk_ws_frame_parse_buffer(hk_ws_frame_t *frame, hk_buffer_t *buffer);
void hk_ws_frame_free(hk_ws_frame_t *frame);
void hk_ws_frame_free_with_data(hk_ws_frame_t *frame);

uint8_t *hk_ws_generate_close_frame(int *frame_length, int masked);
uint8_t *hk_ws_generate_frame_head(uint64_t data_length, int *head_length, int masked, uint8_t opcode);


#endif /* __HK_WS_H__ */
