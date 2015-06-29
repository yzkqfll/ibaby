
#ifndef __THER_MTD_H__
#define __THER_MTD_H__


enum {
	MTD_OK = 0,

	/* logic error */
	MTD_ERR_ADDR_NOT_ALIGN,

	/* erase error */
	MTD_ERR_ERASE_TIMEOUT,
	MTD_ERR_ERASE_SIZE,

	/* program error */

	/* read error */

	/* Manufacturers ID error */
	MTD_ERR_MF_ID,

	/* memory type and capacity error */
	MTD_ERR_MEM_TYPE_CAP,
};


struct mtd_info {
	uint32 size;

	uint32 write_size;
	uint32 erase_size;
	uint32 read_size;

	int8  (*open)(void);
	int8  (*close)(void);
	int8  (*erase)(uint32 addr, uint32 len);
	int8 (*read)(uint32 addr, void *buf, uint32 len);
	int8 (*write)(uint32 addr, const void *buf, uint32 len);
};

void ther_mtd_init(void);
struct mtd_info* get_mtd(void);
int8 ther_mtd_open(struct mtd_info* m);
int8 ther_mtd_close(struct mtd_info* m);
int8 ther_mtd_erase(struct mtd_info* m, uint32 addr, uint32 len, uint32 *ret_len);
int8 ther_mtd_read(struct mtd_info* m, uint32 addr, void *buf, uint32 len, uint32 *ret_len);
int8 ther_mtd_write(struct mtd_info* m, uint32 addr, void *buf, uint32 len, uint32 *ret_len);

#endif

