#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>

#include "lvgl.h"

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static uint64_t framebuffer_memory_length = 0;
static int framebuffer_descriptor = 0;
static char *framebuffer_memory = nullptr;

void fbdev_init() {
	framebuffer_descriptor = open("/dev/fb0", O_RDWR);
	if (framebuffer_descriptor == -1) {
		perror("Error: cannot open framebuffer device");
		return;
	}

	if (ioctl(framebuffer_descriptor, FBIOGET_FSCREENINFO, &finfo) == -1) {
		perror("Error reading fixed information");
		return;
	}

	if (ioctl(framebuffer_descriptor, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("Error reading variable information");
		return;
	}

	framebuffer_memory_length = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	framebuffer_memory = (char *)mmap(nullptr, framebuffer_memory_length, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_descriptor, 0);
	if ((int)framebuffer_memory == -1) {
		perror("Error: failed to map framebuffer device to memory");
		return;
	}
}

template <typename Type>
void do_copy(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
             const lv_color_t *color_p) {
    int act_x1 = std::clamp<int>(x1, 0, vinfo.xres - 1);
    int act_y1 = std::clamp<int>(y1, 0, vinfo.yres - 1);
    int act_x2 = std::clamp<int>(x2, 0, vinfo.xres - 1);
    int act_y2 = std::clamp<int>(y2, 0, vinfo.yres - 1);

    const lv_color_t *color_pointer = color_p;
    Type *pixels = (Type *) framebuffer_memory;

    for (int32_t y_it = act_y1; y_it <= act_y2; ++y_it) {
	long off_y = (y_it + vinfo.yoffset) * vinfo.xres;
	for (int32_t x_it = act_x1; x_it <= act_x2; ++x_it) {
		pixels[(x_it + vinfo.xoffset) + off_y] = color_pointer->full;
		++color_pointer;
	}

	color_pointer += (x2 - act_x2);
    }
}

void fbdev_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
		const lv_color_t *color_p) {
	if(framebuffer_memory == nullptr ||
			x2 < 0 ||
			y2 < 0 ||
			x1 > vinfo.xres - 1 ||
			y1 > vinfo.yres - 1)
	{
		lv_flush_ready();
		return;
	}

	switch (vinfo.bits_per_pixel) {
		case 32:
		case 24:
			do_copy<uint32_t>(x1, y1, x2, y2, color_p);
			break;
		case 16:
			do_copy<uint16_t>(x1, y1, x2, y2, color_p);
			break;
		case 8:
			do_copy<uint8_t>(x1, y1, x2, y2, color_p);
			break;
		default:
			break;
	}

	lv_flush_ready();
}

int main() {
    fbdev_init();

    lv_init();

    lv_disp_drv_t display_driver;
    lv_disp_drv_init(&display_driver);

    display_driver.disp_flush = fbdev_flush;

    lv_disp_drv_register(&display_driver);

    lv_obj_t *label = lv_btn_create(lv_scr_act(), nullptr);
    // lv_label_set_text(label, "Hello World!");
    lv_obj_align(label, nullptr, LV_ALIGN_CENTER, 0, 0);

    while (1) {
        lv_task_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
	std::cout << "Loop" << std::endl;
    }
    return 0;
}
