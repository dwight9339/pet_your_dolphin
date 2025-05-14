#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dolphin/dolphin.h"
#include "dolphin/dolphin_i.h"

#include "pet_app_state.h"
#include "draw_animations.h"
#include "compiled/assets_icons.h"

#define TAG "Pet Your Dolphin"
#define DAILY_PET_LIMIT 2
#define MS_IN_HOUR  (60UL*60UL*1000UL)
#define MS_IN_DAY   (24UL*MS_IN_HOUR)
#ifndef TEST_MODE
    #define PETTING_DURATION 6000
#else
    #define PETTING_DURATION 1500
#endif


// This structure holds our dolphin-related data.
typedef struct {
    Dolphin* dolphin;
    DolphinStats stats;
} DolphinData;

// The application state structure.
typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    bool running;
    bool petting_in_progress;
    uint32_t petting_anim_start_tick;
    uint32_t idle_anim_start_tick;
    DolphinData* data;
    PetAppState state;
} AppData;

static bool handle_reset(AppData* app) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    // === Compute effective date for a 5 AM reset ===
    uint8_t eff_day   = now.day;
    uint8_t eff_month = now.month;
    uint16_t eff_year = now.year;
    if(now.hour < 5) {
        // Roll back one calendar day
        if(eff_day > 1) {
            eff_day--;
        } else {
            // Move to last day of previous month
            if(eff_month > 1) {
                eff_month--;
            } else {
                eff_month  = 12;
                eff_year--;
            }
            // Days in each month (no external deps)
            static const uint8_t days_in_month[13] = {
                0,31,28,31,30,31,30,31,31,30,31,30,31
            };
            eff_day = days_in_month[eff_month];
            // Feb leap-year check
            if(eff_month == 2 &&
               ((eff_year % 400 == 0) ||
                (eff_year % 4 == 0 && eff_year % 100 != 0))) {
                eff_day = 29;
            }
        }
    }

    // === Compare to last reset “effective date” ===
    bool is_new_day =
        (eff_day   != app->state.last_pet_day)   ||
        (eff_month != app->state.last_pet_month) ||
        (eff_year  != app->state.last_pet_year);

    if(is_new_day) {
        app->state.last_pet_day = now.day;
        app->state.last_pet_month = now.month;
        app->state.last_pet_year = now.year;
        app->state.daily_pet_count = 0;
        pet_app_state_save(&app->state);

        FURI_LOG_I(TAG, "Daily pet count reset.");
        FURI_LOG_I(TAG, "Date updated to %i-%i-%i.",
            app->state.last_pet_month,
            app->state.last_pet_day,
            app->state.last_pet_year);
        return true;
    }
    return false;
}

static void render_callback(Canvas* canvas, void* ctx) {
    AppData* app = (AppData*)ctx;
    handle_reset(app);

    canvas_set_bitmap_mode(canvas, 1);
    canvas_clear(canvas);

    uint32_t ticks = furi_get_tick();
    
    draw_base(canvas, ticks);

    if (app->petting_in_progress) {
        if (ticks - app->petting_anim_start_tick >= PETTING_DURATION) {
            #ifdef TEST_MODE
                dolphin_deed(DolphinDeedTestLeft);
                FURI_LOG_I(TAG, "[Test Mode] Butthurt incremented by 1");
                FURI_LOG_I(TAG, "[Test Mode] Bypassing pet count update");
            #else
                dolphin_deed(DolphinDeedPluginGameWin);
                app->state.daily_pet_count++;
            #endif
            app->data->stats = dolphin_stats(app->data->dolphin);
            FURI_LOG_I(TAG, "Dolphin Petted! daily pet count: %lu, butthurt: %lu",
                app->state.daily_pet_count,
                app->data->stats.butthurt);
            pet_app_state_save(&app->state);
            app->petting_in_progress = false;
            app->idle_anim_start_tick = ticks;
            draw_happy_idle(canvas, 0);
        } else {
            draw_petting(canvas, ticks - app->petting_anim_start_tick, PETTING_DURATION);
        }
    } else {
        if (app->data->stats.butthurt > 8) {
            draw_sad_idle(canvas, ticks - app->idle_anim_start_tick);
        } else if (app->data->stats.butthurt > 4) {
            draw_neutral_idle(canvas, ticks - app->idle_anim_start_tick);
        } else {
            draw_happy_idle(canvas, ticks - app->idle_anim_start_tick);
        }

        if (app->state.daily_pet_count < DAILY_PET_LIMIT) {
            draw_pet_prompt(canvas, ticks - app->idle_anim_start_tick, false);
        } else {
            draw_pet_prompt(canvas, ticks - app->idle_anim_start_tick, true);
            draw_speech_bubble(canvas);
            if(app->data->stats.butthurt > 8) {
                canvas_draw_str(canvas, 12, 15, "That's");
                canvas_draw_str(canvas, 5, 24, "enough for");
                canvas_draw_str(canvas, 13, 33, "today");
            } else if(app->data->stats.butthurt > 4) {
                canvas_draw_str(canvas, 10, 15, "Thanks");
                canvas_draw_str(canvas, 11, 24, "for the");
                canvas_draw_str(canvas, 16, 33, "pets");
            } else {
                canvas_draw_str(canvas, 12, 20, "You're");
                canvas_draw_str(canvas, 9, 29, "the best!");
            }
        }
    }
}

