#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <assert.h>
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
    GAME_RESULT_DIE,            /* TODO: unused for now */
    GAME_RESULT_ABORT
} game_result_t;


#define PLAYER_ROTATION_SPEED 0.05
#define PLAYER_MOVEMENT_SPEED 0.1

#define PROJECTILE_SPEED 0.003f

TTF_Font *font = NULL;

Mix_Chunk *door_sound = NULL;
Mix_Chunk *pain_sound = NULL;
Mix_Chunk *brush_sound = NULL;

int line_height_buffer[RAY_COUNT] = {0};

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 128

SDL_Texture *wall_texture = NULL;
SDL_Texture *wall_window_texture = NULL;
SDL_Texture *wall_painting_texture = NULL;
SDL_Texture *wall_door_texture = NULL;
SDL_Texture *fly_texture = NULL;
SDL_Texture *poo_texture = NULL;
SDL_Texture *brush_texture = NULL;
SDL_Texture *flower_texture = NULL;
SDL_Texture *coin_texture = NULL;

struct {
    SDL_Texture **texture;
    char *name;
} name_to_texture_table[] = {
    { &wall_texture, "wall.png"},
    { &wall_window_texture, "wall_with_window.png"},
    { &wall_door_texture, "wall_with_door.png"},
    { &wall_painting_texture, "wall_painting.png"},
    { &fly_texture, "fly.png"},
    { &poo_texture, "poo.png"},
    { &brush_texture, "brush.png"},
    { &flower_texture, "flower.png"},
    { &coin_texture, "coin.png"},
};

SDL_Texture **char_to_texture_table[] = {
    ['1'] = &wall_texture,
    ['2'] = &wall_window_texture,
    ['3'] = &wall_painting_texture,
    ['-'] = &wall_door_texture,
    ['|'] = &wall_door_texture
};

/* Game state */

/* Tile map */
char **map;
int map_width;
int map_height;

Player player = {0, 0, 0};
int coins_collected = 0;
int enemies_left = 0;

#define ENEMY_PROXIMITY_DISTANCE 0.5

/* Objects are updateable entities. They might represent a object (if the
 * texture is there) */
typedef struct Object Object;
struct Object {
    SDL_Texture *texture;
    float x, y;
    Vector2 direction;
    float hit_distance;
    float touch_distance;

    bool is_updateable;
    bool is_hittable;
    bool is_visible;
    bool is_harmless;
    bool is_touchable;

    void (*update) (Object *Object, Uint32 elapsed_time);
    void (*hit) (Object *Object);
    void (*touch) (Object *Object);

    float distance_to_player;
    float angle_to_player;

    union {
        struct {
            bool is_opening;
            float door_width;
        } door;
    } as;
};

#define MAX_OBJECTS 50
Object objects[MAX_OBJECTS];
int num_objects = 0;


/* A map of doors in the game. Each float is a state of the door, i.e. the
 * door_width "persentage" use to either draw door column upon ray hit, or just
 * ignore it */
Object ***door_map;


/* Function prototypes */

game_result_t game_loop(SDL_Renderer *renderer);
void handle_events(SDL_Event *event, bool *is_running);
void render_walls(SDL_Renderer *renderer);
float cast_ray(float angle, char *wall_type, float *tex_offset) ;
bool is_wall(int x, int y);
bool is_within_bounds(int x, int y) ;
bool is_collision(float x, float y);
bool is_door_collision(float x, float y, char *wall_type, float *tex_offset);
bool is_wall_collision(float x, float y, char *wall_type, float *tex_offset);
bool is_horizontal_wall(Vector2 position);
bool has_no_things_to_do();

void init_poo(Object *object, int x, int y);
void poo_hit(Object *object);
void poo_touch(Object *object);

void init_fly(Object *object, int x, int y);
void fly_hit(Object *object);
void fly_update(Object *object, Uint32 elapsed_time);
void fly_touch(Object *object);

void init_flower(Object *object, int x, int y);

void init_coin(Object *object, int x, int y);
void touch_coin(Object *object);

void init_projectile(Object *object);
void projectile_update(Object *object, Uint32 elapsed_time);

void init_door(Object *object, int x, int y);
void door_hit(Object *object);
void door_update(Object *object, Uint32 elapsed_time);

void update_objects(Uint32 elapsed_time);

