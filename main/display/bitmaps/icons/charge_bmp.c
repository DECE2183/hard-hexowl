//	charge bitmap
//
//	Memory usage
//		Bitmap: 32
//		Descriptor: 6
//
//		Total: 38
//

typedef struct {
	const unsigned char *map;
	const unsigned char w;
	const unsigned char h;
} __bitmap_t;

const unsigned char __charge_map[] = {
	0x00,0x00,0x00,0x0E,0x0E,0x00,0x0E,0x0E,0x00,0xDE,0xEE,0xE0,0xEE,0xEE,0xE0,0x9E,0xEE,0x90,0x09,0xEB,0x00,0x04,0xE4,0x00
};

const __bitmap_t _charge_icon = {
    .map = __charge_map,
    .w = 6,
    .h = 8,
};

const void *charge_icon = &_charge_icon;
