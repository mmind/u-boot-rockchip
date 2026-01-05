// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2026 Heiko Stuebner <heiko@sntech.de>
 */

#include <dm.h>
#include <env.h>
#include <i2c_eeprom.h>
#include <init.h>
#include <net.h>
#include <netdev.h>
#include <vsprintf.h>

#ifndef CONFIG_XPL_BUILD

DECLARE_GLOBAL_DATA_PTR;

#define DTB_DIR			"rockchip/"

struct tsx33_model {
	const char *mb;
	const u8 mb_pcb;
	const char *bp;
	const u8 bp_pcb;
	const char *name;
	const char *fdtfile;
};

/*
 * gd->board_type is unsigned long, so start at 1 for actual found types
 * The numeric PCB ids should be matched against the highest available
 * one for best feature matching. For example Q0AI0_13 gained per-disk
 * gpios for presence detection and power-control. Similar changes
 * for the other boards.
 * So the list should be sorted higest to lowest pcb-id.
 */
enum tsx33_device_id {
	UNKNOWN_TSX33 = 0,
	Q0AI0_13,
	Q0AI0_11,
	Q0AJ0_Q0AM0_12_11,
	Q0AJ0_Q0AM0_11_10,
	Q0B20_Q0AW0_12_10,
	Q0B20_Q0B30_12_10,
	Q0B20_Q0B30_10_10,
	QA0110_10,
	TSX33_MODELS,
};

/*
 * All TSx33 devices consist of a mainboard and possible backplane.
 * Each board has a model identifier and a pcb code written to an eeprom
 * on it. Later board revisions got per-harddrive presence detection
 * and power-supply switches, so might need an overlay applied later on
 * to support those.
 */
static const struct tsx33_model tsx33_models[TSX33_MODELS] = {
	[UNKNOWN_TSX33] = {
		"UNKNOWN",
		0,
		NULL,
		0,
		"Unknown TSx33",
		NULL,
	},
	[Q0AI0_13] = {
		"Q0AI0",
		13,
		NULL,
		0,
		"TS133",
		NULL, /* not yet supported */
	},
	[Q0AI0_11] = {
		"Q0AI0",
		11,
		NULL,
		0,
		"TS133",
		NULL, /* not yet supported */
	},
	[Q0AJ0_Q0AM0_12_11] = {
		"Q0AJ0",
		12,
		"Q0AM0",
		11,
		"TS233",
		DTB_DIR "rk3568-qnap-ts233.dtb",
	},
	[Q0AJ0_Q0AM0_11_10] = {
		"Q0AJ0",
		11,
		"Q0AM0",
		10,
		"TS233",
		DTB_DIR "rk3568-qnap-ts233.dtb",
	},
	[Q0B20_Q0AW0_12_10] = {
		"Q0B20",
		12,
		"Q0AW0",
		10,
		"TS216G",
		NULL, /* not yet supported */
	},
	[Q0B20_Q0B30_12_10] = {
		"Q0B20",
		12,
		"Q0B30",
		10,
		"TS433",
		DTB_DIR "rk3568-qnap-ts433.dtb",
	},
	[Q0B20_Q0B30_10_10] = {
		"Q0B20",
		10,
		"Q0B30",
		10,
		"TS433",
		DTB_DIR "rk3568-qnap-ts433.dtb",
	},
	[QA0110_10] = {
		/*
		 * The board ident in the original firmware is longer here.
		 * Likely the field order is different in the ident string.
		 * So this needs an EEPROM dump to figure out.
		 */
		"QA0110",
		11,
		NULL,
		0,
		"TS433eU",
		NULL, /* not yet supported */
	},
};

enum tsx33_product_id_format {
	LEN_22 = 0,
	LEN_21,
	LEN_12, /* has the least amound of dashes in the string, so last */
	TSX33_PRODUCT_FORMATS,
};

/*
 * We (only) identify the serial format via the different position of dashes.
 * Because we read the product string from an eeprom there is no
 */
struct tsx33_product_format {
	int *dashes;
	int num_dashes;
	int (*decoder)(char *product, char *board, int *pcb);
};