void render_sprites(SDL_Renderer *renderer);
void render_text(SDL_Renderer *renderer, const char *message, SDL_Color color, SDL_Color outline_color, int x, int y);
void render_ui(SDL_Renderer *renderer);

void fire_projectile(void);
void free_maps(void);
void load_maps(const char *filename);
void wait_for_key_press();

void load_sound(void);
void free_sound(void);

void load_textures(SDL_Renderer *renderer);
void free_textures(void);

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL could not initialize: %s\n", SDL_GetError());
        return 1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 128) < 0) {
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

    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 48);
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

    load_sound();
    load_textures(renderer);
    load_maps("map.txt");
    Mix_PlayMusic(music, -1);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255}; // New outline color
    switch (game_loop(renderer)) {
    case GAME_RESULT_WIN:
        render_text(renderer, "You win!", white, black, WINDOW_WIDTH / 2 - 75, WINDOW_HEIGHT / 2 - 24);
        SDL_RenderPresent(renderer);
        wait_for_key_press();
        break;
    case GAME_RESULT_ABORT:
        fprintf(stderr, "Aborted");
        break;
    }

    free_maps();
    free_sound();
    free_textures();

    Mix_FreeMusic(music);

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

        if (has_no_things_to_do())
            return GAME_RESULT_WIN;

        update_objects(elapsed_time);

        render_walls(renderer);
        render_sprites(renderer);
        render_ui(renderer);

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
            if (!is_collision(new_x, new_y)) {
                player.x = new_x;
                player.y = new_y;
            }
        } else if (event->key.keysym.sym == SDLK_DOWN) {
            float new_x = player.x - cosf(player.direction) * PLAYER_MOVEMENT_SPEED;
            float new_y = player.y - sinf(player.direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_collision(new_x, new_y)) {
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

        char wall_type;
        float tex_offset;

        float raw_distance = cast_ray(ray_angle, &wall_type, &tex_offset);

        /* use a conversion table to turn wall_type into a texture for drawing */
        SDL_Texture *texture = *char_to_texture_table[wall_type];

        /* Calculate the line height while correcting for the fisheye effect */
        float corrected_distance = raw_distance * cosf(player.direction - ray_angle);
        int line_height = (int)(WINDOW_HEIGHT / corrected_distance);

        /* Save line height in a depth buffer to use in in sprite rendering */
        line_height_buffer[i] = line_height;

        /* Set the source rectangle for the texture */
        int tex_rect_x = (int)(tex_offset * (float)TEXTURE_WIDTH);
        int tex_rect_y = 0;
        SDL_Rect src_rect = {tex_rect_x, tex_rect_y, 1, TEXTURE_HEIGHT};

        /* Set the destination rectangle for the texture */
        SDL_Rect dest_rect = {i * rays_per_column, (WINDOW_HEIGHT - line_height) / 2, rays_per_column, line_height};

        /* Render the textured wall */
        SDL_RenderCopy(renderer, texture, &src_rect, &dest_rect);
    }
}

/* check if the tile at x, y is a wall */
bool is_wall(int x, int y) {
    return isdigit(map[y][x]);
}

bool is_within_bounds(int x, int y) {
    return x >= 0 && x < map_width && y >= 0 && y < map_height;
}

float cast_ray(float angle, char *wall_type, float *tex_offset) {
    Vector2 direction = {cosf(angle), sinf(angle)};
    float distance = 0.0;
    Vector2 position = {player.x, player.y};

    while (distance < MAX_DISTANCE) {
        float new_x = position.x + direction.x * RAY_STEP;
        float new_y = position.y + direction.y * RAY_STEP;

        if (is_wall_collision(new_x, new_y, wall_type, tex_offset)) {
            break;
        }

        if (is_door_collision(new_x, new_y, wall_type, tex_offset)) {
            break;
        }

        position.x = new_x;
        position.y = new_y;

        distance += RAY_STEP;
    }

    return distance;
}

bool is_collision(float x, float y) {
    char wall_type = '\0';
    float offset = 0.0f;
    return is_wall_collision(x, y, &wall_type, &offset) ||
        is_door_collision(x, y, &wall_type, &offset);
}

bool is_door(int map_x, int map_y) {
    char c = map[map_y][map_x];
    return c == '-' || c == '|';
}

