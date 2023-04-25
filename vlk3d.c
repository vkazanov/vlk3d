#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

/* The simplest game skeleton.

   See the Makefile for how to compile
*/


/* Constants */

#define WINDOW_WIDTH 1366
#define WINDOW_HEIGHT 768

#define FOV (M_PI / 3.0)
#define RAY_COUNT WINDOW_WIDTH
#define RAY_STEP 0.01
#define MAX_DISTANCE 20.0

/* Types/typedefs */

typedef struct {
    float x;
    float y;
} Vector2;

typedef struct {
    float x;
    float y;
    float direction;            /* degrees */
} Player;

typedef enum {
    GAME_RESULT_WIN,
    GAME_RESULT_DIE,
    GAME_RESULT_ABORT
} game_result_t;


#define PLAYER_ROTATION_SPEED 0.05
#define PLAYER_MOVEMENT_SPEED 0.1

#define PROJECTILE_SPEED 0.003f

int line_height_buffer[RAY_COUNT] = {0};

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 128

SDL_Texture *wall_texture = NULL;
SDL_Texture *wall_window_texture = NULL;
SDL_Texture *fly_texture = NULL;
SDL_Texture *poo_texture = NULL;
SDL_Texture *brush_texture = NULL;

/* Game state */

char **map;
int map_width;
int map_height;

Player player = {0, 0, 0};

#define ENEMY_PROXIMITY_DISTANCE 0.5

typedef struct Sprite Sprite;
struct Sprite {
    SDL_Texture *texture;
    float x, y;
    Vector2 direction;
    float hit_distance;
    bool is_visible;
    bool is_harmless;
    void (*update) (Sprite *Sprite, Uint32 elapsed_time);
    void (*hit) (Sprite *Sprite);

    float distance_to_player;
    float angle_to_player;
};

#define MAX_SPRITES 50
Sprite sprites[MAX_SPRITES];
int num_sprites = 0;


/* Function prototypes */

game_result_t game_loop(SDL_Renderer *renderer);
void handle_events(SDL_Event *event, bool *is_running);
void render_walls(SDL_Renderer *renderer);
float cast_ray(float angle);
bool is_wall(int x, int y);
bool is_within_bounds(int x, int y) ;
bool is_wall_collision(float x, float y);
bool is_horizontal_wall(Vector2 position);
bool is_close_to_enemy(float x, float y);
bool has_no_things_to_do();

void init_poo(Sprite *sprite, int x, int y);
void poo_hit(Sprite *sprite);

void init_fly(Sprite *sprite, int x, int y);
void fly_hit(Sprite *sprite);
void fly_update(Sprite *sprite, Uint32 elapsed_time);

void init_projectile(Sprite *sprite);
void projectile_update(Sprite *sprite, Uint32 elapsed_time);

void update_sprites(Uint32 elapsed_time);

