#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hk_ws.h"
#include "hk_buffer.h"

#define hk_strcmp2(s, c0, c1) \
    s[0] == c0 && s[1] == c1
    
#define hk_strcmp3(s, c0, c1, c2) \
    s[0] == c0 && s[1] == c1 && s[2] == c2

#define hk_strcmp8(s, c0, c1, c2, c3, c4, c5, c6, c7) \
    s[0] == c0 && s[1] == c1 && s[2] == c2 && s[3] == c3 \
        && s[4] == c4 && s[5] == c5 && s[6] == c6 && s[7] == c7

static inline int hk_ws_frame_unmask(hk_ws_frame_t *frame, char *stream);
static inline int hk_ws_frame_unmask_utf8_stream(hk_ws_frame_t *frame, char *stream);

hk_ws_frame_t *
hk_ws_frame_init()
{
	hk_ws_frame_t *frame;
	
	frame = malloc(sizeof(hk_ws_frame_t));
	if ( frame == NULL ) {
		return NULL;
	}
	
	frame->payload_len = 0;
	frame->opcode = 0;
	frame->mask.raw = 0;
	
	frame->is_masked = 0;
	
	frame->data = NULL;
	frame->data_length = 0;
	frame->utf8_state = HK_UTF8_NONE;
	
	return frame;
}

uint8_t *
hk_ws_generate_frame_head(uint64_t data_length, int *head_length, int masked, uint8_t opcode)
{
	uint8_t *pos, *frame_data;
	uint64_t payload_len;
	int is_masked;
	union {
		uint32_t raw;
		uint8_t octets[4];
	} mask;
		
	// define frame properties 
	payload_len = data_length;
	
	if ( masked ) {
		is_masked = 1;
		mask.raw = rand();
	} else {
		is_masked = 0;
	}
	
	// define data length
	*head_length = 2;
	
	if ( payload_len < 126 ) {
		// pass
	} else if (payload_len < 65536 ) {
		*head_length += 2;
	} else {
		*head_length += 8;
	}
	
	if ( is_masked ) {
		 *head_length += 4;
	}
	
	frame_data = malloc(*head_length * sizeof(uint8_t));
	if ( frame_data == NULL ) {
		return NULL;
	}
	
	pos = frame_data;
	
	// Opcode, rsv, fin
	*pos = (opcode&15) | 0b10000000;
	pos += 1;
	
	// Payload length
	if ( payload_len < 126 ) {
		*pos = (payload_len&127) | (is_masked << 7);
	} else if (payload_len < 65536 ) {
		*pos = 126 | (is_masked << 7);
		pos += 1;
		*(uint16_t*)pos = htons(payload_len);
		pos += 2;
	} else {
		*pos = 127 | (is_masked << 7);
		pos += 1;

		*((uint64_t*)pos) = ntohl((uint32_t)payload_len);
		*((uint64_t*)pos) <<= 32;
		*((uint64_t*)pos) += ntohl( * (((uint32_t *) (&payload_len))+1) );

		pos += 8;
	}
	
	// Mask key
	
	if ( is_masked ) {
		*((uint32_t*)pos) = mask.raw;
		pos += 4;
	}
	
	return frame_data;
}

uint8_t *
hk_ws_generate_close_frame(int *frame_length, int masked)
{
	uint8_t *pos, *data;
	
	uint64_t payload_len;
	uint8_t opcode;
	union {
		uint32_t raw;
		uint8_t octets[4];
	} mask;
	int is_masked;
	
	// define frame properties 
	opcode = HK_WS_CLOSE_FRAME;
	
	if ( masked ) {
		is_masked = 1;
		mask.raw = rand();
	} else {
		is_masked = 0;
	}
	
	// define data length
	*frame_length = 2;
	
	if ( is_masked ) {
		 *frame_length += 4;
	}
	
	data = malloc(*frame_length * sizeof(uint8_t));
	if ( data == NULL ) {
		return NULL;
	}
	
	pos = data;
	
	// Opcode, rsv, fin
	*pos = (opcode&15) | 0b10000000;
	pos += 1;
	
	// Payload length
	*pos = 0 | (is_masked << 7);
	
	// Mask key
	if ( is_masked ) {
		*((uint32_t*)pos) = mask.raw;
		pos += 4;
	}
	
	return data;
}


