#define BOOT_DATA_MAX 4096

char bootData[BOOT_DATA_MAX] __attribute__ ((section("k42_boot_data")));
