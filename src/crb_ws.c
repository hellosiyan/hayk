#include <stdlib.h>
#include <stdint.h>

#include "crb_ws.h"

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
	frame->mask.raw = 0;
	
	frame->is_masked = 0;
	
	frame->data = NULL;
	frame->data_length = 0;
	
	return frame;
}

void 
crb_ws_frame_free(crb_ws_frame_t *frame)
{
	free(frame);
}
