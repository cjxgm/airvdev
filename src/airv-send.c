#include <linux/input.h>
#include <linux/uinput.h>
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

    struct uinput_setup ui_setup = {0};
    if (ioctl(device, EVIOCGID, &ui_setup.id) < 0)
        err(1, "Failed to get device ID of %s", device_path);
    if (ioctl(device, EVIOCGNAME(sizeof(ui_setup.name)-1), ui_setup.name) < 0)
        err(1, "Failed to get device name of %s", device_path);
    if (write(1, &ui_setup, sizeof(ui_setup)) < sizeof(ui_setup))
        err(1, "Failed to write uinput setup to stdout");

    __u32 evbits = 0;
    for (int i=0; i<0x20; i++) {
        char bit = 0;
        ioctl(device, EVIOCGBIT(i, sizeof(bit)), &bit); // when it errors out, it means no such bit.
        evbits <<= 1;
        evbits |= bit;
    }
    if (write(1, &evbits, sizeof(evbits)) < sizeof(evbits))
        err(1, "Failed to write device event bits to stdout");

    __u64 absbits = 0;
    struct input_absinfo absinfos[0x40] = {0};
    for (int i=0; i<0x40; i++) {
        absbits <<= 1;
        absbits |= (ioctl(device, EVIOCGABS(i), &absinfos[i]) >= 0);
    }
    if (write(1, &absbits, sizeof(absbits)) < sizeof(absbits))
        err(1, "Failed to write device abs bits to stdout");
    if (write(1, absinfos, sizeof(absinfos)) < sizeof(absinfos))
        err(1, "Failed to write device abs infos to stdout");

    while (1) {
        struct input_event event;
        if (read(device, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to read input event from %s", device_path);
        if (write(1, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to write input event to stdout");
    }
}

