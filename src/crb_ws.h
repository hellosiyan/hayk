#ifndef __CRB_WS_H__
#define __CRB_WS_H__ 1

#include "crb_buffer.h"

#define CRB_WS_KEY "SEC-WEBSOCKET-KEY"
#define CRB_WS_VERSION "SEC-WEBSOCKET-VERSION"
#define CRB_WS_PROTOCOL "SEC-WEBSOCKET-PROTOCOL"
#define CRB_WS_EXTENSIONS "SEC-WEBSOCKET-EXTENSIONS"
#define CRB_WS_ORIGIN "ORIGIN"

#define CRB_PARSE_DONE					1
#define CRB_VALIDATE_DONE				1
#define CRB_UNMASK_DONE					1
#define CRB_PARSE_INCOMPLETE			10
#define CRB_ERROR_INVALID_METHOD		11
#define CRB_ERROR_INVALID_REQUEST		12
#define CRB_ERROR_INVALID_OPCODE		13
#define CRB_ERROR_CRITICAL				14

#define CRB_WS_CONT_FRAME		0
#define CRB_WS_TEXT_FRAME		1
#define CRB_WS_BIN_FRAME		2
#define CRB_WS_CLOSE_FRAME		8
#define CRB_WS_PING_FRAME		9
#define CRB_WS_PONG_FRAME		10
#define CRB_WS_IS_CONTROL_FRAME	8

#define CRB_WS_TYPE_DATA	0
#define CRB_WS_TYPE_CONTROL	1

typedef enum {
	CRB_UTF8_NONE,
	CRB_UTF8_CHAR,
	CRB_UTF8_3_OCTETS_1,
	CRB_UTF8_3_OCTETS_2,
	CRB_UTF8_4_OCTETS_1,
	CRB_UTF8_4_OCTETS_2,
	CRB_UTF8_TAIL,
	CRB_UTF8_TAIL_X2,
	CRB_UTF8_TAIL_X3,
} crb_utf8_state_e;

typedef struct crb_ws_frame_s crb_ws_frame_t;
struct crb_ws_frame_s {
	uint8_t rsv;
	uint64_t payload_len;
	uint8_t opcode;
	uint8_t crb_type;
	union {
		uint32_t raw;
		uint8_t octets[4];
	} mask;
	char *data;
	uint64_t data_length;
	crb_utf8_state_e utf8_state;
	
	unsigned is_masked:1;
	unsigned is_fin:1;
};

crb_ws_frame_t *crb_ws_frame_init();
int crb_ws_frame_parse_buffer(crb_ws_frame_t *frame, crb_buffer_t *buffer);
crb_ws_frame_t *crb_ws_frame_create_from_data(char *data, uint64_t data_length, int masked);
uint8_t *crb_ws_frame_head_from_data(uint8_t *data, uint64_t data_length, int *length, int masked, uint8_t opcode);
uint8_t *crb_ws_frame_close(int *length, int masked);
void crb_ws_frame_free(crb_ws_frame_t *frame);
void crb_ws_frame_free_with_data(crb_ws_frame_t *frame);


#endif /* __CRB_WS_H__ */
