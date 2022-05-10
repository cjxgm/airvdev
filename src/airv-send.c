#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static int test_bit(int bit, __u32 arr[])
{
    return ((arr[bit/32] >> (bit%32)) & 1);
}

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
    char const* path;
    int fd;
    struct device_metadata meta;
};

enum message_kind
{
    message_open = 0,
    message_event = 1,
};

int main(int argc, char* argv[])
{
    if (argc < 1 + 1)
        errx(1, "USAGE: %s /dev/input/event* [/dev/input/event* ...]", argv[0]);

    if (argc > 1 + 256)
        errx(1, "Support at most 256 input devices.");

    struct device devices[256] = {0};

    for (int i=1; i<argc; i++)
        devices[i-1].path = argv[i];

    int ep = epoll_create(1);
    if (ep < 0)
        err(1, "Failed to create epoll instance.");

    for (int i=0; i<256; i++) {
        struct device* dev = &devices[i];
        if (dev->path == NULL) continue;

        dev->fd = open(dev->path, O_RDONLY);
        if (dev->fd < 0)
            err(1, "Failed to open %s", dev->path);

        if (ioctl(dev->fd, EVIOCGID, &dev->meta.id) < 0)
            err(1, "Failed to get device ID of %s", dev->path);
        if (ioctl(dev->fd, EVIOCGNAME(sizeof(dev->meta.name)-1), dev->meta.name) < 0)
            err(1, "Failed to get device name of %s", dev->path);
        if (ioctl(dev->fd, EVIOCGPROP(0x20), dev->meta.props) < 0)
            err(1, "Failed to get device properties of %s", dev->path);
        if (ioctl(dev->fd, EVIOCGBIT(0, sizeof(dev->meta.bits[0])), dev->meta.bits[0x20]) < 0)
            err(1, "Failed to get device event bits of %s", dev->path);
        for (int i=0; i<0x20; i++) {
            if (test_bit(i, dev->meta.bits[0x20]))
                ioctl(dev->fd, EVIOCGBIT(i, 0x300), dev->meta.bits[i]);
        }
        if (test_bit(EV_ABS, dev->meta.bits[0x20])) {
            for (int i=0; i<0x40; i++) {
                if (test_bit(i, dev->meta.bits[EV_ABS]))
                    if (ioctl(dev->fd, EVIOCGABS(i), &dev->meta.absinfos[i]) < 0)
                        err(1, "Failed to get abs info %02x of %s", i, dev->path);
            }
        }

        struct epoll_event epev;
        epev.events = EPOLLIN;
        epev.data.ptr = dev;
        if (epoll_ctl(ep, EPOLL_CTL_ADD, dev->fd, &epev) < 0)
            err(1, "Failed to register interest in reading device %s", dev->path);

        __u8 header = message_open;
        if (write(1, &header, sizeof(header)) < sizeof(header))
            err(1, "Failed to write message header to stdout.");

        __u8 dev_idx = (__u8) i;
        if (write(1, &dev_idx, sizeof(dev_idx)) < sizeof(dev_idx))
            err(1, "Failed to write device index to stdout.");

        if (write(1, &dev->meta, sizeof(dev->meta)) < sizeof(dev->meta))
            err(1, "Failed to write device metadata to stdout.");

        char space = '\t';
        char newline = '\n';
        write(2, dev->path, strlen(dev->path));
        write(2, &space, sizeof(space));
        write(2, dev->meta.name, sizeof(dev->meta.name));
        write(2, &newline, sizeof(newline));
    }

    struct air_event
    {
        __u16 type;
        __u16 code;
        __s32 value;
    };

    while (1) {
        struct epoll_event epev;
        int n = epoll_wait(ep, &epev, 1, -1);
        if (n == 0) continue;
        if (n < 0) err(1, "Failed to wait on epoll instance.");

        struct device* dev = epev.data.ptr;
        struct input_event event = {0};
        if (read(dev->fd, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to read input event from %s", dev->path);

        struct air_event air_ev = {0};
        air_ev.type = event.type;
        air_ev.code = event.code;
        air_ev.value = event.value;

        __u8 header = message_event;
        if (write(1, &header, sizeof(header)) < sizeof(header))
            err(1, "Failed to write message header to stdout.");

        __u8 dev_idx = (__u8) (int) (dev - devices);
        if (write(1, &dev_idx, sizeof(dev_idx)) < sizeof(dev_idx))
            err(1, "Failed to write device index to stdout.");

        if (write(1, &air_ev, sizeof(air_ev)) < sizeof(air_ev))
            err(1, "Failed to write input event to stdout.");
    }
}

