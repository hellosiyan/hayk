#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "crb_ws.h"
#include "crb_buffer.h"

#define crb_strcmp2(s, c0, c1) \
    s[0] == c0 && s[1] == c1
    
#define crb_strcmp3(s, c0, c1, c2) \
    s[0] == c0 && s[1] == c1 && s[2] == c2

#define crb_strcmp8(s, c0, c1, c2, c3, c4, c5, c6, c7) \
    s[0] == c0 && s[1] == c1 && s[2] == c2 && s[3] == c3 \
        && s[4] == c4 && s[5] == c5 && s[6] == c6 && s[7] == c7


static void
bitprint(uint8_t *data, int len)
{
	uint8_t *start = data;
	uint8_t x, y, i, z;
	for (y = 0; y < len; y += 1) {
		i = start[y];
		printf("%i-", i);
		for (x = 0; x < 8; x ++) {
			z = 48 + ((i >> x)&1);
			printf("%c", z);
		}
		printf("\n");
	}
}

crb_ws_frame_t *
crb_ws_frame_init()
{
	crb_ws_frame_t *frame;
	
	frame = malloc(sizeof(crb_ws_frame_t));
	if ( frame == NULL ) {
		return NULL;
	}
	
	frame->payload_len = 0;
	frame->opcode = 0;
	frame->crb_type = 0;
	frame->mask.raw = 0;
	
	frame->is_masked = 0;
	
	frame->data = NULL;
	frame->data_length = 0;
	
	return frame;
}

crb_ws_frame_t *
crb_ws_frame_create_from_data(char *data, uint64_t data_length, int masked)
{
	crb_ws_frame_t *frame;
	
	if ( data == NULL || data_length <= 0 ) {
		return NULL;
	}
	
	frame = crb_ws_frame_init();
	
	frame->payload_len = data_length;
	frame->opcode = CRB_WS_TEXT_FRAME;
	frame->crb_type = CRB_WS_TYPE_DATA;
	
	if ( masked ) {
		frame->is_masked = 1;
		frame->mask.raw = rand();
	} else {
		frame->is_masked = 0;
	}
	
	frame->data = data;
}

