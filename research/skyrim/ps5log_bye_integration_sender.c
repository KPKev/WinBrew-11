/* Host-only adapter around the preserved logger, using its real init and close. */
#define _DEFAULT_SOURCE 1
#define PS5LOG_IMPLEMENTATION
#include "../../build/return-wrapper/source/native/ps5log/ps5log.h"
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    unsigned long port = strtoul(argv[1], NULL, 10);
    if (!port || port > 65535) return 2;
    ps5log_config config;
    ps5log_config_defaults(&config);
    config.enabled = 1;
    config.port = (uint16_t)port;
    snprintf(config.server, sizeof(config.server), "127.0.0.1");
    if (ps5log_init(&config, "FAKE78005", "prospero-win-wine64", 0x1234) != 0) return 3;
    ps5log_close(argv[2]);
    return 0;
}
