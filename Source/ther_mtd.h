
#ifndef __THER_MTD_H__
#define __THER_MTD_H__


struct mtd_info {
	uint32  size;
	uint32  erase_size;
	uint32  write_size;

	bool  (*open)(void);
	bool  (*close)(void);
	bool  (*erase)(uint32 offset, uint32 len);
	uint32 (*read)(uint32 offset, void *buf, uint32 len);
	uint32 (*write)(uint32 offset, const void *buf, uint32 len);
};

void ther_mtd_init(void);

#endif