uint8_t *
crb_ws_frame_head_from_data(uint8_t *data, uint64_t data_length, int *length, int masked)
{
	uint8_t *pos, *frame_data;
	
	uint64_t payload_len;
	uint8_t opcode;
	union {
		uint32_t raw;
		uint8_t octets[4];
	} mask;
	int is_masked;
	
	// define frame properties 
	payload_len = data_length;
	opcode = CRB_WS_TEXT_FRAME;
	
	if ( masked ) {
		is_masked = 1;
		mask.raw = rand();
	} else {
		is_masked = 0;
	}
	
	
	// define data length
	*length = 2;
	
	if ( payload_len < 126 ) {
		// pass
	} else if (payload_len <= 65536 ) {
		*length += 2;
	} else {
		*length += 8;
	}
	
	if ( is_masked ) {
		 *length += 4;
	}
	
	frame_data = malloc(*length * sizeof(uint8_t));
	pos = frame_data;
	
	// Opcode, rsv, fin
	*pos = (opcode&15) | 0b10000000;
	pos += 1;
	
	// Payload length
	if ( payload_len < 126 ) {
		*pos = (payload_len&127) | (is_masked << 7);
	} else if (payload_len <= 65536 ) {
		*pos = 126 | (is_masked << 7);
		pos += 1;
		*(uint16_t*)pos = payload_len;
		pos += 2;
	} else {
		*pos = 127 | (is_masked << 7);
		pos += 1;
		*((uint64_t*)pos) = (uint64_t)payload_len;
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
crb_ws_frame_close(int *length, int masked)
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
	opcode = CRB_WS_CLOSE_FRAME;
	
	if ( masked ) {
		is_masked = 1;
		mask.raw = rand();
	} else {
		is_masked = 0;
	}
	
	// define data length
	*length = 2;
	
	if ( is_masked ) {
		 *length += 4;
	}
	
	data = malloc(*length * sizeof(uint8_t));
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
crb_ws_frame_parse_buffer(crb_ws_frame_t *frame, crb_buffer_t *buffer)
{
	uint16_t raw;
	char *read_pos;
	
	read_pos = buffer->rpos;
	
	if ( buffer->used < 2 ) {
		return CRB_PARSE_INCOMPLETE;
	}
	
	
	raw = * (uint16_t*) read_pos;
	
	// Opcode
	switch( raw & 15 ) {
		case CRB_WS_CONT_FRAME: frame->opcode = CRB_WS_CONT_FRAME; break;
		case CRB_WS_TEXT_FRAME: frame->opcode = CRB_WS_TEXT_FRAME; break;
		case CRB_WS_BIN_FRAME: frame->opcode = CRB_WS_BIN_FRAME; break;
		case CRB_WS_CLOSE_FRAME: frame->opcode = CRB_WS_CLOSE_FRAME; break;
		case CRB_WS_PING_FRAME: frame->opcode = CRB_WS_PING_FRAME; break;
		case CRB_WS_PONG_FRAME: frame->opcode = CRB_WS_PONG_FRAME; break;
		default:
			return CRB_ERROR_INVALID_OPCODE;
	}
	
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
			return CRB_PARSE_INCOMPLETE;
		}
		
		frame->payload_len = ntohs(*(uint16_t *) (read_pos + 2));
		
		if ( frame->payload_len < 4 ) {
			return CRB_PARSE_INCOMPLETE;
		}
		
		read_pos = read_pos + 4; 
	} else if( frame->payload_len == 127 ) {
		// 64bit length
		// require the next 8 bytes
		if ( buffer->used < 10 ) {
			return CRB_PARSE_INCOMPLETE;
		}
		
		frame->payload_len = ntohl(* (uint32_t *) (read_pos + 2));
		frame->payload_len <<= 32;
		frame->payload_len += ntohl(* (uint32_t *) (read_pos + 6));
		
		if ( frame->payload_len < 4 ) {
			return CRB_PARSE_INCOMPLETE;
		}
		
		read_pos = read_pos + 8; 
	} else {
		if ( frame->payload_len < 4 ) {
			return CRB_PARSE_INCOMPLETE;
		}
		read_pos = read_pos + 2; 
	}
	
	// Masking key
	if ( frame->is_masked ) {
		if ( buffer->used < (read_pos + 4) - buffer->ptr ) {
			printf("Incomplete for Mask %i, required %i.\n", buffer->used, (read_pos + 4) - buffer->ptr);
			return CRB_PARSE_INCOMPLETE;
		}
	
		frame->mask.raw = * (uint32_t *) read_pos;
		read_pos = read_pos + 4;
	}
	
	// Payload
	if ( buffer->used < (read_pos + frame->payload_len) - buffer->ptr ) {
		printf("Incomplete for Payload %i, required %i.\n", buffer->used, (read_pos + frame->payload_len) - buffer->ptr);
		return CRB_PARSE_INCOMPLETE;
	}
	
	// Payload / Unmask 
	{
		int i, j;
		uint8_t ch;
		
		for (i = 0; i < frame->payload_len; i += 1) {
			j = i%4;
			ch = (*(u_char*)(read_pos+i))^frame->mask.octets[j];
			*(read_pos+i) = ch;
		}
		
		// detect message type (data or control)
		// remove the first 4 characters for the type id from the plain message
		if ( crb_strcmp3(read_pos, 'D', 'A', 'T') ) {
			frame->crb_type = CRB_WS_TYPE_DATA;
			read_pos += 4;
			frame->payload_len -= 4;
		} else if ( crb_strcmp3(read_pos, 'C', 'T', 'L') ) {
			frame->crb_type = CRB_WS_TYPE_CONTROL;
			read_pos += 4;
			frame->payload_len -= 4;
		} else {
			printf("Unrecognised frame type (dat/ctl).\n");
			return CRB_PARSE_INCOMPLETE;
		}
		
		frame->data = malloc(frame->payload_len + 1);
		frame->data = memcpy(frame->data, read_pos, frame->payload_len);
		frame->data_length = frame->payload_len;
		frame->data[frame->data_length] = '\0';
	}
	
	read_pos = read_pos + frame->payload_len;
	buffer->rpos = read_pos;
	
	return CRB_PARSE_DONE;
}

void 
crb_ws_frame_free(crb_ws_frame_t *frame)
{
	if ( frame == NULL ) {
		return;
	}
	
	free(frame);
}

void
crb_ws_frame_free_with_data(crb_ws_frame_t *frame)
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


