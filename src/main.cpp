// clang-format off
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stropts.h>
#include <errno.h>
#include <stdio.h>
#include <linux/fb.h>
#include <sys/mman.h>
// clang-format on

#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>

#include "SDL/SDL.h"
#include "lvgl.h"

static fb_var_screeninfo vinfo;
static fb_var_screeninfo original_vinfo;
static fb_fix_screeninfo finfo;
static unsigned char *framebuffer_memory;
static int framebuffer_descriptor;
static uint64_t framebuffer_size;

void fbdev_init() {
    framebuffer_descriptor = open("/dev/fb0", O_RDWR);

    if (!framebuffer_descriptor) {
        return;
    }

    if (ioctl(framebuffer_descriptor, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Error getting information about the framebuffer");
    }

    memcpy(&original_vinfo, &vinfo, sizeof(fb_var_screeninfo));

    if (ioctl(framebuffer_descriptor, FBIOGET_VSCREENINFO, &finfo) < 0) {
        perror("Error getting information about the framebuffer");
    }

    vinfo.bits_per_pixel = 24;
    vinfo.xres = 320;
    vinfo.yres = 480;
    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres;

    if (ioctl(framebuffer_descriptor, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
        perror("Error setting new framebuffer dimensions");
    }

    framebuffer_size = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);

    framebuffer_memory =
        (unsigned char *)mmap(0, framebuffer_size, PROT_READ | PROT_WRITE,
                              MAP_SHARED, framebuffer_descriptor, 0);

    if ((int64_t)framebuffer_memory < 0) {
        perror("Error mmapping framebuffer memory");
    }
}

template <typename Type>
void do_copy(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
             const lv_color_t *color_p) {
    int act_x1 = std::clamp<int>(x1, 0, vinfo.xres - 1);
    int act_y1 = std::clamp<int>(y1, 0, vinfo.yres - 1);
    int act_x2 = std::clamp<int>(x2, 0, vinfo.xres - 1);
    int act_y2 = std::clamp<int>(y2, 0, vinfo.yres - 1);

    Type *color_pointer = (Type *)color_p;
    Type *framebuffer_memory_type = (Type *)framebuffer_memory;

    int32_t x_off = (act_x1 + vinfo.xoffset);
    int32_t x_off_color = (act_x1 - x1);
    int32_t cpy_length = act_x2 - act_x1;

    for (int32_t y_it = act_y1; y_it < act_y2; ++y_it) {
        int32_t y_off = (y_it + vinfo.yoffset) * vinfo.xres;
        int32_t y_off_color = (y_it - y1) * (x2 - x1);

        std::memcpy(framebuffer_memory_type + y_off + x_off,
                    color_pointer + y_off_color + x_off_color, cpy_length);
    }
}

void fbdev_map(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
               const lv_color_t *color) {
    if (!framebuffer_memory || x2 < 0 || y2 < 0 ||
        x1 > (int32_t)vinfo.xres - 1 || y2 > (int32_t)vinfo.yres - 1) {
        return;
    }

    switch (vinfo.bits_per_pixel) {
        case 32:
        case 24:
            do_copy<int32_t>(x1, y1, x2, y2, color);
            break;
        case 16:
            do_copy<int16_t>(x1, y1, x2, y2, color);
            break;
        case 8:
            do_copy<int8_t>(x1, y1, x2, y2, color);
            break;
        default:
            break;
    }
}

void fbdev_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                 const lv_color_t *color) {
    fbdev_map(x1, y1, x2, y2, color);
    lv_flush_ready();
}

int main() {
    fbdev_init();

    lv_init();

    lv_disp_drv_t display_driver;
    lv_disp_drv_init(&display_driver);

    display_driver.disp_flush = fbdev_flush;

    lv_disp_drv_register(&display_driver);

    lv_obj_t *label = lv_label_create(lv_scr_act(), nullptr);
    lv_label_set_text(label, "Hello World!");
    lv_obj_align(label, nullptr, LV_ALIGN_CENTER, 0, 0);

    while (1) {
        lv_task_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    munmap(framebuffer_memory, framebuffer_size);
    close(framebuffer_descriptor);
    return 0;
}
