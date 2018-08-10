// clang-format off
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>
#include <linux/fb.h>
// clang-format on

#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>

#include "lvgl.h"
#include "tslib.h"

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static uint64_t framebuffer_memory_length = 0;
static int framebuffer_descriptor = 0;
static char *framebuffer_memory = nullptr;

static tsdev *touch_device;

void init() {
	int tty_fd = open("/dev/tty0", O_RDONLY, 0);

	if (tty_fd < 0) {
		perror("Error opening tty0");
	}

	if (ioctl(tty_fd, KDSETMODE, KD_TEXT) < 0) {
		perror("Error setting mode");
	}

	if (ioctl(tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
		perror("Error setting mode");
	}

	framebuffer_descriptor = open("/dev/fb0", O_RDWR);
	if (framebuffer_descriptor == -1) {
		perror("Error: cannot open framebuffer device");
		return;
	}

	if (ioctl(framebuffer_descriptor, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("Error reading variable information");
		return;
	}

	vinfo.bits_per_pixel = 16;

	if (ioctl(framebuffer_descriptor, FBIOPUT_VSCREENINFO, &vinfo) == -1) {
		perror("Error writing variable information");
		return;
	}

	if (ioctl(framebuffer_descriptor, FBIOGET_FSCREENINFO, &finfo) == -1) {
		perror("Error reading fixed information");
		return;
	}

	framebuffer_memory_length = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel) / 8;

	framebuffer_memory = (char *)mmap(nullptr, framebuffer_memory_length, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_descriptor, 0);
	if ((int)framebuffer_memory == -1) {
		perror("Error: failed to map framebuffer device to memory");
		return;
	}

	setenv((char*)"TSLIB_FBDEVICE", "/dev/fb0", 1);
  	setenv((char*)"TSLIB_TSDEVICE", "/dev/input/event0", 1);
  	setenv((char*)"TSLIB_CALIBFILE", (char*)"/etc/pointercal", 1);
	setenv((char*)"TSLIB_CONFFILE", (char*)"/etc/ts.conf", 1);

	touch_device = ts_open("/dev/input/event0", 1);

	if (!touch_device) {
		std::cout << "Error opening touch device" << std::endl;
	}

	ts_config(touch_device);
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

    int32_t line_length = finfo.line_length / sizeof(Type);

    for (int32_t y_it = act_y1; y_it <= act_y2; ++y_it) {
	long off_y = y_it * line_length;
	for (int32_t x_it = act_x1; x_it <= act_x2; ++x_it) {
		pixels[(x_it + vinfo.xoffset) + off_y] = color_pointer->full;
		++color_pointer;
	}

	color_pointer += (x2 - act_x2);
    }
}

bool tsinput_read(lv_indev_data_t *data) {
	ts_sample sample;

	ts_read(touch_device, &sample, 1);
	data->point.x = sample.x;
	data->point.y = sample.y;
	data->state = sample.pressure > 10 ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
	return false;
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
    init();

    lv_init();

    lv_disp_drv_t display_driver;
    lv_disp_drv_init(&display_driver);

    display_driver.disp_flush = fbdev_flush;

    lv_disp_drv_register(&display_driver);

    lv_indev_drv_t input_driver;
    lv_indev_drv_init(&input_driver);
    input_driver.type = LV_INDEV_TYPE_POINTER;
    input_driver.read = tsinput_read;

    lv_indev_drv_register(&input_driver);

    lv_theme_t *theme = lv_theme_night_init(20, nullptr);
    lv_theme_set_current(theme);

    lv_obj_t *screen = lv_obj_create(nullptr, nullptr);
    lv_scr_load(screen);

    lv_obj_t *btn = lv_btn_create(screen, nullptr);
    lv_btn_set_fit(btn, true, true);
    lv_btn_set_style(btn, LV_BTN_STYLE_REL, theme->btn.rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, theme->btn.pr);
    lv_btn_set_style(btn, LV_BTN_STYLE_TGL_REL, theme->btn.tgl_rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_TGL_PR, theme->btn.tgl_pr);
    lv_btn_set_style(btn, LV_BTN_STYLE_INA, theme->btn.ina);
    lv_obj_set_pos(btn, 20, 20);
    lv_obj_t *btn_label = lv_label_create(btn, nullptr);
    lv_label_set_text(btn_label, "Button 1");


    while (1) {
        lv_task_handler();
	lv_tick_inc(5);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}
