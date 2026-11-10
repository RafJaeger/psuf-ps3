#ifndef FPSU_COVER_H
#define FPSU_COVER_H

#include "app.h"

typedef struct {
    void *pixels;
    unsigned int pitch;
    unsigned int width;
    unsigned int height;
} fpsu_cover;

/* Initialize fpsu_cover to {0} before first use. All calls run on the UI thread.
 * release frees decoded pixels only; it never removes the persistent disk cache.
 */
void cover_release(fpsu_cover *cover);
void cover_set_progress_callback(int (*callback)(void *), void *user);
/* All loaders return 0 on success, -1 if unavailable/invalid. Cache keys are
 * canonical Title IDs (four uppercase letters and five digits), never list indices.
 * Valid downloads persist at FPSU_CACHE_ROOT/covers/<TITLE_ID>.jpg across launches.
 */
/* Prepare missing covers only; validate/reuse existing cache or local art first.
 * A progress callback returning nonzero cancels pending network work.
 */
int cover_cache_for_game(const fpsu_game *game);
/* Library rendering: disk cache first, local fallback, no network access.
 * Call on every fresh app/library entry regardless of prior download actions.
 */
int cover_load_cached_or_local_for_game(const fpsu_game *game, fpsu_cover *cover);
int cover_load_banner_for_game(const fpsu_game *game, fpsu_cover *cover);
/* Both reuse valid persistent cache; only missing/invalid covers use the network. */
int cover_load_remote_for_game(const fpsu_game *game, fpsu_cover *cover);
/* Cache, local art, then network. */
int cover_load_for_game(const fpsu_game *game, fpsu_cover *cover);

/* Poll from the UI thread. Returns 1 when the selection has been resolved.
 * NULL discards pending art and returns 1 when it is safe to decode other images.
 */
int cover_poll_preview(const fpsu_game *game, fpsu_cover *cover, fpsu_cover *banner);

#endif
