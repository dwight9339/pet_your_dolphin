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
    int32_t petting_elapsed_ms;
    uint32_t petting_anim_start_tick;
    uint32_t idle_anim_start_tick;
    DolphinData* data;
    PetAppState state;
} AppData;

static bool handle_reset(AppData* app) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    bool is_new_day =
        (now.day != app->state.last_pet_day) ||
        (now.month != app->state.last_pet_month) ||
        (now.year != app->state.last_pet_year);

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

    canvas_clear(canvas);

    uint32_t ticks = furi_get_tick();
    
    draw_base(canvas, ticks);

    draw_happy_idle(canvas, ticks - app->idle_anim_start_tick);
}

static void input_callback(InputEvent* input, void* ctx) {
    AppData* app = (AppData*)ctx;
    // Only process press events.
    if(input->type == InputTypePress) {
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

    app->petting_elapsed_ms = -1;
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
                if (app->state.daily_pet_count < DAILY_PET_LIMIT) {
                    dolphin_deed(DolphinDeedPluginGameWin);
                    furi_delay_ms(50);
                    app->state.daily_pet_count++;
                    app->data->stats = dolphin_stats(app->data->dolphin);
                    FURI_LOG_I(TAG, "Dolphin Petted! daily pet count: %lu, butthurt: %lu",
                        app->state.daily_pet_count,
                        app->data->stats.butthurt);
                    pet_app_state_save(&app->state);
                } else {
                    FURI_LOG_I(TAG, "Daily petting limit reached");
                }
                
            } else if(input.key == InputKeyBack && input.type == InputTypePress) {
                FURI_LOG_I(TAG, "Exiting app.");
                app->running = false;
            }
        }
        view_port_update(app->view_port);
    }

    pet_your_dolphin_app_free(app);
    return 0;
}