bool is_door_collision(float x, float y, char *wall_type, float *tex_offset) {
    int map_x = (int)floor(x);
    int map_y = (int)floor(y);

    /* check if the tile is right */
    if (!is_door(map_x, map_y)) {
        return false;
    }

    *wall_type = map[map_y][map_x];

    Object *door_object = door_map[map_y][map_x];
    float door_width = door_object->as.door.door_width;

    /* horizontal door */
    if (map[map_y][map_x] == '-') {
        float y_diff = fabs(fabs(roundf(y) - y) - 0.5);
        float x_diff = x - floorf(x);

        if (y_diff >= 0.02f) {
            return false;
        }

        if (x_diff < door_width) {
            *tex_offset = 1 - door_width + x_diff;
            return true;
        } else {
            return false;
        }
    }

    /* if vertical door */
    if (map[map_y][map_x] == '|') {
            float x_diff = fabs(fabs(roundf(x) - x) - 0.5);
            float y_diff = y - floorf(y);

            if (x_diff >= 0.02f) {
                return false;
            }

            if (y_diff < door_width) {
                *tex_offset = 1 - door_width + y_diff;
                return true;
            }

            return false;
        }

        /* unreachable */
        return false;
}

bool is_wall_collision(float x, float y, char *wall_type, float *tex_offset) {
    int map_x = (int)floor(x);
    int map_y = (int)floor(y);
    *wall_type = '\0';

    /* if outside of bounds  - counts as wall*/
    if (!is_within_bounds(map_x, map_y)) {
        *wall_type = '1';
        return true;
    }

    if (!is_wall(map_x, map_y)) {
        return false;
    }

    *wall_type = map[map_y][map_x];

    /* check if horisontal or vertical wall, find texture offset accordingly */
    if (fabs(roundf(x) - x) >= fabs(roundf(y) - y)) {
        *tex_offset = x - floorf(x);
    } else {
        *tex_offset = y - floorf(y);
    }

    return true;
}

bool has_no_things_to_do() {
    for (int i = 0; i < num_objects; i++) {
        if (!objects[i].is_harmless) {
            return false;
        }
    }
    return true;
}

void init_poo(Object *object, int x, int y) {
    *object = (typeof(*object)) {
        .texture = poo_texture,
        .x = x + 0.5,
        .y = y + 0.5,
        .is_updateable = true,
        .is_hittable = true,
        .is_harmless = false,
        .is_visible = true,
        .is_touchable = true,
        .touch_distance = 0.5,
        .hit_distance = 0.5,
        .hit = poo_hit,
        .touch = poo_touch
    };

    enemies_left++;
}

void poo_hit(Object *object) {
    object->is_updateable = false;
    object->is_hittable = false;
    object->is_harmless = true;
    object->is_visible = false;
    object->is_touchable = false;

    enemies_left--;
}

void poo_touch(Object *object) {
    if (coins_collected)
        coins_collected--;

    poo_hit(object);

    Mix_PlayChannel(-1, pain_sound, 0);
}

void init_fly(Object *object, int x, int y) {
    *object = (typeof(*object)) {
        .texture = fly_texture,
        .x = x + 0.5,
        .y = y + 0.5,
        .is_updateable = true,
        .is_harmless = false,
        .is_visible = true,
        .is_hittable = true,
        .is_touchable = true,

        .hit_distance = 0.25,
        .touch_distance = 0.25,

        .update = fly_update,
        .touch = fly_touch,
        .hit = fly_hit
    };

    enemies_left++;
}

void fly_hit(Object *object) {
    object->is_updateable = false;
    object->is_hittable = false;
    object->is_harmless = true;
    object->is_visible = false;
    object->is_touchable = false;

    enemies_left--;
}

void fly_touch(Object *object) {
    if (coins_collected)
        coins_collected--;

    fly_hit(object);

    Mix_PlayChannel(-1, pain_sound, 0);
}

void init_projectile(Object *object) {
    *object = (typeof(*object)) {
        .texture = brush_texture,
        .is_updateable = false,
        .is_hittable = false,
        .is_visible = false,
        .is_harmless = true,
        .update = projectile_update
    };

}

