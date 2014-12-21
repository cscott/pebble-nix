#include <pebble.h>

static Window *s_window;

static GBitmap *s_background_image;
static BitmapLayer *s_background_image_layer;

static Layer *s_time_layer; // The filled-in dots

#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 14
#define BLOCK_SPACING_X (BLOCK_SIZE_X + 3)
#define BLOCK_SPACING_Y (BLOCK_SIZE_Y + 3)
#define BLOCK_MARGIN_LEFT 45

#define HOURS_TENS_TOP 4
#define HOURS_ONES_TOP 24
#define MINUTES_TENS_TOP 79
#define MINUTES_ONES_TOP 116

static unsigned short get_display_hour(unsigned short hour) {

    if (clock_is_24h_style()) {
        return hour;
    }

    unsigned short display_hour = hour % 12;

    // Converts "0" to "12"
    return display_hour ? display_hour : 12;

}

/* generate a uniform random number in the range [0,1] */
static unsigned short get_random_bit(void) {
    static uint16_t lfsr = 0xACE1u;
    /* 16-bit galois LFSR, period 65535. */
    /* see http://en.wikipedia.org/wiki/Linear_feedback_shift_register */
    /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    unsigned short out = lfsr & 1u;
    lfsr = (lfsr >> 1) ^ (-(out) & 0xB400u);
    return out;
}

/* generate a uniform random number in the range [0, 2^n) */
static unsigned short get_random_bits(unsigned short n) {
    unsigned short out = 0;
    while (n--) { out = (out << 1) | get_random_bit(); }
    return out;
}

/* generate a uniform random number in the range [0, max) */
static unsigned short get_random_int(unsigned short max) {
    unsigned short val;
    unsigned short low;
    // special case powers of 2
    switch (max) {
    case 1: return 0;
    case 2: return get_random_bit();
    case 4: return get_random_bits(2);
    case 8: return get_random_bits(3);
    default: break;
    }
    low = 128 % max;
    do {
        // this loop is necessary to ensure the numbers are uniformly
        // distributed.
        val = get_random_bits(7);
    } while (val < low);
    return val % max;
}

static void draw_nix_for_digit(GContext *ctx, unsigned short digit,
                               unsigned short rows, unsigned short top) {
    // make an array of top corners
    GPoint cells[rows*3];
    unsigned short i, j;
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
    // Durstenfeld's version of the Fisher-Yates shuffle
    for (i=rows*3; i>1; i--) {
        j = get_random_int(i);
        // swap the (i-1) and jth items in the list. (note i-1 can == j)
        GPoint tmp = cells[i-1];
        cells[i-1] = cells[j];
        cells[j] = tmp;
    }
    // draw the first 'digit' cells
    for (i=0; i<digit; i++) {
        GRect rect = GRect(cells[i].x, cells[i].y, BLOCK_SIZE_X, BLOCK_SIZE_Y);
        graphics_fill_rect(ctx, rect, 0, GCornerNone);
    }
}

static void time_layer_update_callback(Layer *me, GContext *ctx) {
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    (void)me;

    unsigned short display_hour = get_display_hour(tm->tm_hour);
    graphics_context_set_fill_color(ctx, GColorWhite);
    draw_nix_for_digit(ctx, display_hour/10, 1, HOURS_TENS_TOP);
    draw_nix_for_digit(ctx, display_hour%10, 3, HOURS_ONES_TOP);
    draw_nix_for_digit(ctx, tm->tm_min / 10, 2, MINUTES_TENS_TOP);
    draw_nix_for_digit(ctx, tm->tm_min % 10, 3, MINUTES_ONES_TOP);
}

// Called once per second
static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
    // Redraw every 3 seconds.
    if ((tick_time->tm_sec % 3) != 0) { return; }

    // Causes a redraw of the layer (via the associated layer update callback)
    // Note: This will cause the entire layer to be cleared first so it needs
    //       to be redrawn in its entirety
    layer_mark_dirty(s_time_layer);
    // XXX recompute time array?
}

static void main_window_load(Window *window) {
    GRect frame = GRect(0, 0, 144, 168);
    GRect inner = GRect(
        BLOCK_MARGIN_LEFT, HOURS_TENS_TOP,
        BLOCK_SPACING_X*3,
        (MINUTES_ONES_TOP - HOURS_TENS_TOP) + BLOCK_SPACING_Y*3
    );
    GRect bounds = GRect(
        -inner.origin.x, -inner.origin.y,
        frame.size.w, frame.size.h
    );
    window_set_background_color(window, GColorBlack);
    // Init the background image
    s_background_image = gbitmap_create_with_resource(
        RESOURCE_ID_NIX_BACKGROUND
    );
    s_background_image_layer = bitmap_layer_create(frame);
    bitmap_layer_set_bitmap(s_background_image_layer, s_background_image);
    layer_add_child(window_get_root_layer(window),
                    bitmap_layer_get_layer(s_background_image_layer));

    // Init the layer that shows the time.
    // Associate with layer object and set dimensions.
    // optimize redraw a little bit by setting a tight frame
    s_time_layer = layer_create(inner);
    layer_set_bounds((Layer *)s_time_layer, bounds);
    // Set the drawing callback function for the layer
    layer_set_update_proc(s_time_layer, time_layer_update_callback);
    // Add the child to the app's base window.
    layer_add_child(window_get_root_layer(window), s_time_layer);

    // Register with TickTimerService
    tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}

static void main_window_unload(Window *window) {
    gbitmap_destroy(s_background_image);
    bitmap_layer_destroy(s_background_image_layer);
    layer_destroy(s_time_layer);
    tick_timer_service_unsubscribe();
}

static void init() {
    // Create main Window element and assign to pointer
    s_window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });

    // Show the Window on the watch, with animated=true
    window_stack_push(s_window, true /* Animated */);
}

static void deinit() {
    // Destroy Window
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
