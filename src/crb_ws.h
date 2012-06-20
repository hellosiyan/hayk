#ifndef __CRB_WS_H__
#define __CRB_WS_H__ 1

#define CRB_WS_CONT_FRAME	0
#define CRB_WS_TEXT_FRAME	1
#define CRB_WS_BIN_FRAME	2
#define CRB_WS_CLOSE_FRAME	8
#define CRB_WS_PING_FRAME	9
#define CRB_WS_PONG_FRAME	10

typedef struct crb_ws_frame_s crb_ws_frame_t;
struct crb_ws_frame_s {
	uint64_t payload_len;
	uint8_t opcode;
	union {
		uint32_t raw;
		uint8_t octets[4];
	} mask;
	char *data;
	uint64_t data_length;
	
	unsigned is_masked:1;
};

crb_ws_frame_t *crb_ws_frame_init();
void crb_ws_frame_free(crb_ws_frame_t *frame);


#endif /* __CRB_WS_H__ */
