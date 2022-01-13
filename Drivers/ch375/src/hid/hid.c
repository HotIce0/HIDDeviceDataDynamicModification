#include "hid/hid.h"
#include "bswap.h"

uint8_t *hid_fetch_item(uint8_t *start, uint8_t *end, HIDItem *item)
{
	uint8_t fb;

	if ((end - start) <= 0) {
		return NULL;
	}

	fb = *start++;
	item->type = (fb >> 2) & 0x03;
	item->tag = (fb >> 4) & 0x0F;

	if (item->tag == HID_ITEM_TAG_LONG) {
		if ((end - start) < 2)
			return NULL;

		item->size = *start++;
		item->tag  = *start++;

		if ((end - start) < item->size)
			return NULL;

		item->data.longdata = start;
		start += item->size;
		return start;
	}

	item->format = HID_ITEM_FORMAT_SHORT;
	item->size = fb & 0x03;

    switch (item->size) {
	case 0:
		return start;

	case 1:
		if ((end - start) < 1)
			return NULL;
		item->data.u8 = *start++;
		return start;

	case 2:
		if ((end - start) < 2)
			return NULL;
		item->data.u16 = le16_to_cpu(*(uint16_t *)start);
		start = (uint8_t *)((uint16_t *)start + 1);
		return start;

	case 3:
		item->size++;
		if ((end - start) < 4)
			return NULL;
		item->data.u32 = le32_to_cpu(*(uint32_t *)start);
		start = (uint8_t *)((uint32_t *)start + 1);
		return start;
	}

	return NULL;
}