void projectile_update(Object *projectile, Uint32 elapsed_time) {
    float dx = projectile->direction.x * PROJECTILE_SPEED * elapsed_time;
    float dy = projectile->direction.y * PROJECTILE_SPEED * elapsed_time;

    float new_x = projectile->x + dx;
    float new_y = projectile->y + dy;

    if (is_collision(new_x, new_y)) {
        goto remove_projectile;
    }

    /* move the project to the new position */
    projectile->x = new_x;
    projectile->y = new_y;

    /* check all other objects but skip the first one - itself */
    for (int j = 1; j < num_objects; j++) {
        if (!objects[j].is_hittable) {
            continue;
        }
        float dist_x = objects[j].x - projectile->x;
        float dist_y = objects[j].y - projectile->y;
        float distance = sqrtf(dist_x * dist_x + dist_y * dist_y);

        if (distance < objects[j].hit_distance) {
            objects[j].hit(&objects[j]);

            Mix_PlayChannel(-1, brush_sound, 0);

            goto remove_projectile;
        }
    }
    return;

remove_projectile:
    projectile->is_visible = false;
    projectile->is_updateable = false;
}

float random_float(float min, float max) {
    float scale = rand() / (float) RAND_MAX;
    return min + scale * (max - min);
}

void fly_update(Object *object, Uint32 elapsed_time) {
    /* Speed factor*/
    const float speed = 0.002f;

    /* random directions */
    float dx = random_float(-1.0f, 1.0f);
    float dy = random_float(-1.0f, 1.0f);

    /* Scale the movement by the elapsed time and speed factor */
    dx *= elapsed_time * speed;
    dy *= elapsed_time * speed;

    /* Calculate the new position */
    float new_x = object->x + dx;
    float new_y = object->y + dy;

    /* Check if the new position is within map boundaries and not a wall */
    int new_tile_x = (int)new_x;
    int new_tile_y = (int)new_y;

    if (new_tile_x >= 0 && new_tile_x < map_width &&
        new_tile_y >= 0 && new_tile_y < map_height &&
        map[new_tile_y][new_tile_x] != '1') {

        object->x = new_x;
        object->y = new_y;
    }
}

void init_flower(Object *object, int x, int y) {
    *object = (typeof(*object)) {
        .texture = flower_texture,
        .x = x + 0.5,
        .y = y + 0.5,
        .is_updateable = false,
        .is_hittable = false,
        .is_harmless = true,
        .is_visible = true,
    };
}

void init_coin(Object *object, int x, int y) {
    *object = (typeof(*object)) {
        .texture = coin_texture,
        .x = x + 0.5,
        .y = y + 0.5,
        .is_harmless = true,
        .is_visible = true,
        .is_touchable = true,
        .touch_distance = 0.5,
        .touch = touch_coin
    };
}

void touch_coin(Object *object) {
    object->is_visible = false;
    object->is_touchable = false;
    coins_collected++;

}

void update_objects(Uint32 elapsed_time) {
    /* update  */
    for (int i = 0; i < num_objects; i++) {
        if (objects[i].is_updateable && objects[i].update) {
            objects[i].update(&objects[i], elapsed_time);
        }
    }

    /* touch */
    for (int i = 0; i < num_objects; i++) {
        if (!objects[i].is_touchable) {
            continue;
        }

        float distance_to_object = sqrtf(powf(objects[i].x - player.x, 2) + powf(objects[i].y - player.y, 2));
        if (distance_to_object > objects[i].touch_distance) {
            continue;
        }

        assert(objects[i].touch);

        objects[i].touch(&objects[i]);
    }

}

void init_door(Object *object, int x, int y) {
    *object = (typeof(*object)) {
        .texture = NULL,        /* do not render */
        .x = x + 0.5,
        .y = y + 0.5,
        .hit_distance = 0.5f,
        .is_updateable = false,
        .is_hittable = true,
        .is_harmless = true,
        .is_visible = false,

        .hit = door_hit,
        .update = door_update,

        .as.door = {
            .is_opening = false,
            .door_width = 1.0f
        }
    };
}

void door_hit(Object *object) {
    if (object->as.door.door_width > 0.0f) {
        object->is_updateable = true;
        object->as.door.is_opening = true;

        Mix_PlayChannel(-1, door_sound, 0);
    }
}

void door_update(Object *object, Uint32 elapsed_time) {
    if (!object->as.door.is_opening) {
        return;
    }
    float diff = elapsed_time * 0.002f;
    object->as.door.door_width -= diff;
    if (object->as.door.door_width <= 0.0f) {
        object->as.door.is_opening = false;
        object->is_updateable = false;
        object->is_hittable = false;
        object->as.door.door_width = 0.0f;
    }
}