/*
 * The longest board-name length known.
 * Buffers using this need to be MAX-LEN + 1 for the final 0-termination.
 */
#define PRODUCT_MAX_BOARD_LEN		6

#define LEN22_PRODUCT_BOARD_OFF		6
#define LEN22_PRODUCT_BOARD_OFF2	12
#define LEN22_PRODUCT_BOARD_LEN		6
#define LEN22_PRODUCT_PCB_OFF		16
#define LEN22_PRODUCT_PCB_LEN		2
#define LEN22_PRODUCT_BOM_OFF		14
#define LEN22_PRODUCT_BOM_LEN		1

static int tsx33_decode_len22(char *product, char *board, int *pcb)
{
	strncpy(board, product + LEN22_PRODUCT_BOARD_OFF,
		LEN22_PRODUCT_BOARD_LEN - 1);
	board[LEN22_PRODUCT_BOARD_LEN - 1] = product[LEN22_PRODUCT_BOARD_OFF2];

	/* add an artificial end to the PCB part for strtol */
	product[LEN22_PRODUCT_PCB_OFF + LEN22_PRODUCT_PCB_LEN] = '\0';
	*pcb = (int)simple_strtol(product + LEN22_PRODUCT_PCB_OFF, NULL, 0);

	return 0;
}

#define LEN21_PRODUCT_BOARD_OFF		6
#define LEN21_PRODUCT_BOARD_OFF2	11
#define LEN21_PRODUCT_BOARD_LEN		5
#define LEN21_PRODUCT_PCB_OFF		15
#define LEN21_PRODUCT_PCB_LEN		2
#define LEN21_PRODUCT_BOM_OFF		13
#define LEN21_PRODUCT_BOM_LEN		1

static int tsx33_decode_len21(char *product, char *board, int *pcb)
{
	strncpy(board, product + LEN21_PRODUCT_BOARD_OFF,
		LEN21_PRODUCT_BOARD_LEN - 1);
	board[LEN21_PRODUCT_BOARD_LEN - 1] = product[LEN21_PRODUCT_BOARD_OFF2];

	/* add an artificial end to the PCB part for strtol */
	product[LEN21_PRODUCT_PCB_OFF + LEN21_PRODUCT_PCB_LEN] = '\0';
	*pcb = (int)simple_strtol(product + LEN21_PRODUCT_PCB_OFF, NULL, 0);

	return 0;
}

#define LEN12_PRODUCT_BOARD_OFF	4
#define LEN12_PRODUCT_BOARD_LEN	5
#define LEN12_PRODUCT_PCB_OFF	9
#define LEN12_PRODUCT_PCB_LEN	2
#define LEN12_PRODUCT_BOM_OFF	11
#define LEN12_PRODUCT_BOM_LEN	1

static int tsx33_decode_len12(char *product, char *board, int *pcb)
{
	strncpy(board, product + LEN12_PRODUCT_BOARD_OFF,
		LEN12_PRODUCT_BOARD_LEN);

	/* add an artificial end to the PCB part for strtol */
	product[LEN12_PRODUCT_PCB_OFF + LEN12_PRODUCT_PCB_LEN] = '\0';
	*pcb = (int)simple_strtol(product + LEN12_PRODUCT_PCB_OFF, NULL, 0);

	return 0;
}

const struct tsx33_product_format product_formats[TSX33_PRODUCT_FORMATS] = {
	[LEN_22] = {
		(int[]){ 2, 11, 15 },
		3,
		tsx33_decode_len22,
	},
	[LEN_21] = {
		(int[]){ 2, 10, 14 },
		3,
		tsx33_decode_len21,
	},
	[LEN_12] = {
		(int[]){ 2 },
		1,
		tsx33_decode_len12,
	},
};

