#include "flipper_rng.h"
#include "flipper_rng_about.h"
#include "qrcode.h"
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

#define TAG "FlipperRNG"

// Bitcoin donation address - Replace with your own!
#define BTC_ADDRESS "bc1q4usujj2pujxhh23fgy0dfzrweh7k9zaqm2t0fq"

typedef struct {
    QRCode* qrcode;
    uint8_t* qrcode_data;
    bool show_qr;
} FlipperRngAboutModel;

static void flipper_rng_about_draw_callback(Canvas* canvas, void* context) {
    FlipperRngAboutModel* model = context;
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    if(!model->show_qr) {
        // Show info text with header
        canvas_set_font(canvas, FontPrimary);
        char title[32];
        snprintf(title, sizeof(title), "FlipperRNG v%s", FLIPPER_RNG_VERSION);
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, title);
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 20, "High-quality entropy");
        canvas_draw_str(canvas, 2, 30, "from HW RNG, RF & IR");
        
        canvas_draw_str(canvas, 2, 44, "Author: Luke Macken");
        canvas_draw_str(canvas, 2, 54, "Press OK for donation QR");
    } else {
        // Show QR code - side by side layout
        if(model->qrcode) {
            // Draw QR code on the left
            uint8_t size = model->qrcode->size;
            uint8_t pixel_size = 2;  // Make each QR module 2x2 pixels for better visibility
            uint8_t qr_size = size * pixel_size;
            
            // Position QR code on the left side
            uint8_t left = 2;  // Small margin from left edge
            uint8_t top = (64 - qr_size) / 2;  // Center vertically
            
            // Draw QR code
            for(uint8_t y = 0; y < size; y++) {
                for(uint8_t x = 0; x < size; x++) {
                    if(qrcode_getModule(model->qrcode, x, y)) {
                        if(pixel_size == 1) {
                            canvas_draw_dot(canvas, left + x, top + y);
                        } else {
                            // Draw 2x2 pixel block for each module
                            canvas_draw_box(canvas, 
                                left + x * pixel_size, 
                                top + y * pixel_size, 
                                pixel_size, 
                                pixel_size);
                        }
                    }
                }
            }
            
            // Draw text on the right side
            uint8_t text_left = left + qr_size + 4;  // Start text after QR with margin
            
            // Title on one line
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, text_left, 10, "Donate BTC:");
            
            // Address split into multiple lines
            canvas_set_font(canvas, FontSecondary);
            const char* addr = BTC_ADDRESS;
            size_t addr_len = strlen(addr);
            
            // Calculate how many chars fit per line in the remaining space
            uint8_t chars_per_line = (128 - text_left) / 5;  // Approx 5 pixels per char
            if(chars_per_line > 12) chars_per_line = 12;  // Max reasonable width
            
            // Split address into lines
            uint8_t line_y = 22;  // Start closer to title since it's now one line
            size_t addr_pos = 0;
            
            while(addr_pos < addr_len && line_y < 62) {
                char line[13] = {0};  // Max 12 chars + null
                size_t copy_len = addr_len - addr_pos;
                if(copy_len > chars_per_line) copy_len = chars_per_line;
                
                memcpy(line, addr + addr_pos, copy_len);
                line[copy_len] = '\0';
                
                canvas_draw_str(canvas, text_left, line_y, line);
                
                addr_pos += copy_len;
                line_y += 8;  // Slightly tighter line spacing
            }
        }
    }
}

static bool flipper_rng_about_input_callback(InputEvent* event, void* context) {
    View* view = context;
    bool consumed = false;
    
    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            // Toggle between info and QR code
            with_view_model(
                view,
                FlipperRngAboutModel* model,
                {
                    model->show_qr = !model->show_qr;
                },
                true
            );
            consumed = true;
        } else if(event->key == InputKeyBack) {
            // Back handled by previous callback
            consumed = false;
        }
    }
    
    return consumed;
}

View* flipper_rng_about_view_alloc(void) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FlipperRngAboutModel));
    view_set_context(view, view);  // Set view as its own context for input callback
    view_set_draw_callback(view, flipper_rng_about_draw_callback);
    view_set_input_callback(view, flipper_rng_about_input_callback);
    
    with_view_model(
        view,
        FlipperRngAboutModel* model,
        {
            // Initialize QR code for BTC address with bitcoin: URI
            uint8_t version = 3;  // Version 3 = 29x29 modules
            size_t buffer_size = qrcode_getBufferSize(version);
            model->qrcode_data = malloc(buffer_size);
            
            if(model->qrcode_data) {
                model->qrcode = malloc(sizeof(QRCode));
                if(model->qrcode) {
                    // Create bitcoin URI
                    char bitcoin_uri[64];
                    snprintf(bitcoin_uri, sizeof(bitcoin_uri), "bitcoin:%s", BTC_ADDRESS);
                    
                    int8_t res = qrcode_initBytes(
                        model->qrcode,
                        model->qrcode_data,
                        MODE_BYTE,
                        version,
                        ECC_LOW,
                        (uint8_t*)bitcoin_uri,
                        strlen(bitcoin_uri)
                    );
                    
                    if(res != 0) {
                        FURI_LOG_E(TAG, "Failed to generate QR code");
                        free(model->qrcode);
                        free(model->qrcode_data);
                        model->qrcode = NULL;
                        model->qrcode_data = NULL;
                    }
                } else {
                    free(model->qrcode_data);
                    model->qrcode_data = NULL;
                }
            }
            
            model->show_qr = false;
        },
        true
    );
    
    return view;
}

void flipper_rng_about_view_free(View* view) {
    with_view_model(
        view,
        FlipperRngAboutModel* model,
        {
            if(model->qrcode) {
                free(model->qrcode);
            }
            if(model->qrcode_data) {
                free(model->qrcode_data);
            }
        },
        false
    );
    view_free(view);
}
