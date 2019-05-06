#include <linux/input.h>
#include <sys/ioctl.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    if (argc != 2)
        errx(1, "USAGE: %s /dev/input/event*", argv[0]);

    char const* device_path = argv[1];

    int device = open(device_path, O_RDONLY);
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
    if (ioctl(device, EVIOCGID, &meta.id) < 0)
        err(1, "Failed to get device ID of %s", device_path);
    if (ioctl(device, EVIOCGNAME(sizeof(meta.name)-1), meta.name) < 0)
        err(1, "Failed to get device name of %s", device_path);
    for (int i=0; i<0x20; i++) {
        char bit = 0;
        ioctl(device, EVIOCGBIT(i, sizeof(bit)), &bit); // when it errors out, it means no such bit.
        meta.evbits <<= 1;
        meta.evbits |= bit;
    }
    for (int i=0; i<0x40; i++) {
        ioctl(device, EVIOCGABS(i), &meta.absinfos[i]); // Just ignore the errors.
    }

    if (write(1, &meta, sizeof(meta)) < sizeof(meta))
        err(1, "Failed to write device metadata to stdout");

    struct air_event
    {
        __u16 type;
        __u16 code;
        __s32 value;
    };

    while (1) {
        struct input_event event = {0};
        if (read(device, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to read input event from %s", device_path);

        struct air_event air_ev = {0};
        air_ev.type = event.type;
        air_ev.code = event.code;
        air_ev.value = event.value;

        if (write(1, &air_ev, sizeof(air_ev)) < sizeof(air_ev))
            err(1, "Failed to write input event to stdout");
    }
}

