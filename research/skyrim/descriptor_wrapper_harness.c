/* Host-only: invoke the exact preserved pre-grant descriptor wrapper. */
#include "../../build/return-wrapper/source/native/pw_title_publication_native.h"
#include <stdio.h>
#include <stdint.h>
int main(void) {
    uint8_t bytes[PW_TITLE_LAUNCH_MAX_BYTES + 1];
    size_t count = fread(bytes, 1, sizeof(bytes), stdin);
    if (ferror(stdin)) { puts("input-error"); return 2; }
    if (count > PW_TITLE_LAUNCH_MAX_BYTES) { puts("oversize"); return 1; }
    PwTitlePublicationDescriptor descriptor;
    char error[192] = {0};
    int result = pw_title_publication_descriptor_parse(bytes, count, "FAKE78005", &descriptor, error, sizeof(error));
    puts(result == 0 ? "accepted" : "rejected");
    return result == 0 ? 0 : 1;
}
