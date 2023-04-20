#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

/* The simplest game skeleton.

   Compile with something like:
   gcc -o vlk3d vlk3d.c (sdl2-config --cflags --libs) -lm -lSDL2_image; and ./vlk3d
*/


/* Constants */

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

#define FOV M_PI / 3.0
#define RAY_COUNT (WINDOW_WIDTH)
#define MAX_DISTANCE 20.0

/* Types/typedefs */

typedef struct {
    float x;
    float y;
} Vector2;

typedef struct {
    float x;
    float y;
    float direction;
} Player;


#define PLAYER_ROTATION_SPEED 0.1
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


SDL_Texture *wall_texture = NULL;

/* Game state */

Projectile *projectiles = NULL;

int **map;
int map_width;
int map_height;

Player player = {0, 0, 0};

#define ENEMY_PROXIMITY_DISTANCE 0.5

typedef struct {
    float x;
    float y;
} Enemy;

#define MAX_ENEMIES 50
Enemy enemies[MAX_ENEMIES];
int num_enemies = 0;


/* Function prototypes */

void game_loop(SDL_Window *window, SDL_Renderer *renderer);
void handle_events(SDL_Event *event, bool *is_running);
void render(SDL_Renderer *renderer);
float cast_ray(float angle);
bool is_wall_collision(float x, float y);
bool is_close_to_enemy(float x, float y);
bool is_line_of_sight_blocked(Vector2 start, Vector2 end);

void update_projectiles();
void render_projectiles(SDL_Renderer *renderer);
void render_enemies(SDL_Renderer *renderer);
void fire_projectile();
void free_projectiles();
void free_map();