static void input_callback(InputEvent* input, void* ctx) {
    AppData* app = (AppData*)ctx;
    // Only process press events.
    if(input->type == InputTypePress) {
        FURI_LOG_I(TAG, "Enter button pressed");
        furi_message_queue_put(app->queue, input, FuriWaitForever);
    }
}

AppData* pet_your_dolphin_app_alloc(void) {
    AppData* app = malloc(sizeof(AppData));
    memset(app, 0, sizeof(AppData));

    if(!pet_app_state_load(&app->state)) {  // No previous state
        app->state.last_pet_day = 0;
        app->state.last_pet_month = 0;
        app->state.last_pet_year = 0;
        app->state.daily_pet_count = 0;
    } else {
        FURI_LOG_I(TAG, "Loaded app data. Prev date: %i-%i-%i, Prev pet count: %lu",
            app->state.last_pet_month,
            app->state.last_pet_day,
            app->state.last_pet_year,
            app->state.daily_pet_count);
    }

    app->petting_in_progress = false;
    app->petting_anim_start_tick = 0;
    app->idle_anim_start_tick = 0;

    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->data = malloc(sizeof(DolphinData));
    memset(app->data, 0, sizeof(DolphinData));

    #ifdef TEST_MODE
        dolphin_deed(DolphinDeedTestRight);
        app->state.daily_pet_count = 0;
        FURI_LOG_I(TAG, "[TEST MODE] Butthurt level reset to 0.");
        FURI_LOG_I(TAG, "[TEST MODE] Daily pet count reset to 0.");
    #endif

    app->data->dolphin = furi_record_open(RECORD_DOLPHIN);
    app->data->stats = dolphin_stats(app->data->dolphin);
    FURI_LOG_I(TAG, "Initial dolphin butthurt: %lu", app->data->stats.butthurt);

    app->running = true;

    return app;
}

//
// Free all allocated resources.
//
void pet_your_dolphin_app_free(AppData* app) {
    furi_record_close(RECORD_DOLPHIN);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    free(app->data);
    free(app);
}

//
// Main application function.
//
int32_t pet_your_dolphin(void* p) {
    UNUSED(p);
    AppData* app = pet_your_dolphin_app_alloc();
    InputEvent input;

    // Main event loop: wait for key events and update the view.
    while(app->running) {
        if(furi_message_queue_get(app->queue, &input, 100) == FuriStatusOk) {
            if(input.key == InputKeyOk && input.type == InputTypePress) {
                if (!app->petting_in_progress && app->state.daily_pet_count < DAILY_PET_LIMIT) {
                    app->petting_in_progress = true;
                    app->petting_anim_start_tick = furi_get_tick();
                } else if(app->state.daily_pet_count >= DAILY_PET_LIMIT) {
                    FURI_LOG_I(TAG, "Daily petting limit reached");
                } else {
                    FURI_LOG_I(TAG, "Petting already in progress");
                }
            } else if(input.key == InputKeyBack && input.type == InputTypePress) {
                FURI_LOG_I(TAG, "Exiting app.");
                app->running = false;
            }
            #ifdef TEST_MODE
            else if (input.key == InputKeyUp) {  // Toggle to "pet limit reached" mode
                if (app->state.daily_pet_count == 0) {
                    app->state.daily_pet_count = DAILY_PET_LIMIT;
                } else {
                    app->state.daily_pet_count = 0;
                }
            }
            #endif
        }
        view_port_update(app->view_port);
    }

    pet_your_dolphin_app_free(app);
    return 0;
}
