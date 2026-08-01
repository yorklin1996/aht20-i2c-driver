#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_DEV      "/dev/i2c-1"
#define AHT20_ADDR   0x38
#define STATUS_BUSY  0x80
#define STATUS_CAL   0x08

struct aht20_data {
    float temperature;
    float humidity;
};

/* Parse 20-bit packed humidity and temperature from raw bytes */
void parse_raw(unsigned char *buf, struct aht20_data *out)
{
    unsigned int raw_h = (buf[1] << 12) | (buf[2] << 4) | (buf[3] >> 4);
    unsigned int raw_t = ((buf[3] & 0x0F) << 16) | (buf[4] << 8) | buf[5];

    out->humidity = (float)raw_h / 1048576.0 * 100.0;
    out->temperature = (float)raw_t / 1048576.0 * 200.0 - 50.0;
}

/* CRC-8, polynomial 0x31, init 0xFF (per AHT20 datasheet) */
unsigned char crc8(unsigned char *data, int len)
{
    unsigned char crc = 0xFF;
    int i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];

        for (j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = crc << 1;
        }
    }

    return crc;
}

/* Trigger a measurement and read 7 bytes from the sensor */
int aht20_read(unsigned char *buf)
{
    unsigned char cmd[3] = {0xAC, 0x33, 0x00};
    int fd;

    fd = open(I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, AHT20_ADDR) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    /* Send measurement command */
    if (write(fd, cmd, 3) != 3) {
        perror("write");
        close(fd);
        return -1;
    }

    /* Datasheet requires 80ms for measurement to complete */
    usleep(80000);

    if (read(fd, buf, 7) != 7) {
        perror("read");
        close(fd);
        return -1;
    }

    /* Status byte: bit7 = busy, bit3 = calibrated */
    if (buf[0] & STATUS_BUSY) {
        fprintf(stderr, "sensor busy\n");
        close(fd);
        return -1;
    }

    if (!(buf[0] & STATUS_CAL)) {
        fprintf(stderr, "sensor not calibrated\n");
        close(fd);
        return -1;
    }

    /* Verify data integrity: buf[6] is the CRC of buf[0..5] */
    if (crc8(buf, 6) != buf[6]) {
        fprintf(stderr, "CRC mismatch\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int main(void)
{
    unsigned char buf[7];
    struct aht20_data *d;

    d = malloc(sizeof(struct aht20_data));
    if (!d)
        return -1;

    if (aht20_read(buf) < 0) {
        free(d);
        return -1;
    }

    parse_raw(buf, d);

    printf("Temperature: %.2f C\n", d->temperature);
    printf("Humidity:    %.2f %%\n", d->humidity);

    free(d);
    return 0;
}