void load_map(const char *filename);

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Wolf3D-like Game",
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

    // Load the wall texture
    SDL_Surface *wall_surface = IMG_Load("wall.bmp");
    if (wall_surface == NULL) {
        fprintf(stderr, "Failed to load wall texture: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    wall_texture = SDL_CreateTextureFromSurface(renderer, wall_surface);
    SDL_FreeSurface(wall_surface);

    if (wall_texture == NULL) {
        fprintf(stderr, "Failed to create wall texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    load_map("map.txt");
    game_loop(window, renderer);

    free_projectiles();
    free_map();

    SDL_DestroyTexture(wall_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void game_loop(SDL_Window *window, SDL_Renderer *renderer) {
    bool is_running = true;
    SDL_Event event;

    while (is_running) {
        while (SDL_PollEvent(&event)) {
            handle_events(&event, &is_running);
        }

        if (is_close_to_enemy(player.x, player.y)) {
            is_running = false;
        } else {
            update_projectiles();
            render(renderer);
            render_projectiles(renderer);
            render_enemies(renderer);

            SDL_RenderPresent(renderer);
            SDL_Delay(16);
        }
    }
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

    for (int i = 0; i < RAY_COUNT; i++) {
        float angle = player.direction - FOV / 2.0 + FOV * i / (float)RAY_COUNT;
        float raw_distance = cast_ray(angle);
        float corrected_distance = raw_distance * cosf(player.direction - angle); // Correct the fishbowl effect
        int line_height = (int)(WINDOW_HEIGHT / corrected_distance);

        // Calculate the direction vector
        Vector2 direction = {cosf(angle), sinf(angle)};

        // Calculate the texture coordinate
        Vector2 hit_position = {player.x + direction.x * raw_distance, player.y + direction.y * raw_distance};
        float tex_x;
        if (fabs(direction.x) > fabs(direction.y)) {
            tex_x = fmod(hit_position.y, 1);
        } else {
            tex_x = fmod(hit_position.x, 1);
        }
        if (tex_x < 0) tex_x += 1;

        // Set the source rectangle for the texture
        int texture_w, texture_h;
        SDL_QueryTexture(wall_texture, NULL, NULL, &texture_w, &texture_h);
        SDL_Rect src_rect = {(int)(tex_x * texture_w), 0, 1, texture_h};

        // Set the destination rectangle for the texture
        SDL_Rect dest_rect = {i * (WINDOW_WIDTH / RAY_COUNT), (WINDOW_HEIGHT - line_height) / 2, (WINDOW_WIDTH / RAY_COUNT), line_height};

        // Render the textured wall
        SDL_RenderCopy(renderer, wall_texture, &src_rect, &dest_rect);
    }
}

float cast_ray(float angle) {
    Vector2 direction = {cosf(angle), sinf(angle)};
    float distance = 0.0;
    Vector2 position = {player.x, player.y};

    while (distance < MAX_DISTANCE) {
        position.x += direction.x * 0.1;
        position.y += direction.y * 0.1;
        distance += 0.1;

        int mapX = (int)floor(position.x);
        int mapY = (int)floor(position.y);

        if (mapX >= 0 && mapX < map_width && mapY >= 0 && mapY < map_height && map[mapY][mapX] == 1) {
            break;
        }
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

bool is_close_to_enemy(float x, float y) {
    for (int i = 0; i < num_enemies; i++) {
        float distance = sqrtf(powf(x - enemies[i].x, 2) + powf(y - enemies[i].y, 2));
        if (distance < ENEMY_PROXIMITY_DISTANCE) {
            return true;
        }
    }
    return false;
}

bool is_line_of_sight_blocked(Vector2 start, Vector2 end) {
    Vector2 direction = {end.x - start.x, end.y - start.y};
    float distance = sqrtf(direction.x * direction.x + direction.y * direction.y);
    direction.x /= distance;
    direction.y /= distance;

    float current_distance = 0.0;
    Vector2 position = {start.x, start.y};

    while (current_distance < distance) {
        position.x += direction.x * 0.1;
        position.y += direction.y * 0.1;
        current_distance += 0.1;

        int mapX = (int)floor(position.x);
        int mapY = (int)floor(position.y);

        if (mapX >= 0 && mapX < map_width && mapY >= 0 && mapY < map_height && map[mapY][mapX] == 1) {
            return true;
        }
    }

    return false;
}


void update_projectiles() {
    Projectile *current = projectiles;
    Projectile *prev = NULL;

    while (current != NULL) {
        current->position.x += current->direction.x * PROJECTILE_SPEED;
        current->position.y += current->direction.y * PROJECTILE_SPEED;

        if (is_wall_collision(current->position.x, current->position.y)) {
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

void render_enemies(SDL_Renderer *renderer) {
    for (int i = 0; i < num_enemies; i++) {
        if (is_line_of_sight_blocked((Vector2){player.x, player.y}, (Vector2){enemies[i].x, enemies[i].y})) {
            continue;
        }


        float angle = atan2f(enemies[i].y - player.y, enemies[i].x - player.x);
        if (angle < 0) {
            angle += 2 * M_PI;
        }
        float distance_to_enemy = sqrtf(powf(enemies[i].x - player.x, 2) + powf(enemies[i].y - player.y, 2));
        float relative_angle = player.direction - angle;

        // Check if the enemy is in the player's field of view
        if (relative_angle > -FOV / 2.0 && relative_angle < FOV / 2.0) {
            int line_height = (int)(WINDOW_HEIGHT / distance_to_enemy);

            // Calculate the horizontal position of the enemy on the screen
            int screen_x = (int)((WINDOW_WIDTH / 2) - tanf(relative_angle) * (WINDOW_WIDTH / 2) / tanf(FOV / 2));

            // Calculate the size of the enemy square, taking perspective into account
            int square_size = (int)(line_height * 0.5);

            // Set the color and render the enemy as a big brown square
            SDL_SetRenderDrawColor(renderer, 139, 69, 19, 255); // Brown color
            SDL_Rect square = {screen_x - square_size / 2, (WINDOW_HEIGHT - line_height) / 2 + (line_height - square_size) / 2, square_size, square_size};
            SDL_RenderFillRect(renderer, &square);
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
                if (num_enemies < MAX_ENEMIES) {
                    enemies[num_enemies].x = x + 0.5;
                    enemies[num_enemies].y = y + 0.5;
                    num_enemies++;
                }
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