int compare_objects_by_distance(const void *left, const void *right) {
    return ((Object *)left)->distance_to_player < ((Object *)right)->distance_to_player ? -1 : 1;
}

Object *objects_visible[MAX_OBJECTS] = {0};

void render_sprites(SDL_Renderer *renderer) {
    /* Collect sprites that are visible and qsort them based on line height (aka
     * distance). This'll solve the sprite overlapping problem. */

    int num_objects_visible = 0;
    for (int i = 0; i < num_objects; i++) {
        if (!objects[i].is_visible) {
            continue;
        }

        /* Angle between a player space positive x-axis and sprite positiion */
        float angle = atan2f(objects[i].y - player.y, objects[i].x - player.x);

        /* Find the angle between player's direction vector and the object and
         * normalize it */
        float relative_angle = player.direction - angle;
        if (relative_angle > M_PI) {
            relative_angle -= 2 * M_PI;
        }

        /* Check if the object is in the player's field of view */
        if (relative_angle < -FOV / 2.0 || relative_angle > FOV / 2.0) {
            continue;
        }

        /* Distance and fisheye effect correction */
        float distance_to_object = sqrtf(powf(objects[i].x - player.x, 2) + powf(objects[i].y - player.y, 2));
        distance_to_object *= cosf(relative_angle);

        objects_visible[num_objects_visible] = &objects[i];
        objects_visible[num_objects_visible]->distance_to_player = distance_to_object;
        objects_visible[num_objects_visible]->angle_to_player = relative_angle;

        num_objects_visible++;
    }

    /* Now, sort the array based on distance to the player */
    qsort(objects_visible, num_objects_visible, sizeof(Object *), compare_objects_by_distance);

    /* Go through visible objects and draw them */
    for (int i = 0; i < num_objects_visible; i++) {
        Object *object = objects_visible[i];

        /* Object line height based on the distance to the player  */
        int line_height = (int)(WINDOW_HEIGHT / object->distance_to_player);

        /* Calculate the horizontal position of the enemy on the screen */
        int screen_x = (int)((WINDOW_WIDTH / 2) - tanf(object->angle_to_player) * (WINDOW_WIDTH / 2) / tanf(FOV / 2));

        /* Calculate the size of the object */
        const int object_size = (int)(line_height);

        /* Render the texture column by column */
        for (int col = 0; col < object_size; col++) {
            /* find the current object column to check and make sure it doesn't
             * go above the number of rays casted - this might cause a crash
             * when standing right next to the objects */
            int screen_col = screen_x - object_size / 2 + col;
            if (screen_col < 0 || screen_col >= RAY_COUNT)
                continue;

            /* see if the wall column for this ray is further away than object
             * column. Ignore otherise.*/
            int wall_line_height = line_height_buffer[screen_col];
            if (line_height < wall_line_height)
                continue;

            /* Calculate the source and destination rectangles for the
             * current column. Note that ceilf here is necessary to avoid
             * zero-width rectangles */
            SDL_Rect src_rect = {ceilf(col * (float)TEXTURE_WIDTH / object_size), 0, ceilf((float)TEXTURE_WIDTH / object_size), TEXTURE_HEIGHT};
            SDL_Rect dest_rect = {(screen_x - object_size / 2) + col, (WINDOW_HEIGHT - object_size) / 2, 1, object_size};

            /* Render the current column of the texture */
            SDL_RenderCopyEx(renderer, object->texture, &src_rect, &dest_rect, 0, NULL, SDL_FLIP_NONE);
        }
    }
}

void render_ui(SDL_Renderer *renderer) {
    // Convert the coins_collected to a string
    char coin_str[50];
    snprintf(coin_str, sizeof(coin_str), "Coins: %d", coins_collected);

    // Create a surface from the font and string
    SDL_Color font_color = {0, 0, 0, 255}; // White text
    SDL_Surface *text_surface = TTF_RenderText_Solid(font, coin_str, font_color);

    // Create a texture from the surface
    SDL_Texture *text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

    // Set the destination rectangle for the texture
    SDL_Rect dest_rect = {WINDOW_WIDTH - text_surface->w - 10, 10, text_surface->w, text_surface->h};

    // Render the text texture
    SDL_RenderCopy(renderer, text_texture, NULL, &dest_rect);

    // Clean up
    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}

