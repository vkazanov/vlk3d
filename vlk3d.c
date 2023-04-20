#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <SDL2/SDL.h>

/* The simplest game skeleton.

   Compile with something like:
   gcc -o vlk3d vlk3d.c (sdl2-config --cflags --libs) -lm; and ./vlk3d
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

#define PROJECTILE_SPEED 0.3
#define PROJECTILE_COLOR_R 255
#define PROJECTILE_COLOR_G 0
#define PROJECTILE_COLOR_B 0

Projectile *projectiles = NULL;

/* Game state */

int map[6][6] = {
    {1, 1, 1, 1, 1, 1},
    {1, 1, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 1, 1},
    {1, 1, 1, 1, 1, 1}
};

Player player = {2.0, 2.0, 0.0};

/* Function prototypes */

void game_loop(SDL_Window *window, SDL_Renderer *renderer);
void handle_events(SDL_Event *event, bool *is_running, Player *player);
void render(SDL_Renderer *renderer, Player *player);
float cast_ray(Player *player, float angle);
bool is_wall_collision(float x, float y);

void update_projectiles();
void render_projectiles(SDL_Renderer *renderer);
void fire_projectile(Player *player);
void free_projectiles();

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

    game_loop(window, renderer);

    free_projectiles();

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
            handle_events(&event, &is_running, &player);
        }

        update_projectiles();

        render(renderer, &player);
        render_projectiles(renderer);

        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }
}

void handle_events(SDL_Event *event, bool *is_running, Player *player) {
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
            float newX = player->x + cosf(player->direction) * PLAYER_MOVEMENT_SPEED;
            float newY = player->y + sinf(player->direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_wall_collision(newX, newY)) {
                player->x = newX;
                player->y = newY;
            }
        } else if (event->key.keysym.sym == SDLK_DOWN) {
            float newX = player->x - cosf(player->direction) * PLAYER_MOVEMENT_SPEED;
            float newY = player->y - sinf(player->direction) * PLAYER_MOVEMENT_SPEED;
            if (!is_wall_collision(newX, newY)) {
                player->x = newX;
                player->y = newY;
            }
        } else if (event->key.keysym.sym == SDLK_LEFT) {
            player->direction -= PLAYER_ROTATION_SPEED;
        } else if (event->key.keysym.sym == SDLK_RIGHT) {
            player->direction += PLAYER_ROTATION_SPEED;
        }
        break;
        /* Handle other event types here */
    default:
        break;
    }
}

void render(SDL_Renderer *renderer, Player *player) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < RAY_COUNT; i++) {
        float angle = player->direction - FOV / 2.0 + FOV * i / (float)RAY_COUNT;
        float raw_distance = cast_ray(player, angle);
        float corrected_distance = raw_distance * cosf(player->direction - angle); // Correct the fishbowl effect
        int lineHeight = (int)(WINDOW_HEIGHT / corrected_distance);

        int color = (int)(255 * (1 - corrected_distance / MAX_DISTANCE));
        color = color > 255 ? 255 : (color < 0 ? 0 : color);

        SDL_SetRenderDrawColor(renderer, color, color, color, 255);
        SDL_RenderDrawLine(renderer, i * (WINDOW_WIDTH / RAY_COUNT), (WINDOW_HEIGHT - lineHeight) / 2, i * (WINDOW_WIDTH / RAY_COUNT), (WINDOW_HEIGHT + lineHeight) / 2);
    }

    SDL_RenderPresent(renderer);
}

float cast_ray(Player *player, float angle) {
    Vector2 direction = {cosf(angle), sinf(angle)};
    float distance = 0.0;
    Vector2 position = {player->x, player->y};

    while (distance < MAX_DISTANCE) {
        position.x += direction.x * 0.1;
        position.y += direction.y * 0.1;
        distance += 0.1;

        int mapX = (int)floor(position.x);
        int mapY = (int)floor(position.y);

        if (mapX >= 0 && mapX < 6 && mapY >= 0 && mapY < 6 && map[mapY][mapX] == 1) {
            break;
        }
    }

    return distance;
}

bool is_wall_collision(float x, float y) {
    int mapX = (int)floor(x);
    int mapY = (int)floor(y);

    if (mapX >= 0 && mapX < 6 && mapY >= 0 && mapY < 6 && map[mapY][mapX] == 1) {
        return true;
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
        float distance_to_player = sqrtf(powf(current->position.x - player.x, 2) + powf(current->position.y - player.y, 2));
        float relative_angle = player.direction - angle;

        // Check if the projectile is in the player's field of view
        if (relative_angle > -FOV / 2.0 && relative_angle < FOV / 2.0) {
            float distance = cast_ray(&player, angle);
            int lineHeight = (int)(WINDOW_HEIGHT / distance);
            int screenWidth = (int)(WINDOW_WIDTH / (float)RAY_COUNT);

            // Calculate the horizontal position of the projectile on the screen
            int screenX = (int)((WINDOW_WIDTH / 2) + tanf(relative_angle) * distance_to_player * screenWidth);

            // Set the color and render the projectile as a vertical line
            SDL_SetRenderDrawColor(renderer, PROJECTILE_COLOR_R, PROJECTILE_COLOR_G, PROJECTILE_COLOR_B, 255);
            SDL_RenderDrawLine(renderer, screenX, (WINDOW_HEIGHT - lineHeight) / 2, screenX, (WINDOW_HEIGHT + lineHeight) / 2);
        }

        current = current->next;
    }
}

void fire_projectile(Player *player) {
    Projectile *new_projectile = (Projectile *)malloc(sizeof(Projectile));
    new_projectile->position = (Vector2){player->x, player->y};
    new_projectile->direction = (Vector2){cosf(player->direction), sinf(player->direction)};
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
