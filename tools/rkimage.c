/*
 * (C) Copyright 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * See README.rockchip for details of the rkimage format
 */

#include "imagetool.h"
#include "mkimage.h"
#include <image.h>
#include <linux/kconfig.h>

static uint32_t header;

static int rkimage_check_params(struct image_tool_params *params)
{
	return 0;
}

static int rkimage_verify_header(unsigned char *buf, int size,
				 struct image_tool_params *params)
{
	return 0;
}

static void rkimage_print_header(const void *buf)
{
}

enum {
	RK_FOOTER		= 0xdeadbeaf,
};

static void rkimage_set_header(void *buf, struct stat *sbuf, int ifd,
			       struct image_tool_params *params)
{
	uint32_t footer = RK_FOOTER;
	int f_offset;

	memcpy(buf, CONFIG_ROCKCHIP_RKIMAGE_HEADER, 4);

	f_offset = (params->orig_file_size + 3) / 4 * 4;
printf("setader: offset %d, osize %d, fsize %d\n", f_offset, params->orig_file_size, params->file_size);
	memcpy(buf + f_offset, &footer, 4);
}

static int rkimage_extract_subimage(void *buf, struct image_tool_params *params)
{
	return 0;
}

static int rkimage_check_image_type(uint8_t type)
{
	if (type == IH_TYPE_RKIMAGE)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/* Pad file to the next full 256byte - this method returns that size */
static int rkimage_vrec_header(struct image_tool_params *params,
			    struct image_type_params *tparams)
{
	int pad_size;

	params->orig_file_size = params->file_size;

	pad_size = (params->orig_file_size + 7) / 4 * 4;
//	pad_size = (params->file_size + 0x7ff) / 0x800 * 0x800;
printf("vrec_header: osize %d, fsize %d\n", params->orig_file_size, params->file_size);

	return pad_size - params->file_size;
}

/*
 * rk_image parameters
 */
U_BOOT_IMAGE_TYPE(
	rkimage,
	"Rockchip Boot Image support",
	4,
	&header,
	rkimage_check_params,
	rkimage_verify_header,
	rkimage_print_header,
	rkimage_set_header,
	rkimage_extract_subimage,
	rkimage_check_image_type,
	NULL,
	rkimage_vrec_header
);
