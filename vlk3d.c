#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

/* The simplest game skeleton.

   Compile with something like:
   gcc -o vlk3d vlk3d.c (sdl2-config --cflags --libs) -lm -lSDL2_image -lSDL2_ttf; and ./vlk3d
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

typedef struct Projectile {
    Vector2 position;
    Vector2 direction;
    struct Projectile *next;
} Projectile;

#define PROJECTILE_SPEED 0.1
#define PROJECTILE_COLOR_R 255
#define PROJECTILE_COLOR_G 0
#define PROJECTILE_COLOR_B 0

int line_height_buffer[RAY_COUNT] = {0};

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 128

SDL_Texture *wall_texture = NULL;
SDL_Texture *fly_texture = NULL;
SDL_Texture *poo_texture = NULL;

/* Game state */

Projectile *projectiles = NULL;

int **map;
int map_width;
int map_height;

Player player = {0, 0, 0};

#define ENEMY_PROXIMITY_DISTANCE 0.5

typedef struct {
    float x, y;
    float hit_distance;
    bool is_visible;
    bool is_harmless;
} Sprite;

#define MAX_SPRITES 50
Sprite sprites[MAX_SPRITES];
int num_sprites = 0;


/* Function prototypes */

game_result_t game_loop(SDL_Renderer *renderer);
void handle_events(SDL_Event *event, bool *is_running);
void render(SDL_Renderer *renderer);
float cast_ray(float angle);
bool is_wall_collision(float x, float y);
bool is_horizontal_wall(Vector2 position);
bool is_close_to_enemy(float x, float y);
bool has_no_things_to_do();
bool should_render(Sprite *sprite);

void init_poo(Sprite *sprite, int x, int y);
void poo_hit(Sprite *sprite);