int 
hk_ws_frame_parse_buffer(hk_ws_frame_t *frame, hk_buffer_t *buffer)
{
	uint16_t raw = 0;
	char *read_pos = NULL;
	
	if ( frame == NULL || buffer == NULL ) {
		return HK_PARSE_INCOMPLETE;
	}
	
	read_pos = buffer->rpos;
	
	if ( buffer->used < 2 ) {
		return HK_PARSE_INCOMPLETE;
	}
	
	raw = * (uint16_t*) read_pos;
	
	// Opcode
	switch( raw & 15 ) {
		case HK_WS_CONT_FRAME: frame->opcode = HK_WS_CONT_FRAME; break;
		case HK_WS_TEXT_FRAME: frame->opcode = HK_WS_TEXT_FRAME; break;
		case HK_WS_BIN_FRAME: frame->opcode = HK_WS_BIN_FRAME; break;
		case HK_WS_CLOSE_FRAME: frame->opcode = HK_WS_CLOSE_FRAME; break;
		case HK_WS_PING_FRAME: frame->opcode = HK_WS_PING_FRAME; break;
		case HK_WS_PONG_FRAME: frame->opcode = HK_WS_PONG_FRAME; break;
		default:
			return HK_ERROR_INVALID_OPCODE;
	}

	// RSV bits
	frame->rsv = (raw&112)>>4;

	// Fin
	frame->is_fin = (raw&128)>>7;
	
	// Mask
	if ( raw & 256 == 0 ) {
		frame->is_masked = 0; 
	} else {
		frame->is_masked = 1; 
	}
	
	// Payload Length
	frame->payload_len = (raw >> 8)&127;
	
	if ( frame->payload_len == 126 ) {
		// 16bit length
		// require the next 2 bytes
		if ( buffer->used < 4 ) {
			return HK_PARSE_INCOMPLETE;
		}
		
		frame->payload_len = ntohs(*(uint16_t *) (read_pos + 2));
		
		if ( frame->payload_len < 4 && frame->opcode == HK_WS_TEXT_FRAME ) {
			return HK_PARSE_INCOMPLETE;
		}
		
		read_pos = read_pos + 4; 
	} else if( frame->payload_len == 127 ) {
		// 64bit length
		// require the next 8 bytes
		if ( buffer->used < 10 ) {
			return HK_PARSE_INCOMPLETE;
		}
		
		frame->payload_len = ntohl(* (uint32_t *) (read_pos + 2));
		frame->payload_len <<= 32;
		frame->payload_len += ntohl(* (uint32_t *) (read_pos + 6));
		
		if ( frame->payload_len < 4 && frame->opcode == HK_WS_TEXT_FRAME ) {
			return HK_PARSE_INCOMPLETE;
		}
		
		read_pos = read_pos + 10; 
	} else {
		read_pos = read_pos + 2; 
	}
	
	// Masking key
	if ( frame->is_masked ) {
		if ( buffer->used < (read_pos + 4) - buffer->ptr ) {
			return HK_PARSE_INCOMPLETE;
		}
	
		frame->mask.raw = * (uint32_t *) read_pos;
		read_pos = read_pos + 4;
	}
	
	// Payload
	if ( buffer->used <= (read_pos + frame->payload_len) - buffer->ptr ) {
		return HK_PARSE_INCOMPLETE;
	}
	
	// Payload / Unmask 
	if( frame->opcode == HK_WS_TEXT_FRAME || frame->opcode == HK_WS_BIN_FRAME || frame->opcode == HK_WS_CONT_FRAME  ) {
		int unmask_result;

		unmask_result = hk_ws_frame_unmask_utf8_stream(frame, read_pos);
		if ( unmask_result != HK_UNMASK_DONE ) {
			return unmask_result;
		}
		
		frame->data = malloc(frame->payload_len + 1);
		if ( frame->data == NULL ) {
			hk_log_error("Cannot allocate memory for payload");
			return HK_ERROR_CRITICAL;
		}
		
		frame->data = memcpy(frame->data, read_pos, frame->payload_len);
		frame->data_length = frame->payload_len;
		frame->data[frame->data_length] = '\0';
	} else if( frame->opcode == HK_WS_PING_FRAME  ) {
		if ( frame->payload_len > 125 ) {
			// Control frames are allowed payload up to 125 bytes
			return HK_ERROR_CRITICAL;
		}
		
		hk_ws_frame_unmask(frame, read_pos);
		
		frame->data = malloc(frame->payload_len + 1);
		if ( frame->data == NULL ) {
			hk_log_error("Cannot allocate memory for payload");
			return HK_ERROR_CRITICAL;
		}

		frame->data = memcpy(frame->data, read_pos, frame->payload_len);
		frame->data_length = frame->payload_len;
		frame->data[frame->data_length] = '\0';
	}
	
	read_pos = read_pos + frame->payload_len;
	buffer->rpos = read_pos;
	
	return HK_PARSE_DONE;
}

void 
hk_ws_frame_free(hk_ws_frame_t *frame)
{
	if ( frame == NULL ) {
		return;
	}
	
	free(frame);
}