static int tsx33_decode_product(char *product, char *board, int *pcb)
{
	const struct tsx33_product_format *format;
	int i, j;

	/* Find the correct serial variant */
	for (i = 0; i < TSX33_PRODUCT_FORMATS; i++) {
		format = &product_formats[i];

		for (j = 0; j < format->num_dashes; j++) {
			int dash_char = format->dashes[j];

			if (product[dash_char] != '-')
				goto next_format;
		}

		/* all dashes in expected position */
		break;
next_format:
		format = NULL;
	}

	if (!format)
		return -EINVAL;

	return format->decoder(product, board, pcb);
}

static int tsx33_setup_device(int type, const struct tsx33_model *model)
{
	if (!model->bp)
		printf("Type:  QNAP %s (%s_%d)\n", model->name,
		       model->mb, model->mb_pcb);
	else
		printf("Type:  QNAP %s (%s_%s_%d_%d)\n", model->name, model->mb,
		       model->bp, model->mb_pcb, model->bp_pcb);

	gd->board_type = type;

	if (!model->fdtfile) {
		printf("No board-specific devicetree found, functionality might be degraded.\n");
		return 0;
	}

	return env_set("fdtfile", model->fdtfile);
}

/* EEPROM field offsets and length */
#define MB_PRODUCT_OFF	0x42
#define MB_PRODUCT_LEN	32
#define BP_PRODUCT_OFF	0x6a
#define BP_PRODUCT_LEN	32

static int tsx33_detect_device(void)
{
	struct udevice *dev;
	char product[MB_PRODUCT_LEN];
	char mb_board[PRODUCT_MAX_BOARD_LEN + 1] = { 0 };
	char bp_board[PRODUCT_MAX_BOARD_LEN + 1] = { 0 };
	int mb_pcb = 0, bp_pcb = 0;
	int ret, i;

	/*
	 * Init to 0, for no specific board found.
	 * This allows us to run on similar devices for initial bringup.
	 */
	gd->board_type = 0;

	/* Get mainboard eeprom, this will always be present */
	ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@54", &dev);
	if (ret)
		return ret;

	ret = i2c_eeprom_read(dev, MB_PRODUCT_OFF, product, MB_PRODUCT_LEN);
	if (ret)
		return ret;

	ret = tsx33_decode_product(product, mb_board, &mb_pcb);
	if (ret < 0) {
		printf("Type:  Unknown product-id format '%s'\n", product);
		return ret;
	}

	/* Try to get backplane eeprom, only available if a backplane exists */
	ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@56", &dev);
	if (ret)
		goto search_model;

	ret = i2c_eeprom_read(dev, BP_PRODUCT_OFF, product, BP_PRODUCT_LEN);
	if (ret)
		goto search_model;

	ret = tsx33_decode_product(product, bp_board, &bp_pcb);
	if (ret < 0) {
		printf("Type:  Unknown product-id format '%s'\n", product);
		return ret;
	}

search_model:
	for (i = 0; i < TSX33_MODELS; i++) {
		const struct tsx33_model *model = &tsx33_models[i];

		/*
		 * Check if mainboard does not match the entry.
		 * Condition to meet: Either board-name differs or model
		 * pcb-rev is bigger than the one read from the eeprom.
		 * Both the list and the decoded string are 0-terminated.
		 */
		if (strcmp(model->mb, mb_board) || model->mb_pcb > mb_pcb)
			continue;

		/*
		 * Mainboard matches and there is no backplane found
		 * nor expected.
		 */
		if (!bp_pcb && !model->bp)
			return tsx33_setup_device(i, model);

		/* we expect a backplane, but the entry does not have one */
		if (!model->bp)
			continue;

		/*
		 * Check if backplane matches the entry.
		 * Both the list and the decoded string are 0-terminated.
		 */
		if (!strcmp(model->bp, bp_board) && model->bp_pcb <= bp_pcb)
			return tsx33_setup_device(i, model);
	}

	printf("Type:  No matching type found for %s_%s_%d_%d\n", mb_board,
	       bp_board, mb_pcb, bp_pcb);

	return -ENODEV;
}

int rk_board_late_init(void)
{
	int ret;

	/* If detection fails, we'll still try to continue */
	ret = tsx33_detect_device();
	if (ret)
		printf("Unable to detect device type: %d\n", ret);

	return 0;
}
#endif