void fire_projectile(void) {
    Object *projectile = &objects[0];
    if (projectile->is_visible)
        return;
    projectile->direction = (Vector2){cosf(player.direction), sinf(player.direction)};
    projectile->x = player.x;
    projectile->y = player.y;
    projectile->is_visible = true;
    projectile->is_updateable = true;
}

void load_maps(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening map file: %s\n", filename);
        exit(1);
    }

    fscanf(file, "%d %d\n", &map_width, &map_height);

    map = malloc(map_height * sizeof(map[0]));
    door_map = calloc(map_height * sizeof(door_map[0]), 1);

    /* the first object is always the projectile */
    init_projectile(&objects[0]);
    num_objects++;

    /* need to make sure the player was there */
    bool player_start_found = false;

    /* walk through all map cells, load the map and all the objects*/
    fprintf(stderr, "File: %s\n", filename);
    fprintf(stderr, "Dimensions: %d x %d\n", map_width, map_height);

    for (int y = 0; y < map_height; y++) {
        /* size = width + newline + null */
        map[y] = malloc(map_width * sizeof(map[y][0]) + 2);
        door_map[y] = malloc(map_width * sizeof(door_map[y][0]));
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
                if (num_objects >= MAX_OBJECTS) {
                    goto too_many_objects;
                }
                init_poo(&objects[num_objects], x, y);
                num_objects++;
                map[y][x] = ' ';
                break;
            case 'f':
                if (num_objects >= MAX_OBJECTS) {
                    goto too_many_objects;
                }
                init_fly(&objects[num_objects], x, y);
                num_objects++;
                map[y][x] = ' ';
                break;
            case 'c':
                if (num_objects >= MAX_OBJECTS) {
                    goto too_many_objects;
                }
                init_coin(&objects[num_objects], x, y);
                num_objects++;
                map[y][x] = ' ';
                break;
            case '*':
                if (num_objects >= MAX_OBJECTS) {
                    goto too_many_objects;
                }
                init_flower(&objects[num_objects], x, y);
                num_objects++;
                map[y][x] = ' ';
                break;
            case '-':
            case '|':
                init_door(&objects[num_objects], x, y);
                door_map[y][x] = &objects[num_objects];
                num_objects++;
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

too_many_objects:
    fprintf(stderr, "No starting position found in the map file: %s\n", filename);
    exit(1);
}

void free_maps(void) {
    for (int y = 0; y < map_height; y++) {
        free(map[y]);
        free(door_map[y]);
    }
    free(map);
    free(door_map);
}

void render_text(SDL_Renderer *renderer, const char *message, SDL_Color color, SDL_Color outline_color, int x, int y) {
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

void load_textures(SDL_Renderer *renderer) {
    /* iterate over the name_to_texture_table and load surfaces/textures */
    for (int i = 0; i < sizeof(name_to_texture_table) / sizeof(name_to_texture_table[0]); i++) {
        SDL_Surface *surface = IMG_Load(name_to_texture_table[i].name);
        if (surface == NULL) {
            fprintf(stderr, "Failed to load a surface: %s\n", IMG_GetError());
            exit(1);
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == NULL) {
            fprintf(stderr, "Failed to load a texture: %s\n", IMG_GetError());
            exit(1);
        }

        *name_to_texture_table[i].texture = texture;

        SDL_FreeSurface(surface);
    }
}

void free_textures(void) {
    /* iterate over the name_to_texture_table and destroy textures */
    for (int i = 0; i < sizeof(name_to_texture_table) / sizeof(name_to_texture_table[0]); i++) {
        SDL_DestroyTexture(*name_to_texture_table[i].texture);
    }
}

void load_sound(void) {
    door_sound = Mix_LoadWAV("door.wav");
    if (door_sound == NULL) {
        printf("Failed to load sound: %s\n", Mix_GetError());
        exit(1);
    }

    pain_sound = Mix_LoadWAV("pain.wav");
    if (pain_sound == NULL) {
        printf("Failed to load sound: %s\n", Mix_GetError());
        exit(1);
    }

    brush_sound = Mix_LoadWAV("brush.wav");
    if (brush_sound == NULL) {
        printf("Failed to load sound: %s\n", Mix_GetError());
        exit(1);
    }
}

void free_sound(void) {
    Mix_FreeChunk(door_sound);
}