void update_projectiles();
void render_projectiles(SDL_Renderer *renderer);
void render_sprites(SDL_Renderer *renderer);
void fire_projectile();
void free_projectiles();
void free_map();
void load_map(const char *filename);
void render_text(SDL_Renderer *renderer, const char *message, TTF_Font *font, SDL_Color color, SDL_Color outline_color, int x, int y);
void wait_for_key_press();

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize: %s\n", SDL_GetError());
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

    SDL_Surface *wall_surface = IMG_Load("wallpaper.png");
    SDL_Surface *fly_surface = IMG_Load("fly.png");
    SDL_Surface *poo_surface = IMG_Load("poo.png");
    if (wall_surface == NULL || fly_surface == NULL || poo_surface == NULL) {
        fprintf(stderr, "Failed to load a texture: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    wall_texture = SDL_CreateTextureFromSurface(renderer, wall_surface);
    fly_texture = SDL_CreateTextureFromSurface(renderer, fly_surface);
    poo_texture = SDL_CreateTextureFromSurface(renderer, poo_surface);

    SDL_FreeSurface(wall_surface);
    SDL_FreeSurface(fly_surface);
    SDL_FreeSurface(poo_surface);

    if (wall_texture == NULL || fly_texture == NULL || poo_texture == NULL) {
        fprintf(stderr, "Failed to create a texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

    free_projectiles();
    free_map();

    SDL_DestroyTexture(wall_texture);
    SDL_DestroyTexture(poo_texture);
    SDL_DestroyTexture(fly_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    IMG_Quit();  // Call this when you're done with SDL_image

    SDL_Quit();

    return 0;
}

game_result_t game_loop(SDL_Renderer *renderer) {
    bool is_running = true;
    SDL_Event event;

    while (is_running) {
        while (SDL_PollEvent(&event))
            handle_events(&event, &is_running);

        if (is_close_to_enemy(player.x, player.y))
            return GAME_RESULT_DIE;

        if (has_no_things_to_do())
            return GAME_RESULT_WIN;

        update_projectiles();
        render(renderer);
        render_projectiles(renderer);
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
            fire_projectile(player);
        } else if (event->key.keysym.sym == SDLK_UP) {
            float newX = player.x + cosf(player.direction) * PLAYER_MOVEMENT_SPEED;
            float newY = player.y + sinf(player.direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_wall_collision(newX, newY)) {
                player.x = newX;
                player.y = newY;
            }
        } else if (event->key.keysym.sym == SDLK_DOWN) {
            float newX = player.x - cosf(player.direction) * PLAYER_MOVEMENT_SPEED;
            float newY = player.y - sinf(player.direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_wall_collision(newX, newY)) {
                player.x = newX;
                player.y = newY;
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

void render(SDL_Renderer *renderer) {
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

float cast_ray(float angle) {
    Vector2 direction = {cosf(angle), sinf(angle)};
    float distance = 0.0;
    Vector2 position = {player.x, player.y};

    while (distance < MAX_DISTANCE) {
        position.x += direction.x * RAY_STEP;
        position.y += direction.y * RAY_STEP;

        int map_x = (int)floor(position.x);
        int map_y = (int)floor(position.y);

        if (map_x >= 0 && map_x < map_width && map_y >= 0 && map_y < map_height && map[map_y][map_x] == 1) {
            break;
        }

        distance += RAY_STEP;
    }

    return distance;
}

bool is_wall_collision(float x, float y) {
    int mapX = (int)floor(x);
    int mapY = (int)floor(y);

    if (mapX >= 0 && mapX < map_width && mapY >= 0 && mapY < map_height && map[mapY][mapX] == 1) {
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

bool should_render(Sprite *sprite) {
    return sprite->is_visible;
}

void init_poo(Sprite *sprite, int x, int y) {
    *sprite = (typeof(*sprite)) {
        .x = x + 0.5,
        .y = y + 0.5,
        .is_harmless = false,
        .is_visible = true,
        .hit_distance = 0.5
    };
}

void poo_hit(Sprite *sprite) {
    sprite->is_harmless = true;
    sprite->is_visible = false;
}

void update_projectiles() {
    Projectile *current = projectiles;
    Projectile *prev = NULL;

    while (current != NULL) {
        current->position.x += current->direction.x * PROJECTILE_SPEED;
        current->position.y += current->direction.y * PROJECTILE_SPEED;

        bool remove_projectile = false;

        if (is_wall_collision(current->position.x, current->position.y)) {
            remove_projectile = true;
        }

        // Check for collisions with enemies
        for (int j = 0; j < num_sprites; j++) {
            float dx = sprites[j].x - current->position.x;
            float dy = sprites[j].y - current->position.y;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < sprites[j].hit_distance) {
                remove_projectile = true;
                sprites[j].is_harmless = true;
                sprites[j].is_visible = false;
            }
        }

        if (remove_projectile) {
            if (prev) {
                prev->next = current->next;
            } else {
                projectiles = current->next;
            }

            Projectile *to_remove = current;
            current = current->next;
            free(to_remove);
        } else {
            prev = current;
            current = current->next;
        }
    }
}

void render_projectiles(SDL_Renderer *renderer) {
    Projectile *current = projectiles;

    while (current != NULL) {
        float angle = atan2f(current->position.y - player.y, current->position.x - player.x);
        if (angle < 0) {
            angle += 2 * M_PI;
        }
        float distance_to_projectile = sqrtf(powf(current->position.x - player.x, 2) + powf(current->position.y - player.y, 2));
        float relative_angle = player.direction - angle;

        // Check if the projectile is in the player's field of view
        if (relative_angle > -FOV / 2.0 && relative_angle < FOV / 2.0) {
            int line_height = (int)(WINDOW_HEIGHT / distance_to_projectile);

            // Calculate the horizontal position of the projectile on the screen
            int screen_x = (int)((WINDOW_WIDTH / 2) - tanf(relative_angle) * (WINDOW_WIDTH / 2) / tanf(FOV / 2));

            // Calculate the size of the projectile square, taking perspective into account
            int square_size = (int)(line_height * 0.2);

            // Set the color and render the projectile as a square
            SDL_SetRenderDrawColor(renderer, PROJECTILE_COLOR_R, PROJECTILE_COLOR_G, PROJECTILE_COLOR_B, 255);
            SDL_Rect square = {screen_x - square_size / 2, (WINDOW_HEIGHT - line_height) / 2 + (line_height - square_size) / 2, square_size, square_size};
            SDL_RenderFillRect(renderer, &square);
        }

        current = current->next;
    }
}

void render_sprites(SDL_Renderer *renderer) {
    for (int i = 0; i < num_sprites; i++) {
        /* check if the sprite should be rendered at all  */
        if (!should_render(&sprites[i])) {
            continue;
        }

        /* Angle between a player space positive x-axis and enemy's positiion */
        float angle = atan2f(sprites[i].y - player.y, sprites[i].x - player.x);

        /* Find the angle between player's direction vector and the enemy and
         * normalize it */
        float relative_angle = player.direction - angle;
        if (relative_angle > M_PI) {
            relative_angle -= 2 * M_PI;
        }

        /* Check if the enemy is in the player's field of view */
        if (relative_angle < -FOV / 2.0 || relative_angle > FOV / 2.0) {
            continue;
        }

        /* Distance and fisheye effect correction */
        float distance_to_enemy = sqrtf(powf(sprites[i].x - player.x, 2) + powf(sprites[i].y - player.y, 2));
        distance_to_enemy *= cosf(relative_angle);

        /* Sprite line height based on the distance to the enemy  */
        int line_height = (int)(WINDOW_HEIGHT / distance_to_enemy);

        /* Calculate the horizontal position of the enemy on the screen */
        int screen_x = (int)((WINDOW_WIDTH / 2) - tanf(relative_angle) * (WINDOW_WIDTH / 2) / tanf(FOV / 2));

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
            SDL_RenderCopyEx(renderer, poo_texture, &src_rect, &dest_rect, 0, NULL, SDL_FLIP_NONE);
        }
    }
}

void fire_projectile(void) {
    Projectile *new_projectile = (Projectile *)malloc(sizeof(Projectile));
    new_projectile->position = (Vector2){player.x, player.y};
    new_projectile->direction = (Vector2){cosf(player.direction), sinf(player.direction)};
    new_projectile->next = projectiles;

    projectiles = new_projectile;
}

void free_projectiles() {
    Projectile *current = projectiles;

    while (current != NULL) {
        Projectile *to_remove = current;
        current = current->next;
        free(to_remove);
    }
}

void load_map(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening map file: %s\n", filename);
        exit(1);
    }

    fscanf(file, "%d %d", &map_width, &map_height);

    map = (int **)malloc(map_height * sizeof(int *));
    bool player_start_found = false;
    for (int y = 0; y < map_height; y++) {
        map[y] = (int *)malloc(map_width * sizeof(int));
        for (int x = 0; x < map_width; x++) {
            fscanf(file, "%1d", &map[y][x]);

            if (map[y][x] == 9) {
                player.x = x + 0.5;
                player.y = y + 0.5;
                map[y][x] = 0;
                player_start_found = true;
            } else if (map[y][x] == 2) {
                /* do not overflow the number of sprits */
                if (num_sprites >= MAX_SPRITES) {
                    fprintf(stderr, "No starting position found in the map file: %s\n", filename);
                    exit(1);
                }
                init_poo(&sprites[num_sprites], x, y);;
                num_sprites++;
                map[y][x] = 0;
            }

        }
    }

    if (!player_start_found) {
        fprintf(stderr, "No starting position found in the map file: %s\n", filename);
        exit(1);
    }

    fclose(file);
}

void free_map() {
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
