#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

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
        __u32 evbits;
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
    fprintf(stderr, "  EV    ");
    for (int i=0; i<0x20; i++)
        fputc(((meta.evbits & ((__u32)1 << (0x20 - 1 - i))) ? '+' : '.'), stderr);
    fputc('\n', stderr);
    fprintf(stderr, "  ABS   ");
    for (int i=0; i<0x40; i++)
        fputc((meta.absinfos[i].minimum != meta.absinfos[i].maximum ? '+' : '.'), stderr);
    fputc('\n', stderr);

    struct uinput_setup ui_setup = {0};
    ui_setup.id = meta.id;
    memcpy(ui_setup.name, meta.name, sizeof(ui_setup.name) - 1);
    if (ioctl(device, UI_DEV_SETUP, &ui_setup) < 0)
        err(1, "Failed to setup device");

    for (int i=0; i<0x20; i++)
        if (meta.evbits & ((__u32)1 << (0x20 - 1 - i))) {
            fprintf(stderr, "  Enable %02x bit\n", i);
            if (ioctl(device, UI_SET_EVBIT, i) < 0)
                err(1, "Failed to set evbit %02x", i);
        }

    if (meta.evbits & ((__u32)1 << (0x20 - 1 - EV_ABS))) {
        for (int i=0; i<0x40; i++) {
            if (meta.absinfos[i].minimum == meta.absinfos[i].maximum)
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

    while (1) {
        struct input_event event;
        if (read(0, &event, sizeof(event)) < sizeof(event))
            continue;
            //err(1, "Failed to read input event from stdin");
        fprintf(stderr, "  EVENT %04x %04x %08x\n", event.type, event.code, event.value);
        if (write(device, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to write input event to %s", device_path);
    }
}

