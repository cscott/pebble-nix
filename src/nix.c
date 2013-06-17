#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x32, 0xD5, 0xDF, 0x25, 0x13, 0x21, 0x47, 0x80, 0x92, 0x84, 0xD4, 0xAA, 0x39, 0xDC, 0x4F, 0x5F }
PBL_APP_INFO(MY_UUID,
             "Nix", "cscott.net",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window window;

BmpContainer image_container;

Layer timeLayer; // The filled-in dots

#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 14
#define BLOCK_SPACING_X (BLOCK_SIZE_X + 3)
#define BLOCK_SPACING_Y (BLOCK_SIZE_Y + 3)
#define BLOCK_MARGIN_LEFT 45

#define HOURS_TENS_TOP 4
#define HOURS_ONES_TOP 24
#define MINUTES_TENS_TOP 79
#define MINUTES_ONES_TOP 116

unsigned short get_display_hour(unsigned short hour) {

  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;

}

unsigned short get_random_bit(void) {
    static uint16_t lfsr = 0xACE1u;
    /* 16-bit galois LFSR, period 65535. */
    /* see http://en.wikipedia.org/wiki/Linear_feedback_shift_register */
    /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    return lfsr & 1;
}

void draw_nix_for_digit(GContext *ctx, unsigned short digit,
                        unsigned short rows, unsigned short top) {
    // make an array of top corners
    GPoint cells[rows*3];
    unsigned short i, j, k;
    unsigned short left;
    for (i=j=0; i<rows; i++) {
        left = BLOCK_MARGIN_LEFT;
        cells[j++] = GPoint(left, top);
        left += BLOCK_SPACING_X;
        cells[j++] = GPoint(left, top);
        left += BLOCK_SPACING_X;
        cells[j++] = GPoint(left, top);
        top += BLOCK_SPACING_Y;
    }
    // shuffle cells.
    // this is a bubble sort (gasp) with comparisons replaced with random bits
    for (i=rows*3; i!=0; ) {
        k = 0;
        for (j=1; j<i; j++) {
            // "compare" cells[j-1] and cells[j]
            if (get_random_bit()) {
                GPoint tmp = cells[j-1];
                cells[j-1] = cells[j];
                cells[j] = tmp;
                k = j;
            }
        }
        i = k;
    }
    // draw the first 'digit' cells
    for (i=0; i<digit; i++) {
        GRect rect = GRect(cells[i].x, cells[i].y, BLOCK_SIZE_X, BLOCK_SIZE_Y);
        graphics_fill_rect(ctx, rect, 0, GCornerNone);
    }
}

void timeLayer_update_callback(Layer *me, GContext *ctx) {
    (void)me;

    PblTm t;

    get_time(&t);

    unsigned short display_hour = get_display_hour(t.tm_hour);
    graphics_context_set_fill_color(ctx, GColorWhite);
    draw_nix_for_digit(ctx, display_hour/10, 1, HOURS_TENS_TOP);
    draw_nix_for_digit(ctx, display_hour%10, 3, HOURS_ONES_TOP);
    draw_nix_for_digit(ctx, t.tm_min / 10, 2, MINUTES_TENS_TOP);
    draw_nix_for_digit(ctx, t.tm_min % 10, 3, MINUTES_ONES_TOP);
}

void handle_init(AppContextRef ctx) {
    (void)ctx;

    window_init(&window, "Nix");
    window_stack_push(&window, true /* Animated */);

    resource_init_current_app(&NIX_IMAGE_RESOURCES);

    // Note: This needs to be "de-inited" in the application's
    //       deinit handler.
    bmp_init_container(RESOURCE_ID_IMAGE_NIX_BACKGROUND, &image_container);

    layer_add_child(&window.layer, &image_container.layer.layer);

    // Init the layer that shows the time.
    // Associate with layer object and set dimensions.
    layer_init(&timeLayer, window.layer.frame);
    // Set the drawing callback function for the layer
    timeLayer.update_proc = &timeLayer_update_callback;
    // Add the child to the app's base window.
    layer_add_child(&window.layer, &timeLayer);
}

// Called once per second
static void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)ctx;
  (void)t;

  // Causes a redraw of the layer (via the associated layer update callback)
  // Note: This will cause the entire layer to be cleared first so it needs
  //       to be redrawn in its entirety
  layer_mark_dirty(&timeLayer);
  // XXX recompute time array?
}

void handle_deinit(AppContextRef ctx) {
    (void)ctx;

    // Note: Failure to de-init this here will result in instability and
    //       unable to allocate memory errors.
    bmp_deinit_container(&image_container);
}

void pbl_main(void *params) {
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .deinit_handler = &handle_deinit,
        .tick_info = {
            .tick_handler = &handle_second_tick,
            .tick_units = SECOND_UNIT
        }
    };
    app_event_loop(params, &handlers);
}
