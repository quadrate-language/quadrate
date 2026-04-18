// SDL2 shim layer for Quadrate.
//
// Each function here bridges from Quadrate's stack-based FFI convention
// (all functions have signature `int f(qd_context*)` and pop/push via the
// runtime's qd_pop_* / qd_push_* helpers) to the plain C calling convention
// that SDL exposes. Only the subset we need for the Doom port is surfaced;
// more can be added on demand.

#include <quadrate/rt/ffi.h>
#include <SDL2/SDL.h>

/* --- Init / shutdown --- */

int Init(qd_context* ctx) {
	int64_t flags;
	if (qd_pop_i(ctx, &flags) != QD_OK) return 1;
	int rc = SDL_Init((Uint32)flags);
	qd_push_i(ctx, rc);
	return 0;
}

int Quit(qd_context* ctx) {
	(void)ctx;
	SDL_Quit();
	return 0;
}

int GetError(qd_context* ctx) {
	const char* e = SDL_GetError();
	qd_push_s(ctx, e ? e : "");
	return 0;
}

/* --- Window / renderer / texture --- */

int CreateWindow(qd_context* ctx) {
	int64_t flags, w, h, y, x;
	char title[256];
	if (qd_pop_i(ctx, &flags) != QD_OK) return 1;
	if (qd_pop_i(ctx, &h) != QD_OK) return 1;
	if (qd_pop_i(ctx, &w) != QD_OK) return 1;
	if (qd_pop_i(ctx, &y) != QD_OK) return 1;
	if (qd_pop_i(ctx, &x) != QD_OK) return 1;
	if (qd_pop_s(ctx, title, sizeof(title)) != QD_OK) return 1;
	SDL_Window* win = SDL_CreateWindow(title, (int)x, (int)y, (int)w, (int)h, (Uint32)flags);
	qd_push_p(ctx, win);
	return 0;
}

int DestroyWindow(qd_context* ctx) {
	void* win;
	if (qd_pop_p(ctx, &win) != QD_OK) return 1;
	SDL_DestroyWindow((SDL_Window*)win);
	return 0;
}

int CreateRenderer(qd_context* ctx) {
	int64_t flags, index;
	void* win;
	if (qd_pop_i(ctx, &flags) != QD_OK) return 1;
	if (qd_pop_i(ctx, &index) != QD_OK) return 1;
	if (qd_pop_p(ctx, &win) != QD_OK) return 1;
	SDL_Renderer* r = SDL_CreateRenderer((SDL_Window*)win, (int)index, (Uint32)flags);
	qd_push_p(ctx, r);
	return 0;
}

int DestroyRenderer(qd_context* ctx) {
	void* r;
	if (qd_pop_p(ctx, &r) != QD_OK) return 1;
	SDL_DestroyRenderer((SDL_Renderer*)r);
	return 0;
}

int CreateTexture(qd_context* ctx) {
	int64_t h, w, access, format;
	void* renderer;
	if (qd_pop_i(ctx, &h) != QD_OK) return 1;
	if (qd_pop_i(ctx, &w) != QD_OK) return 1;
	if (qd_pop_i(ctx, &access) != QD_OK) return 1;
	if (qd_pop_i(ctx, &format) != QD_OK) return 1;
	if (qd_pop_p(ctx, &renderer) != QD_OK) return 1;
	SDL_Texture* t = SDL_CreateTexture((SDL_Renderer*)renderer,
			(Uint32)format, (int)access, (int)w, (int)h);
	qd_push_p(ctx, t);
	return 0;
}

int DestroyTexture(qd_context* ctx) {
	void* t;
	if (qd_pop_p(ctx, &t) != QD_OK) return 1;
	SDL_DestroyTexture((SDL_Texture*)t);
	return 0;
}

/* --- Frame update --- */

/* UpdateTexture with a full-surface NULL rect. The framebuffer is passed as
 * a raw pointer plus pitch in bytes. */
int UpdateTextureFull(qd_context* ctx) {
	int64_t pitch;
	void *pixels, *tex;
	if (qd_pop_i(ctx, &pitch) != QD_OK) return 1;
	if (qd_pop_p(ctx, &pixels) != QD_OK) return 1;
	if (qd_pop_p(ctx, &tex) != QD_OK) return 1;
	int rc = SDL_UpdateTexture((SDL_Texture*)tex, NULL, pixels, (int)pitch);
	qd_push_i(ctx, rc);
	return 0;
}

