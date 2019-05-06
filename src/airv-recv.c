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

int main(int argc, char* argv[])
{
    char const* device_path = "/dev/uinput";
    if (argc > 2)
        errx(1, "USAGE: %s [/dev/uinput]", argv[0]);
    if (argc == 2)
        device_path = argv[1];

    int device = open(device_path, O_WRONLY);
    if (device < 0)
        err(1, "Failed to open %s", device_path);

    struct device_metadata
    {
        struct input_id id;
        char name[80];
        __u32 props[1];
        __u32 bits[0x20+1][0x300/32];
        struct input_absinfo absinfos[0x40];
    };

    struct device_metadata meta = {0};
    if (read(0, &meta, sizeof(meta)) < sizeof(meta))
        err(1, "Failed to read device metadata from stdin");

    fprintf(stderr, "Emulating [%s]\n", meta.name);
    fprintf(
        stderr
        , "  ID    %04x %04x %04x %04x\n"
        , meta.id.bustype
        , meta.id.vendor
        , meta.id.product
        , meta.id.version
    );

    fprintf(stderr, "  PROP ");
    for (int i=0; i<0x20; i++)
        if (test_bit(i, meta.props))
            fprintf(stderr, " %02x", i);
    fputc('\n', stderr);

    for (int i=0; i<0x20; i++) {
        if (!test_bit(i, meta.bits[0x20]))
            continue;

        fprintf(stderr, "  EV%02x ", i);
        for (int k=0; k<0x300; k++)
            if (test_bit(k, meta.bits[i]))
                fprintf(stderr, " %03x", k);
        fputc('\n', stderr);
    }

    struct uinput_setup ui_setup = {0};
    ui_setup.id = meta.id;
    memcpy(ui_setup.name, meta.name, sizeof(ui_setup.name) - 1);
    if (ioctl(device, UI_DEV_SETUP, &ui_setup) < 0)
        err(1, "Failed to setup device");

    for (int i=0; i<0x20; i++)
        if (test_bit(i, meta.props))
            if (ioctl(device, UI_SET_PROPBIT, i) < 0)
                err(1, "Failed to set property %02x", i);

    for (int i=0; i<0x20; i++)
        if (test_bit(i, meta.bits[0x20])) {
            fprintf(stderr, "  Enable %02x bit\n", i);
            if (ioctl(device, UI_SET_EVBIT, i) < 0)
                err(1, "Failed to set evbit %02x", i);

            if (0x00 < i && i < 0x05) {
                for (int k=0; k<0x300; k++)
                    if (test_bit(k, meta.bits[i])) {
                        fprintf(stderr, "  Enable %02x bit %03x\n", i, k);
                        if (ioctl(device, UI_SET_EVBIT + i, k) < 0)
                            err(1, "Failed to set %02x bit %03x", i, k);
                    }
            }
        }

    if (test_bit(EV_ABS, meta.bits[0x20])) {
        for (int i=0; i<0x40; i++) {
            if (!test_bit(i, meta.bits[EV_ABS]))
                continue;

            fprintf(stderr, "  Enable %02x abs axis\n", i);

            struct uinput_abs_setup abs_setup = {0};
            abs_setup.code = i;
            abs_setup.absinfo = meta.absinfos[i];

            if (ioctl(device, UI_ABS_SETUP, &abs_setup) < 0)
                err(1, "Failed to setup abs %02x", i);
        }
    }

    if (ioctl(device, UI_DEV_CREATE) < 0)
        err(1, "Failed to emulate device");

    fprintf(stderr, "  Start emulation.\n");

    struct air_event
    {
        __u16 type;
        __u16 code;
        __s32 value;
    };

    while (1) {
        struct air_event air_ev = {0};
        if (read(0, &air_ev, sizeof(air_ev)) < sizeof(air_ev))
            err(1, "Failed to read input event from stdin");

        struct input_event event = {0};
        event.type = air_ev.type;
        event.code = air_ev.code;
        event.value = air_ev.value;

        fprintf(stderr, "  EVENT %04x %04x %08x\n", event.type, event.code, event.value);
        if (write(device, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to write input event to %s", device_path);
    }
}

