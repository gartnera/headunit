// This file contains payloads to be sent to AA for different keyboard keycodes

#ifndef HU_KEYCODES_H
#define HU_KEYCODES_H

typedef enum
{
    HUIB_UP = 0x13,
    HUIB_DOWN = 0x14,
    HUIB_LEFT = 0x15,
    HUIB_RIGHT = 0x16,
    HUIB_BACK = 0x04,
    HUIB_ENTER = 0x17,
    HUIB_MIC = 0x54,
    HUIB_PLAYPAUSE = 0x55,
    HUIB_NEXT = 0x57,
    HUIB_PREV = 0x58,
    HUIB_PHONE = 0x5,
    HUIB_START = 126,
    HUIB_STOP = 127,

    // Special payloads for these two
    HUIB_ROTATE_LEFT = 256,
    HUIB_ROTATE_RIGHT = 257
}  HU_INPUT_BUTTON;

static size_t varint_encode (uint64_t val, uint8_t *ba, int idx) {
	
	if (val >= 0x7fffffffffffffff) {
		return 1;
	}

	uint64_t left = val;
	int idx2 = 0;
	
	for (idx2 = 0; idx2 < 9; idx2 ++) {
		ba [idx+idx2] = (uint8_t) (0x7f & left);
		left = left >> 7;
		if (left == 0) {
			return (idx2 + 1);
		}
		else if (idx2 < 9 - 1) {
			ba [idx+idx2] |= 0x80;
		}
	}
	
	return 9;
}

/**
 * Generates a pressed button payload with passed in timestamp.
 */
static int hu_fill_button_message(uint8_t* buffer, uint64_t timeStamp, HU_INPUT_BUTTON button, int isPress)
{
    // So the rotation is a bit of a special case and we have to fill up buffer slightly differently.

    int buffCount = 0;
    buffer[buffCount++] = 0x80;
    buffer[buffCount++] = 0x01;
    buffer[buffCount++] = 0x08;
    buffCount += varint_encode(timeStamp, buffer + buffCount, 0);

    if (button == HUIB_ROTATE_LEFT || button == HUIB_ROTATE_RIGHT) {
        buffer[buffCount++] = 0x32;
        buffer[buffCount++] = (button == HUIB_ROTATE_LEFT) ? 0x11 : 0x08;
        buffer[buffCount++] = 0x0A;
        buffer[buffCount++] = (button == HUIB_ROTATE_LEFT) ? 0x0F : 0x06;
        buffer[buffCount++] = 0x08;
        buffer[buffCount++] = 0x80;
        buffer[buffCount++] = 0x80;
        buffer[buffCount++] = 0x04;
        buffer[buffCount++] = 0x10;

        if (button == HUIB_ROTATE_LEFT) {
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0xFF;
            buffer[buffCount++] = 0x01;
        } else {
            buffer[buffCount++] = 0x01;
        }
    } else {
        buffer[buffCount++] = 0x22;
        buffer[buffCount++] = 0x0A;
        buffer[buffCount++] = 0x0A;
        buffer[buffCount++] = 0x08;
        buffer[buffCount++] = 0x08;
        buffer[buffCount++] = (uint8_t)button;
        buffer[buffCount++] = 0x10;
        buffer[buffCount++] = isPress ? 0x01 : 0x00;
        buffer[buffCount++] = 0x18;
        buffer[buffCount++] = 0x00;
        buffer[buffCount++] = 0x20;
        buffer[buffCount++] = 0x00;
    }
    
    return buffCount;
}

#endif