int RenderClear(qd_context* ctx) {
	void* r;
	if (qd_pop_p(ctx, &r) != QD_OK) return 1;
	SDL_RenderClear((SDL_Renderer*)r);
	return 0;
}

int RenderCopyFull(qd_context* ctx) {
	void *tex, *r;
	if (qd_pop_p(ctx, &tex) != QD_OK) return 1;
	if (qd_pop_p(ctx, &r) != QD_OK) return 1;
	SDL_RenderCopy((SDL_Renderer*)r, (SDL_Texture*)tex, NULL, NULL);
	return 0;
}

int RenderPresent(qd_context* ctx) {
	void* r;
	if (qd_pop_p(ctx, &r) != QD_OK) return 1;
	SDL_RenderPresent((SDL_Renderer*)r);
	return 0;
}

int SetRenderDrawColor(qd_context* ctx) {
	int64_t a, b, g, r;
	void* renderer;
	if (qd_pop_i(ctx, &a) != QD_OK) return 1;
	if (qd_pop_i(ctx, &b) != QD_OK) return 1;
	if (qd_pop_i(ctx, &g) != QD_OK) return 1;
	if (qd_pop_i(ctx, &r) != QD_OK) return 1;
	if (qd_pop_p(ctx, &renderer) != QD_OK) return 1;
	SDL_SetRenderDrawColor((SDL_Renderer*)renderer,
			(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
	return 0;
}

/* --- Timing --- */

int GetTicks(qd_context* ctx) {
	Uint32 t = SDL_GetTicks();
	qd_push_i(ctx, (int64_t)t);
	return 0;
}

int Delay(qd_context* ctx) {
	int64_t ms;
	if (qd_pop_i(ctx, &ms) != QD_OK) return 1;
	SDL_Delay((Uint32)ms);
	return 0;
}

/* --- Events ---
 *
 * Instead of exposing SDL_Event as a tagged union (which quadrate can't
 * represent yet), we expose a flat poll function that populates separate
 * out-slots for each field the Doom port actually reads: event type, and
 * for KEYDOWN/KEYUP the SDL scancode. Returns 1 if an event was pulled,
 * 0 otherwise. */
/* --- Audio ---
 *
 * Minimal PCM output: open a device at 11025 Hz / 8-bit unsigned / mono
 * (matches Doom's DS* lump sample format), then queue raw sample bytes
 * to play. SDL_QueueAudio returns 0 on success. Queueing is additive —
 * subsequent calls append onto the tail, so short overlapping SFX mix
 * only if we pre-mix manually. For the doom port that's acceptable
 * (pistol shots are brief).
 */
static SDL_AudioDeviceID g_audio_dev = 0;

int AudioInit(qd_context* ctx) {
	SDL_AudioSpec want;
	SDL_AudioSpec have;
	SDL_zero(want);
	want.freq = 11025;
	want.format = AUDIO_U8;
	want.channels = 1;
	want.samples = 1024;
	want.callback = NULL;
	g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (g_audio_dev == 0) {
		qd_push_i(ctx, 0);
		return 0;
	}
	SDL_PauseAudioDevice(g_audio_dev, 0);
	qd_push_i(ctx, 1);
	return 0;
}

int AudioQueue(qd_context* ctx) {
	int64_t len;
	void* ptr;
	if (qd_pop_i(ctx, &len) != QD_OK) return 1;
	if (qd_pop_p(ctx, &ptr) != QD_OK) return 1;
	if (g_audio_dev == 0 || ptr == NULL || len <= 0) {
		qd_push_i(ctx, 0);
		return 0;
	}
	int rc = SDL_QueueAudio(g_audio_dev, ptr, (Uint32)len);
	qd_push_i(ctx, rc == 0 ? 1 : 0);
	return 0;
}

int AudioClearQueue(qd_context* ctx) {
	(void)ctx;
	if (g_audio_dev != 0) SDL_ClearQueuedAudio(g_audio_dev);
	return 0;
}

int PollKey(qd_context* ctx) {
	SDL_Event ev;
	if (!SDL_PollEvent(&ev)) {
		qd_push_i(ctx, 0);   /* has_event */
		qd_push_i(ctx, 0);   /* type */
		qd_push_i(ctx, 0);   /* scancode */
		return 0;
	}
	qd_push_i(ctx, 1);
	qd_push_i(ctx, (int64_t)ev.type);
	int64_t code = 0;
	if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
		code = (int64_t)ev.key.keysym.scancode;
	}
	qd_push_i(ctx, code);
	return 0;
}
