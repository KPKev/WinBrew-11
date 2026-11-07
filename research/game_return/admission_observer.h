/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef WINE11_ADMISSION_OBSERVER_H
#define WINE11_ADMISSION_OBSERVER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum Wine11AdmissionStatus {
    WINE11_ADMISSION_NO_ACCESS = 0,
    WINE11_ADMISSION_DESCRIPTOR_REFUSED,
    WINE11_ADMISSION_WRONG_GAME,
    WINE11_ADMISSION_BUSY,
    WINE11_ADMISSION_PUBLICATION_REFUSED,
    WINE11_ADMISSION_OWNER_REFUSED,
    WINE11_ADMISSION_OWNER_MISMATCH,
    WINE11_ADMISSION_KIT_BINDING_REFUSED,
    WINE11_ADMISSION_READ_ONLY_PASSED
} Wine11AdmissionStatus;
enum {
    WINE11_CHECK_DESCRIPTOR = 1u,
    WINE11_CHECK_PUBLICATION = 2u,
    WINE11_CHECK_OWNERS = 4u,
    WINE11_CHECK_COMMITTED_KIT_BINDING = 8u
};
typedef struct Wine11AdmissionReport {
    Wine11AdmissionStatus status;
    uint32_t passed;
    /* Observation is never permission to install or repair anything. */
    int safe_to_install;
    int full_kit_artifacts_verified;
    int native_stopped_state_verified;
} Wine11AdmissionReport;
/* For a future user-invoked in-process Wine11 diagnostic only. Caller must
 * already possess its normal authorized filesystem grant. No elevation,
 * process launch, permission change, recovery or state-writing operation occurs.
 * Output carries fixed status/flags only, never private IDs, bytes or errors. */
Wine11AdmissionReport wine11_admission_observe(int filesystem_granted);
const char *wine11_admission_summary(Wine11AdmissionStatus);
#ifdef WINE11_ADMISSION_TEST
#include "pw_title_publication_native.h"
Wine11AdmissionReport wine11_admission_observe_test(
    int filesystem_granted, const PwTitlePublicationPlatform *platform);
#endif
#ifdef __cplusplus
}
#endif
#endif