void
hk_ws_frame_free_with_data(hk_ws_frame_t *frame)
{
	if ( frame == NULL ) {
		return;
	}
	
	if ( frame->data != NULL ) {
		free(frame->data);
		frame->data = NULL;
	}
	
	free(frame);
}


static inline int
hk_ws_frame_unmask(hk_ws_frame_t *frame, char *stream)
{
	int i, j;
	uint8_t ch;

	for (i = 0; i < frame->payload_len; i += 1) {
		j = i%4;
		ch = (*(u_char*)(stream+i))^frame->mask.octets[j];
		*(stream+i) = ch;
	}

	return HK_UNMASK_DONE;
}

static inline int 
hk_ws_frame_unmask_utf8_stream(hk_ws_frame_t *frame, char *stream)
{
	int i, j;
	uint8_t ch;	
	hk_utf8_state_e expect;

	switch(frame->opcode) {
		case HK_WS_TEXT_FRAME: expect = HK_UTF8_CHAR; break;
		case HK_WS_BIN_FRAME: expect = HK_UTF8_NONE; break;
		case HK_WS_CONT_FRAME: expect = frame->utf8_state; break;
	}

	for (i = 0; i < frame->payload_len; i += 1) {
		j = i%4;
		ch = (*(u_char*)(stream+i))^frame->mask.octets[j];
		*(stream+i) = ch;

		/* none */
		if ( expect == HK_UTF8_NONE ) {
			// continue;

		/* utf8 code */
		} else if ( expect == HK_UTF8_CHAR ) {
			/* utf8 1 octet */
			if ( ch <= 0x7F ) { // ascii
				expect = HK_UTF8_CHAR;
			/* utf8 2 octets */
			} else if ( (ch >= 0xC2 && ch <= 0xDF) ) {
				expect = HK_UTF8_TAIL;
			/* utf 3 octets */
			} else if ( ch == 0xE0 ) {
				expect = HK_UTF8_3_OCTETS_1;
			} else if ( (ch >= 0xE1 && ch <= 0xEC) ) {
				expect = HK_UTF8_TAIL_X2;
			} else if ( ch == 0xED ) {
				expect = HK_UTF8_3_OCTETS_2;
			} else if ( (ch >= 0xEE && ch <= 0xEF) ) {
				expect = HK_UTF8_TAIL_X2;
			/* utf 4 octets */
			} else if ( ch == 0xF0 ) {
				expect = HK_UTF8_4_OCTETS_1;
			} else if ( (ch >= 0xF1 && ch <= 0xF3) ) {
				expect = HK_UTF8_TAIL_X3;
			} else if ( ch == 0xF4 ) {
				expect = HK_UTF8_4_OCTETS_2;
			} else {
				hk_log_error("Invalid UTF-8 sequence");
				return HK_ERROR_CRITICAL;
			}
			
		/* utf8 3 octets */
		} else if ( expect == HK_UTF8_3_OCTETS_1 && (ch >= 0xA0 && ch <= 0xBF) ) {
			expect = HK_UTF8_TAIL;
		} else if ( expect == HK_UTF8_3_OCTETS_2 && (ch >= 0x80 && ch <= 0x9F) ) {
			expect = HK_UTF8_TAIL;

		/* utf8 4 octets */
		} else if ( expect == HK_UTF8_4_OCTETS_1 && (ch >= 0x90 && ch <= 0xBF) ) {
			expect = HK_UTF8_TAIL_X2;
		} else if ( expect == HK_UTF8_4_OCTETS_2 && (ch >= 0x80 && ch <= 0x8F) ) {
			expect = HK_UTF8_TAIL_X2;
		
		/* utf8 tail */
		} else if ( expect == HK_UTF8_TAIL && (ch >= 0x80 && ch <= 0xBF) ) {
			expect = HK_UTF8_CHAR;
		} else if ( expect == HK_UTF8_TAIL_X2 && (ch >= 0x80 && ch <= 0xBF) ) {
			expect = HK_UTF8_TAIL;
		} else if ( expect == HK_UTF8_TAIL_X3 && (ch >= 0x80 && ch <= 0xBF) ) {
			expect = HK_UTF8_TAIL_X2;
		} else {
			hk_log_error("Invalid UTF-8 sequence");
			return HK_ERROR_CRITICAL;
		}
	}

	if ( frame->is_fin && expect != HK_UTF8_CHAR && expect != HK_UTF8_NONE ) {
		hk_log_error("Invalid UTF-8 sequence");
		return HK_ERROR_CRITICAL;
	}

	frame->utf8_state = expect;

	return HK_UNMASK_DONE;
}