void render_sprites(SDL_Renderer *renderer);
void fire_projectile(void);
void free_map(void);
void load_map(const char *filename);
void render_text(SDL_Renderer *renderer, const char *message, TTF_Font *font, SDL_Color color, SDL_Color outline_color, int x, int y);
void wait_for_key_press();

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL could not initialize: %s\n", SDL_GetError());
        return 1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Error initializing SDL_mixer: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }

    Mix_Music *music = Mix_LoadMUS("melody.mid");
    if (!music) {
        printf("Error loading MIDI file: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
        printf("Failed to initialize SDL_image: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF could not initialize: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 48);
    if (font == NULL) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }


    SDL_Window *window = SDL_CreateWindow("Nika's Room",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH,
                                          WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL);

    if (window == NULL) {
        fprintf(stderr, "Window could not be created: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "Renderer could not be created: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Surface *wall_surface = IMG_Load("wall.png");
    SDL_Surface *wall_window_surface = IMG_Load("wall_with_window.png");
    SDL_Surface *fly_surface = IMG_Load("fly.png");
    SDL_Surface *poo_surface = IMG_Load("poo.png");
    SDL_Surface *brush_surface = IMG_Load("brush.png");
    if (wall_surface == NULL || fly_surface == NULL || poo_surface == NULL || brush_surface == NULL || wall_window_surface == NULL) {
        fprintf(stderr, "Failed to load a texture: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    wall_texture = SDL_CreateTextureFromSurface(renderer, wall_surface);
    wall_window_texture = SDL_CreateTextureFromSurface(renderer, brush_surface);
    fly_texture = SDL_CreateTextureFromSurface(renderer, fly_surface);
    poo_texture = SDL_CreateTextureFromSurface(renderer, poo_surface);
    brush_texture = SDL_CreateTextureFromSurface(renderer, brush_surface);

    SDL_FreeSurface(wall_surface);
    SDL_FreeSurface(wall_window_surface);
    SDL_FreeSurface(fly_surface);
    SDL_FreeSurface(poo_surface);
    SDL_FreeSurface(brush_surface);

    if (wall_texture == NULL || fly_texture == NULL || poo_texture == NULL || brush_surface == NULL || wall_window_surface == NULL) {
        fprintf(stderr, "Failed to create a texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Mix_PlayMusic(music, -1);

    load_map("map.txt");

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255}; // New outline color
    switch (game_loop(renderer)) {
    case GAME_RESULT_WIN:
        render_text(renderer, "You win!", font, white, black, WINDOW_WIDTH / 2 - 75, WINDOW_HEIGHT / 2 - 24);
        SDL_RenderPresent(renderer);
        wait_for_key_press();
        break;
    case GAME_RESULT_DIE:
        render_text(renderer, "You lose!", font, white, black, WINDOW_WIDTH / 2 - 75, WINDOW_HEIGHT / 2 - 24);
        SDL_RenderPresent(renderer);
        wait_for_key_press();
        break;
    case GAME_RESULT_ABORT:
        fprintf(stderr, "Aborted");
        break;
    }

    free_map();

    Mix_FreeMusic(music);
    Mix_CloseAudio();

    SDL_DestroyTexture(wall_texture);
    SDL_DestroyTexture(wall_window_texture);
    SDL_DestroyTexture(poo_texture);
    SDL_DestroyTexture(fly_texture);
    SDL_DestroyTexture(brush_texture);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    IMG_Quit();

    SDL_Quit();

    return 0;
}

game_result_t game_loop(SDL_Renderer *renderer) {
    bool is_running = true;
    SDL_Event event;
    Uint32 current_time, last_time = SDL_GetTicks();

    while (is_running) {
        current_time = SDL_GetTicks();
        Uint32 elapsed_time = current_time - last_time;
        last_time = current_time;

        while (SDL_PollEvent(&event))
            handle_events(&event, &is_running);

        if (is_close_to_enemy(player.x, player.y))
            return GAME_RESULT_DIE;

        if (has_no_things_to_do())
            return GAME_RESULT_WIN;

        update_sprites(elapsed_time);

        render_walls(renderer);
        render_sprites(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    return GAME_RESULT_ABORT;
}

void handle_events(SDL_Event *event, bool *is_running) {
    switch (event->type) {
    case SDL_QUIT:
        *is_running = false;
        break;
    case SDL_KEYDOWN:
        /* Handling key presses */
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            *is_running = false;
        } else if (event->key.keysym.sym == SDLK_SPACE) {
            fire_projectile();
        } else if (event->key.keysym.sym == SDLK_UP) {
            float new_x = player.x + cosf(player.direction) * PLAYER_MOVEMENT_SPEED;
            float new_y = player.y + sinf(player.direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_wall_collision(new_x, new_y)) {
                player.x = new_x;
                player.y = new_y;
            }
        } else if (event->key.keysym.sym == SDLK_DOWN) {
            float new_x = player.x - cosf(player.direction) * PLAYER_MOVEMENT_SPEED;
            float new_y = player.y - sinf(player.direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_wall_collision(new_x, new_y)) {
                player.x = new_x;
                player.y = new_y;
            }
        } else if (event->key.keysym.sym == SDLK_LEFT) {
            player.direction -= PLAYER_ROTATION_SPEED;
        } else if (event->key.keysym.sym == SDLK_RIGHT) {
            player.direction += PLAYER_ROTATION_SPEED;
        }

        // Wrap player.direction within the range [0, 2 * M_PI]
        player.direction = fmod(player.direction, 2 * M_PI);
        if (player.direction < 0) {
            player.direction += 2 * M_PI;
        }

        break;
        /* Handle other event types here */
    default:
        break;
    }
}

void render_walls(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Draw the ceiling (white) */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect ceiling_rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT / 2};
    SDL_RenderFillRect(renderer, &ceiling_rect);

    /* Draw the floor (grey) */
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_Rect floor_rect = {0, WINDOW_HEIGHT / 2, WINDOW_WIDTH, WINDOW_HEIGHT / 2};
    SDL_RenderFillRect(renderer, &floor_rect);

    /* Draw walls using texture mapping */
    const float rays_per_column = (WINDOW_WIDTH / RAY_COUNT);
    const float angle_per_ray = (FOV / (float)RAY_COUNT);

    for (int i = 0; i < RAY_COUNT; i++) {
        float ray_angle = player.direction - FOV / 2.0 + i * angle_per_ray;
        float raw_distance = cast_ray(ray_angle);

        /* Calculate the line height while correcting for the fisheye effect */
        float corrected_distance = raw_distance * cosf(player.direction - ray_angle);
        int line_height = (int)(WINDOW_HEIGHT / corrected_distance);

        /* Save line height in a depth buffer to use in in sprite rendering */
        line_height_buffer[i] = line_height;

        /* Calculate the direction vector */
        Vector2 direction = {cosf(ray_angle), sinf(ray_angle)};

        /* Calculate the coordinate within a texture (0.0 ... 1.0) */
        Vector2 hit_position = {player.x + direction.x * raw_distance, player.y + direction.y * raw_distance};
        float tex_x;            /* texture coordinate */
        if (is_horizontal_wall(hit_position)) {
            /* A horizontal wall */
            tex_x = hit_position.x - floor(hit_position.x);
        } else {
            /* A vertical wall */
            tex_x = hit_position.y - floor(hit_position.y);
        }

        /* Set the source rectangle for the texture */
        int tex_rect_x = (int)(tex_x * (float)TEXTURE_WIDTH);
        int tex_rect_y = 0;
        SDL_Rect src_rect = {tex_rect_x, tex_rect_y, 1, TEXTURE_HEIGHT};

        /* Set the destination rectangle for the texture */
        SDL_Rect dest_rect = {i * rays_per_column, (WINDOW_HEIGHT - line_height) / 2, rays_per_column, line_height};

        /* Render the textured wall */
        SDL_RenderCopy(renderer, wall_texture, &src_rect, &dest_rect);
    }
}

bool is_wall(int x, int y) {
    return isdigit(map[y][x]);
}

bool is_within_bounds(int x, int y) {
    return x >= 0 && x < map_width && y >= 0 && y < map_height;
}

float cast_ray(float angle) {
    Vector2 direction = {cosf(angle), sinf(angle)};
    float distance = 0.0;
    Vector2 position = {player.x, player.y};

    while (distance < MAX_DISTANCE) {
        position.x += direction.x * RAY_STEP;
        position.y += direction.y * RAY_STEP;

        if (is_wall_collision(position.x, position.y)) {
            break;
        }

        distance += RAY_STEP;
    }

    return distance;
}

bool is_wall_collision(float x, float y) {
    int map_x = (int)floor(x);
    int map_y = (int)floor(y);

    if (is_within_bounds(map_x, map_y) && is_wall(map_x, map_y)) {
        return true;
    }

    return false;
}

bool is_horizontal_wall(Vector2 position) {
    /* Just check coordinates to see if the point is on a horizontal or a
     * vertical grid line */
    return fabs(round(position.x) - position.x) >= fabs(round(position.y) - position.y);
}

bool is_close_to_enemy(float x, float y) {
    for (int i = 0; i < num_sprites; i++) {
        if (!sprites[i].is_visible) {
            continue;
        }
        float distance = sqrtf(powf(x - sprites[i].x, 2) + powf(y - sprites[i].y, 2));
        if (distance < ENEMY_PROXIMITY_DISTANCE && !sprites[i].is_harmless) {
            return true;
        }
    }
    return false;
}

bool has_no_things_to_do() {
    for (int i = 0; i < num_sprites; i++) {
        if (!sprites[i].is_harmless) {
            return false;
        }
    }
    return true;
}

void init_poo(Sprite *sprite, int x, int y) {
    *sprite = (typeof(*sprite)) {
        .texture = poo_texture,
        .x = x + 0.5,
        .y = y + 0.5,
        .is_harmless = false,
        .is_visible = true,
        .hit_distance = 0.5,
        .hit = poo_hit
    };
}

void poo_hit(Sprite *sprite) {
    sprite->is_harmless = true;
    sprite->is_visible = false;
}

void init_fly(Sprite *sprite, int x, int y) {
    *sprite = (typeof(*sprite)) {
        .texture = fly_texture,
        .x = x + 0.5,
        .y = y + 0.5,
        .is_harmless = false,
        .is_visible = true,
        .hit_distance = 0.25,
        .update = fly_update,
        .hit = fly_hit
    };
}

void fly_hit(Sprite *sprite) {
    sprite->is_harmless = true;
    sprite->is_visible = false;
}

void init_projectile(Sprite *sprite) {
    *sprite = (typeof(*sprite)) {
        .texture = brush_texture,
        .is_visible = false,
        .is_harmless = true,
        .hit_distance = 1000,
        .update = projectile_update
    };

}

void projectile_update(Sprite *projectile, Uint32 elapsed_time) {
    float dx = projectile->direction.x * PROJECTILE_SPEED * elapsed_time;
    float dy = projectile->direction.y * PROJECTILE_SPEED * elapsed_time;

    projectile->x += dx;
    projectile->y += dy;

    if (is_wall_collision(projectile->x, projectile->y)) {
        projectile->is_visible = false;
        return;
    }

    /* check all other sprites but skip the first one - itself */
    for (int j = 1; j < num_sprites; j++) {
        if (!sprites[j].is_visible) {
            continue;
        }
        float dist_x = sprites[j].x - projectile->x;
        float dist_y = sprites[j].y - projectile->y;
        float distance = sqrtf(dist_x * dist_x + dist_y * dist_y);

        if (distance < sprites[j].hit_distance) {
            projectile->is_visible = false;
            sprites[j].hit(&sprites[j]);
            return;
        }
    }
}

float random_float(float min, float max) {
    float scale = rand() / (float) RAND_MAX;
    return min + scale * (max - min);
}

void fly_update(Sprite *sprite, Uint32 elapsed_time) {
    /* Speed factor*/
    const float speed = 0.002f;

    /* random directions */
    float dx = random_float(-1.0f, 1.0f);
    float dy = random_float(-1.0f, 1.0f);

    /* Scale the movement by the elapsed time and speed factor */
    dx *= elapsed_time * speed;
    dy *= elapsed_time * speed;

    /* Calculate the new position */
    float new_x = sprite->x + dx;
    float new_y = sprite->y + dy;

    /* Check if the new position is within map boundaries and not a wall */
    int new_tile_x = (int)new_x;
    int new_tile_y = (int)new_y;

    if (new_tile_x >= 0 && new_tile_x < map_width &&
        new_tile_y >= 0 && new_tile_y < map_height &&
        map[new_tile_y][new_tile_x] != '1') {

        sprite->x = new_x;
        sprite->y = new_y;
    }
}


void update_sprites(Uint32 elapsed_time) {
    for (int i = 0; i < num_sprites; i++) {
        if (!sprites[i].is_visible)
            continue;

        void (*update_function) = sprites[i].update;
        if (update_function)
            sprites[i].update(&sprites[i], elapsed_time);
    }
}

int compare_sprites_by_distance(const void *left, const void *right) {
    return ((Sprite *)left)->distance_to_player < ((Sprite *)right)->distance_to_player ? -1 : 1;
}

void render_sprites(SDL_Renderer *renderer) {
    /* Collect sprites that are visible and qsort them based on line height (aka
     * distance). This'll solve the sprite overlapping problem. */

    Sprite *sprites_visible[MAX_SPRITES] = {0};
    int num_sprites_visible = 0;
    for (int i = 0; i < num_sprites; i++) {
        if (!sprites[i].is_visible) {
            continue;
        }

        /* Angle between a player space positive x-axis and sprite positiion */
        float angle = atan2f(sprites[i].y - player.y, sprites[i].x - player.x);

        /* Find the angle between player's direction vector and the sprite and
         * normalize it */
        float relative_angle = player.direction - angle;
        if (relative_angle > M_PI) {
            relative_angle -= 2 * M_PI;
        }

        /* Check if the sprite is in the player's field of view */
        if (relative_angle < -FOV / 2.0 || relative_angle > FOV / 2.0) {
            continue;
        }

        /* Distance and fisheye effect correction */
        float distance_to_sprite = sqrtf(powf(sprites[i].x - player.x, 2) + powf(sprites[i].y - player.y, 2));
        distance_to_sprite *= cosf(relative_angle);

        sprites_visible[num_sprites_visible] = &sprites[i];
        sprites_visible[num_sprites_visible]->distance_to_player = distance_to_sprite;
        sprites_visible[num_sprites_visible]->angle_to_player = relative_angle;

        num_sprites_visible++;
    }

    /* Now, sort the array based on distance to the player */
    qsort(sprites_visible, num_sprites_visible, sizeof(Sprite *), compare_sprites_by_distance);

    /* Go through visible sprites and draw them */
    for (int i = 0; i < num_sprites_visible; i++) {
        Sprite *sprite = sprites_visible[i];

        /* Sprite line height based on the distance to the player  */
        int line_height = (int)(WINDOW_HEIGHT / sprite->distance_to_player);

        /* Calculate the horizontal position of the enemy on the screen */
        int screen_x = (int)((WINDOW_WIDTH / 2) - tanf(sprite->angle_to_player) * (WINDOW_WIDTH / 2) / tanf(FOV / 2));

        /* Calculate the size of the sprite */
        const int sprite_size = (int)(line_height);

        /* Render the texture column by column */
        for (int col = 0; col < sprite_size; col++) {
            /* find the current sprite column to check and make sure it doesn't
             * go above the number of rays casted - this might cause a crash
             * when standing right next to the sprites */
            int screen_col = screen_x - sprite_size / 2 + col;
            if (screen_col < 0 || screen_col >= RAY_COUNT)
                continue;

            /* see if the wall column for this ray is further away than sprite
             * column. Ignore otherise.*/
            int wall_line_height = line_height_buffer[screen_col];
            if (line_height < wall_line_height)
                continue;

            /* Calculate the source and destination rectangles for the
             * current column. Note that ceilf here is necessary to avoid
             * zero-width rectangles */
            SDL_Rect src_rect = {ceilf(col * (float)TEXTURE_WIDTH / sprite_size), 0, ceilf((float)TEXTURE_WIDTH / sprite_size), TEXTURE_HEIGHT};
            SDL_Rect dest_rect = {(screen_x - sprite_size / 2) + col, (WINDOW_HEIGHT - sprite_size) / 2, 1, sprite_size};

            /* Render the current column of the texture */
            SDL_RenderCopyEx(renderer, sprite->texture, &src_rect, &dest_rect, 0, NULL, SDL_FLIP_NONE);
        }
    }
}

void fire_projectile(void) {
    Sprite *projectile = &sprites[0];
    if (projectile->is_visible)
        return;
    projectile->direction = (Vector2){cosf(player.direction), sinf(player.direction)};
    projectile->x = player.x;
    projectile->y = player.y;
    projectile->is_visible = true;
}

void load_map(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening map file: %s\n", filename);
        exit(1);
    }

    fscanf(file, "%d %d\n", &map_width, &map_height);

    map = (char **)malloc(map_height * sizeof(char *));

    /* the first sprite is always the projectile */
    init_projectile(&sprites[0]);
    num_sprites++;

    /* need to make sure the player was there */
    bool player_start_found = false;

    /* walk through all map cells, load the map and all the sprites*/
    fprintf(stderr, "File: %s\n", filename);
    fprintf(stderr, "Dimensions: %d x %d\n", map_width, map_height);

    for (int y = 0; y < map_height; y++) {
        /* size = width + newline + null */
        map[y] = malloc(map_width * sizeof(char) + 2);
        fgets(map[y], map_width * sizeof(char) + 2, file);
        for (int x = 0; x < map_width; x++) {
            char c = map[y][x];
            fprintf(stderr, "%c", c);

            switch (c) {
            case '@':
                player.x = x + 0.5;
                player.y = y + 0.5;
                map[y][x] = ' ';
                player_start_found = true;
                break;
            case 'p':
                if (num_sprites >= MAX_SPRITES) {
                    goto too_many_sprites;
                }
                init_poo(&sprites[num_sprites], x, y);
                num_sprites++;
                map[y][x] = ' ';
                break;
            case 'f':
                if (num_sprites >= MAX_SPRITES) {
                    goto too_many_sprites;
                }
                init_fly(&sprites[num_sprites], x, y);
                num_sprites++;
                map[y][x] = ' ';
                break;
            default:
                continue;
            }
        }
        fprintf(stderr, "\n");
    }

    if (!player_start_found) {
        fprintf(stderr, "No starting position found in the map file: %s\n", filename);
        exit(1);
    }

    fclose(file);
    return;

too_many_sprites:
    fprintf(stderr, "No starting position found in the map file: %s\n", filename);
    exit(1);
}

void free_map(void) {
    for (int y = 0; y < map_height; y++) {
        free(map[y]);
    }
    free(map);
}

void render_text(SDL_Renderer *renderer, const char *message, TTF_Font *font, SDL_Color color, SDL_Color outline_color, int x, int y) {
    SDL_Surface *text_surface = TTF_RenderText_Blended(font, message, color);
    SDL_Surface *outline_surface = TTF_RenderText_Blended(font, message, outline_color);

    SDL_Rect offset;
    offset.x = -1;
    offset.y = -1;
    SDL_BlitSurface(outline_surface, NULL, text_surface, &offset);
    offset.x = 1;
    offset.y = -1;
    SDL_BlitSurface(outline_surface, NULL, text_surface, &offset);
    offset.x = -1;
    offset.y = 1;
    SDL_BlitSurface(outline_surface, NULL, text_surface, &offset);
    offset.x = 1;
    offset.y = 1;
    SDL_BlitSurface(outline_surface, NULL, text_surface, &offset);

    SDL_Texture *text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

    SDL_Rect dest;
    dest.x = x;
    dest.y = y;
    dest.w = text_surface->w;
    dest.h = text_surface->h;

    SDL_RenderCopy(renderer, text_texture, NULL, &dest);
    SDL_FreeSurface(text_surface);
    SDL_FreeSurface(outline_surface);
    SDL_DestroyTexture(text_texture);
}


void wait_for_key_press() {
    SDL_Event event;
    bool is_running = true;
    while (is_running) {
        SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            is_running = false;
            break;
        case SDL_KEYDOWN:
            is_running = false;
            break;
        default:
            break;
        }
    }
}
