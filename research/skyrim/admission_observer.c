/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "admission_observer.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
/* Pinned amalgamation adapter: include the unchanged exact reader implementation
 * so owner-marker observations reuse its private, inode-stable read_regular.
 * The build verifies its SHA-256 before compiling this translation unit. */
#ifndef WINE11_PUBLICATION_NATIVE_SOURCE
#define WINE11_PUBLICATION_NATIVE_SOURCE "native/pw_title_publication_native.inc"
#endif
#include WINE11_PUBLICATION_NATIVE_SOURCE

/* Fixed scope: never accept a caller-supplied title or app ID. */
#define OBSERVE_TITLE "FAKE78005"
#define OBSERVE_APP 489830u

enum {
    OWNER_SCHEMA, OWNER_KIND, OWNER_INSTALL, OWNER_TITLE, OWNER_KIT, OWNER_FILES,
    OWNER_BUNDLE, OWNER_REVISION, OWNER_RUNTIME_PATH, OWNER_VARIANT,
    OWNER_TEMPLATE, OWNER_RECIPE, OWNER_RECIPE_VERSION, OWNER_FIELDS
};
typedef struct OwnerObservation { char *values[OWNER_FIELDS]; } OwnerObservation;
static int owner_parse(uint8_t *bytes, size_t length, const char *kind,
                       const PwTitleLaunch *launch, OwnerObservation *out)
{
    static const char *const keys[OWNER_FIELDS] = {
        "schema", "kind", "install_id", "title_id", "kit_id", "files_sha256",
        "runtime_bundle_id", "runtime_revision", "runtime_path", "variant_id",
        "template_path", "recipe_id", "recipe_version"
    };
    unsigned seen = 0;
    OwnerObservation candidate = {{0}};
    if (!bytes || !length || length > 4096 || memchr(bytes, 0, length)) return -1;
    for (size_t begin = 0; begin < length;) {
        size_t end = begin, equal = begin;
        while (end < length && bytes[end] != '\n') ++end;
        while (equal < end && bytes[equal] != '=') ++equal;
        if (equal == begin || equal + 1 >= end) return -1;
        for (size_t n = begin; n < end; ++n)
            if (bytes[n] < 32 || bytes[n] > 126) return -1;
        bytes[equal] = 0; bytes[end] = 0;
        unsigned field = 0;
        while (field < OWNER_FIELDS && strcmp((char *)bytes + begin, keys[field])) ++field;
        if (field == OWNER_FIELDS || (seen & (1u << field))) return -1;
        seen |= 1u << field; candidate.values[field] = (char *)bytes + equal + 1;
        begin = end + 1;
    }
    if (seen != (1u << OWNER_FIELDS) - 1u || strcmp(candidate.values[OWNER_SCHEMA], "1") ||
        strcmp(candidate.values[OWNER_KIND], kind) ||
        strcmp(candidate.values[OWNER_INSTALL], launch->install_id) ||
        strcmp(candidate.values[OWNER_TITLE], launch->title_id) ||
        strcmp(candidate.values[OWNER_BUNDLE], launch->runtime_bundle_id) ||
        strcmp(candidate.values[OWNER_RECIPE], launch->recipe_id) ||
        strcmp(candidate.values[OWNER_RECIPE_VERSION], launch->recipe_version)) return -1;
    if (strlen(candidate.values[OWNER_FILES]) != 64) return -1;
    for (const char *p = candidate.values[OWNER_FILES]; *p; ++p)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return -1;
    *out = candidate;
    return 0;
}
static int owners_coherent(const OwnerObservation *title, const OwnerObservation *bottle)
{
    /* Exact same-kit subset of Playden RespinIdentity, without renamed-kit
     * aliases. File inventory must agree. Allowed respin-only fields may differ. */
    for (unsigned field = 0; field < OWNER_FIELDS; ++field) {
        if (field == OWNER_KIND || field == OWNER_REVISION ||
            field == OWNER_RUNTIME_PATH || field == OWNER_VARIANT) continue;
        if (strcmp(title->values[field], bottle->values[field])) return 0;
    }
    return 1;
}
static Wine11AdmissionReport observe_with(int granted, const PwTitlePublicationPlatform *platform)
{
    Wine11AdmissionReport report = {WINE11_ADMISSION_NO_ACCESS, 0, 0, 0, 0};
    PwTitlePublicationDescriptor descriptor;
    PwTitlePublicationLease lease;
    OwnerObservation title, bottle;
    PwTitlePublication receipt;
    uint8_t *title_bytes = NULL, *bottle_bytes = NULL, *receipt_bytes = NULL;
    size_t title_length = 0, bottle_length = 0, receipt_length = 0;
    char bottle_path[1024], receipt_path[1024];
    if (!granted || !platform) return report;
    pw_title_publication_lease_init(&lease);
    report.status = WINE11_ADMISSION_DESCRIPTOR_REFUSED;
    if (pw_title_publication_read_descriptor("/data/homebrew/" OBSERVE_TITLE "/game-launch.json",
            OBSERVE_TITLE, platform, &descriptor, NULL, 0) != PW_OK) goto done;
    report.passed |= WINE11_CHECK_DESCRIPTOR;
    report.status = WINE11_ADMISSION_WRONG_GAME;
    if (descriptor.launch.app_id != OBSERVE_APP) goto done;
    int opened = pw_title_publication_open(&descriptor, platform, &lease, NULL, 0);
    if (opened != PW_OK) {
        report.status = opened == PW_TITLE_PUBLICATION_ADMISSION_BUSY ?
            WINE11_ADMISSION_BUSY : WINE11_ADMISSION_PUBLICATION_REFUSED;
        goto done;
    }
    report.passed |= WINE11_CHECK_PUBLICATION;
    report.status = WINE11_ADMISSION_OWNER_REFUSED;
    if (append_path(bottle_path, sizeof(bottle_path), descriptor.launch.data_root,
            "library/prefixes/game/.playden-template-owner") ||
        append_path(receipt_path, sizeof(receipt_path), descriptor.launch.data_root,
            ".playden-control/publication-state.v1")) goto done;
    title_bytes = read_regular(platform, "/data/homebrew/" OBSERVE_TITLE "/.playden-title-owner",
                               4096, READ_PRIVATE, &title_length);
    bottle_bytes = read_regular(platform, bottle_path, 4096, READ_PRIVATE, &bottle_length);
    if (owner_parse(title_bytes, title_length, "title", &descriptor.launch, &title) ||
        owner_parse(bottle_bytes, bottle_length, "template", &descriptor.launch, &bottle)) goto done;
    report.status = WINE11_ADMISSION_OWNER_MISMATCH;
    if (!owners_coherent(&title, &bottle)) goto done;
    report.passed |= WINE11_CHECK_OWNERS;
    report.status = WINE11_ADMISSION_KIT_BINDING_REFUSED;
    receipt_bytes = read_regular(platform, receipt_path, PW_TITLE_PUBLICATION_MAX_BYTES,
                                 READ_PRIVATE, &receipt_length);
    if (!receipt_bytes || pw_title_publication_parse(receipt_bytes, receipt_length, &receipt) != PW_OK ||
        receipt.phase != PW_TITLE_PUBLICATION_COMMITTED || !receipt_matches(&receipt, &descriptor.launch) ||
        strcmp(receipt.kit_id, title.values[OWNER_KIT])) goto done;
    report.passed |= WINE11_CHECK_COMMITTED_KIT_BINDING;
    report.status = WINE11_ADMISSION_READ_ONLY_PASSED;
done:
    free(title_bytes); free(bottle_bytes); free(receipt_bytes);
    pw_title_publication_close(&lease);
    return report;
}
#ifdef __PROSPERO__
#include "pw_title_publication_ps5_lock.h"
Wine11AdmissionReport wine11_admission_observe(int granted)
{
    PwTitlePublicationPlatform platform = {NULL, (uint32_t)geteuid(), pw_title_publication_ps5_lock, NULL};
    return observe_with(granted, &platform);
}
#else
Wine11AdmissionReport wine11_admission_observe(int granted)
{
    /* No host fallback can observe real PS5 state. */
    (void)granted;
    Wine11AdmissionReport report = {WINE11_ADMISSION_NO_ACCESS, 0, 0, 0, 0};
    return report;
}
#endif
#ifdef WINE11_ADMISSION_TEST
Wine11AdmissionReport wine11_admission_observe_test(int granted, const PwTitlePublicationPlatform *platform)
{
    return observe_with(granted, platform);
}
#endif
const char *wine11_admission_summary(Wine11AdmissionStatus status)
{
    switch (status) {
    case WINE11_ADMISSION_NO_ACCESS: return "Authorized filesystem context unavailable";
    case WINE11_ADMISSION_DESCRIPTOR_REFUSED: return "Descriptor observation refused";
    case WINE11_ADMISSION_WRONG_GAME: return "Game identity differs";
    case WINE11_ADMISSION_BUSY: return "Publication is busy";
    case WINE11_ADMISSION_PUBLICATION_REFUSED: return "Native publication admission refused";
    case WINE11_ADMISSION_OWNER_REFUSED: return "Owner observation refused";
    case WINE11_ADMISSION_OWNER_MISMATCH: return "Owner identities differ";
    case WINE11_ADMISSION_KIT_BINDING_REFUSED: return "Committed kit binding differs";
    case WINE11_ADMISSION_READ_ONLY_PASSED: return "Owner and publication observations passed";
    }
    return "Observation refused";
}
