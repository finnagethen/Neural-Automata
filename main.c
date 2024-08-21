#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600

#define COLUMNS 300
#define ROWS 300

#define FPS 30
#define FRAMES_TO_SKIP 1


typedef struct {
    float filter[3][3];
    float (*activation)(float);
} settings_t;

float activation_waves(float x) {
    return fabsf(1.2f * x);
}

float activation_worms(float x) {
    return -1.0f / powf(2.0f, (0.6f * powf(x, 2.0f))) + 1.0f;
}

float activation_game_of_life(float x) {
    if (x == 3.0f || x == 11.0f || x == 12.0f) {
        return 1.0f;
    }
    return 0.0f;
}

settings_t settings[] = {
    {
        .filter = {
            {0.565f, -0.716f, 0.565f},
            {-0.716f, 0.627f, -0.716f},
            {0.565f, -0.716f, 0.565f}
        },
        .activation = activation_waves
    },
    {
        .filter = {
            {0.68f, -0.9f, 0.68f},
            {-0.9f, -0.66f, -0.9f},
            {0.68f, -0.9f, 0.68f}
        },
        .activation = activation_worms
    },
    {
        .filter = {
            {1.0f, 1.0f, 1.0f},
            {1.0f, 9.0f, 1.0f},
            {1.0f, 1.0f, 1.0f}
        },
        .activation = activation_game_of_life
    }
};

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;

    Uint32 pixels[COLUMNS * ROWS];
    float board[COLUMNS * ROWS];

    bool quit;
    bool wait;

    struct {
        clock_t last_clock;
        double delta_sec;
        int frame_cnt;
    } time;

} global_t;

global_t global = { };


int get_index(int x, int y) {
    return y * COLUMNS + x;
}

float clamp(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

float get_neighbors_sum(int x, int y, settings_t* settings) {
    float neighbors_sum = 0.0f;

    for (int i = -1; i <= 1; i++) {
        int ny = (y + i + ROWS) % ROWS;

        for (int j = -1; j <= 1; j++) {
            int nx = (x + j + COLUMNS) % COLUMNS;

            neighbors_sum += global.board[get_index(nx, ny)] * settings->filter[j + 1][i + 1];
        }
    }

    return neighbors_sum;
}

void simulation(settings_t* settings) {
    float new_board[COLUMNS * ROWS];

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLUMNS; x++) {
            new_board[get_index(x, y)] = get_neighbors_sum(x, y, settings);
        }
    }

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLUMNS; x++) {
            global.board[get_index(x, y)] = clamp(settings->activation(new_board[get_index(x, y)]), 0.0f, 1.0f);
        }
    }
}

Uint32 get_hex_from_rgba(int r, int g, int b, int a) {
    return (r << 24) | (g << 16) | (b << 8) | a;
}

void put_pixel(float x, int y) {
    if (x < 0 || x >= COLUMNS) return;
    if (y < 0 || y >= ROWS) return;

    global.board[get_index(x, y)] = 1.0f;
}

void erase_pixel(int x, int y) {
    if (x < 0 || x >= COLUMNS) return;
    if (y < 0 || y >= ROWS) return;

    global.board[get_index(x, y)] = 0.0f;
}

void on_mouse_release(SDL_MouseButtonEvent* button) {
    int x = (button->x / WINDOW_WIDTH) * COLUMNS;
    int y  = (button->y / WINDOW_HEIGHT) * ROWS;

    switch (button->button) {
        case 1: put_pixel(x, y);    // left button
            break;
        case 3: erase_pixel(x, y);  // right button
            break;
    }
}

void on_mouse_motion(SDL_MouseMotionEvent* motion) {
    int x = (motion->x / WINDOW_WIDTH) * COLUMNS;
    int y  = (motion->y / WINDOW_HEIGHT) * ROWS;

    if (motion->state & SDL_BUTTON_LMASK) {
        put_pixel(x, y);
    }
    else if (motion->state & SDL_BUTTON_RMASK) {
        erase_pixel(x, y);
    }
}

void on_key_release(SDL_KeyboardEvent* key) {
    if (key->key != SDLK_SPACE) return;

    global.wait = !global.wait;
}

bool render() {
    for (int x = 0; x < COLUMNS; x++) {
        for (int y = 0; y < ROWS; y++) {
            float value = global.board[get_index(x, y)];
            global.pixels[get_index(x, y)] = get_hex_from_rgba((int)(255.0f * value), (int)(255.0f * value), (int)(255.0f * value), 0xFF);
        }
    }

    if (SDL_UpdateTexture(global.texture, NULL, global.pixels, COLUMNS * sizeof(Uint32)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_SetRenderDrawColor(global.renderer, 0x00, 0x00, 0x00, 0xFF) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_SetRenderDrawColor failed: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_RenderClear(global.renderer) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_RenderClear failed: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_RenderTexture(global.renderer, global.texture, NULL, NULL) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_RenderTexture failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_RenderPresent(global.renderer);

    return true;
}

void update(settings_t* settings) {
    simulation(settings);
}

int main(int argc, char* argv[]) {
    int settings_size = sizeof(settings) / sizeof(settings_t);

    if (argc != 2) {
        fprintf(stderr, "Usage: ./%s {SETTING_INDEX: 1 - %d}", argv[0], settings_size);
        return 1;
    }

    int settings_index = atoi(argv[1]) - 1;
    if (settings_index > settings_size - 1 || settings_index < 0) {
        fprintf(stderr, "Error: Use a index between 1 - %d", settings_size);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Init failed: %s\n", SDL_GetError());
        goto SDL_Init_Failure;
    }

    if (SDL_CreateWindowAndRenderer("Game Of Life", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &global.window, &global.renderer) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
        goto SDL_CreateWindowAndRenderer_Failure;
    }

    global.texture = SDL_CreateTexture(global.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, COLUMNS, ROWS);
    if (!global.texture) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        goto SDL_CreateTexture_Failure;
    }

    // Set scaling mode to nearest-neighbor to prevent blurring
    SDL_SetTextureScaleMode(global.texture, SDL_SCALEMODE_NEAREST);

    // Init board for simulation
    srand(time(NULL));
    for (int x = 0; x < COLUMNS; x++) {
        for (int y = 0; y < ROWS; y++) {
            if (rand() % 2 == 0) {
                global.board[get_index(x, y)] = 1.0f;
            }
        }
    }

    SDL_Event event;
    while (!global.quit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: global.quit = true;
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP: on_mouse_release(&event.button);
                    break;
                case SDL_EVENT_MOUSE_MOTION: on_mouse_motion(&event.motion);
                    break;
                case SDL_EVENT_KEY_UP: on_key_release(&event.key);
                    break;
            }
        }

        clock_t now_clock = clock();
        global.time.delta_sec = (double)(now_clock - global.time.last_clock) / CLOCKS_PER_SEC;
        if (global.time.delta_sec < 1.0 / (double)FPS) continue;
        global.time.last_clock = now_clock;

        global.time.frame_cnt += 1;
        if (global.time.frame_cnt > FRAMES_TO_SKIP) {
            global.time.frame_cnt = 0;

            if (!render()) {
                break;
            }
        }

        if (!global.wait) {
            update(&settings[settings_index]);
        }
    }

    SDL_DestroyTexture(global.texture);

SDL_CreateTexture_Failure:

    SDL_DestroyRenderer(global.renderer);
    SDL_DestroyWindow(global.window);

SDL_CreateWindowAndRenderer_Failure:

    SDL_Quit();

SDL_Init_Failure:

    return 0;
}
