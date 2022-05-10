#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static int test_bit(int bit, __u32 arr[])
{
    return ((arr[bit/32] >> (bit%32)) & 1);
}

//#define AIRV_FIX_ASPECT_RATIO 1                 // 0: don't fix; 1: do fix.
//#define AIRV_FIX_ASPECT_RATIO_ABS_CODE 1        // which axis to fix.
//#define AIRV_FIX_ASPECT_RATIO_ABS_SCALE 4/3     // scale the axis by the SCALE.

struct device_metadata
{
    struct input_id id;
    char name[80];
    __u32 props[1];
    __u32 bits[0x20+1][0x300/32];
    struct input_absinfo absinfos[0x40];
};

struct device
{
    int fd;
    struct device_metadata meta;
};

enum message_kind
{
    message_open = 0,
    message_event = 1,
};

static void on_message_open(char const* uinput_path, struct device* dev);
static void on_message_event(struct device const* dev);

int main(int argc, char* argv[])
{
    char const* uinput_path = "/dev/uinput";
    if (argc > 2)
        errx(1, "USAGE: %s [/dev/uinput]", argv[0]);
    if (argc == 2)
        uinput_path = argv[1];

    struct device devices[256] = {0};

    while (1) {
        __u8 header = 0;
        __u8 dev_idx = 0;

        if (read(0, &header, sizeof(header)) < sizeof(header))
            err(1, "Failed to read message header from stdin");

        if (read(0, &dev_idx, sizeof(dev_idx)) < sizeof(dev_idx))
            err(1, "Failed to read device index from stdin");

        enum message_kind kind = header;
        int i = dev_idx;
        struct device* dev = &devices[i];

        switch (kind) {
            case message_open: on_message_open(uinput_path, dev); break;
            case message_event: on_message_event(dev); break;
            default: errx(1, "Unknown message header: 0x%02x", header);
        }
    }
}

static void on_message_open(char const* uinput_path, struct device* dev)
{
    if (read(0, &dev->meta, sizeof(dev->meta)) < sizeof(dev->meta))
        err(1, "Failed to read device metadata from stdin");

    fprintf(stderr, "Emulating [%s]\n", dev->meta.name);
    fprintf(
        stderr
        , "  ID    %04x %04x %04x %04x\n"
        , dev->meta.id.bustype
        , dev->meta.id.vendor
        , dev->meta.id.product
        , dev->meta.id.version
    );

    fprintf(stderr, "  PROP ");
    for (int i=0; i<0x20; i++)
        if (test_bit(i, dev->meta.props))
            fprintf(stderr, " %02x", i);
    fputc('\n', stderr);

    for (int i=0; i<0x20; i++) {
        if (!test_bit(i, dev->meta.bits[0x20]))
            continue;

        fprintf(stderr, "  EV%02x ", i);
        for (int k=0; k<0x300; k++)
            if (test_bit(k, dev->meta.bits[i]))
                fprintf(stderr, " %03x", k);
        fputc('\n', stderr);
    }

    if (dev->fd > 0) {
        close(dev->fd);
        dev->fd = 0;
    }

    dev->fd = open(uinput_path, O_WRONLY);
    if (dev->fd < 0)
        err(1, "Failed to open %s", uinput_path);

    struct uinput_setup ui_setup = {0};
    ui_setup.id = dev->meta.id;
    memcpy(ui_setup.name, dev->meta.name, sizeof(ui_setup.name) - 1);
    if (ioctl(dev->fd, UI_DEV_SETUP, &ui_setup) < 0)
        err(1, "Failed to setup device");

    for (int i=0; i<0x20; i++)
        if (test_bit(i, dev->meta.props))
            if (ioctl(dev->fd, UI_SET_PROPBIT, i) < 0)
                err(1, "Failed to set property %02x", i);

    for (int i=0; i<0x20; i++)
        if (test_bit(i, dev->meta.bits[0x20])) {
            fprintf(stderr, "  Enable %02x bit\n", i);
            if (ioctl(dev->fd, UI_SET_EVBIT, i) < 0)
                err(1, "Failed to set evbit %02x", i);

            if (0x00 < i && i < 0x05) {
                for (int k=0; k<0x300; k++)
                    if (test_bit(k, dev->meta.bits[i])) {
                        fprintf(stderr, "  Enable %02x bit %03x\n", i, k);
                        if (ioctl(dev->fd, UI_SET_EVBIT + i, k) < 0)
                            err(1, "Failed to set %02x bit %03x", i, k);
                    }
            }
        }

    #if AIRV_FIX_ASPECT_RATIO
    int abs_fix_max = 1;
    #endif

    if (test_bit(EV_ABS, dev->meta.bits[0x20])) {
        for (int i=0; i<0x40; i++) {
            if (!test_bit(i, dev->meta.bits[EV_ABS]))
                continue;

            fprintf(stderr, "  Enable %02x abs axis  %d -> %d [%d]\n", i
                    , dev->meta.absinfos[i].minimum, dev->meta.absinfos[i].maximum, dev->meta.absinfos[i].resolution);

            struct uinput_abs_setup abs_setup = {0};
            abs_setup.code = i;
            abs_setup.absinfo = dev->meta.absinfos[i];
            if (abs_setup.absinfo.resolution == 0)
                abs_setup.absinfo.resolution = 300;

            if (ioctl(dev->fd, UI_ABS_SETUP, &abs_setup) < 0)
                err(1, "Failed to setup abs %02x", i);

            #if AIRV_FIX_ASPECT_RATIO
            if (i == AIRV_FIX_ASPECT_RATIO_ABS_CODE)
                abs_fix_max = dev->meta.absinfos[i].maximum;
            #endif
        }
    }

    if (ioctl(dev->fd, UI_DEV_CREATE) < 0)
        err(1, "Failed to emulate device");

    fprintf(stderr, "  Start emulation.\n");

}

static void on_message_event(struct device const* dev)
{
    struct air_event
    {
        __u16 type;
        __u16 code;
        __s32 value;
    };

    struct air_event air_ev = {0};
    if (read(0, &air_ev, sizeof(air_ev)) < sizeof(air_ev))
        err(1, "Failed to read input event from stdin");

    struct input_event event = {0};
    event.type = air_ev.type;
    event.code = air_ev.code;
    event.value = air_ev.value;

    #if AIRV_FIX_ASPECT_RATIO
    if (event.type == EV_ABS && event.code == AIRV_FIX_ASPECT_RATIO_ABS_CODE) {
        event.value = event.value * AIRV_FIX_ASPECT_RATIO_ABS_SCALE;
        if (event.value > abs_fix_max)
            event.value = abs_fix_max;
    }
    #endif

    fprintf(stderr, "  EVENT %04x %04x %08x\n", event.type, event.code, event.value);
    if (write(dev->fd, &event, sizeof(event)) < sizeof(event))
        err(1, "Failed to write input event to %s", dev->meta.name);
}

