#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>



// aliases
ALLEGRO_DISPLAY* disp;
ALLEGRO_BITMAP* buffer;

// constants
#define BUFFER_W 320
#define BUFFER_H 240

#define DISP_SCALE 3
#define DISP_W (BUFFER_W * DISP_SCALE)
#define DISP_H (BUFFER_H * DISP_SCALE)

#define HIT_W 2
#define HIT_H 2

#define SHIP_W 20
#define SHIP_H 36

#define ICON_W 6
#define ICON_H 6

#define GRAZE_R 21

#define ALIEN_BLOB_W        ALIEN_W[0]
#define ALIEN_BLOB_H        ALIEN_H[0]
#define ALIEN_YELLOW_W      ALIEN_W[1]
#define ALIEN_YELLOW_H      ALIEN_H[1]
#define ALIEN_PURPLE_W      ALIEN_W[2]
#define ALIEN_PURPLE_H      ALIEN_H[2]
#define ALIEN_GREEN_W       ALIEN_W[3]
#define ALIEN_GREEN_H       ALIEN_H[3]

#define EXPLOSION_FRAMES 5
#define SPLASH_FRAMES    2

#define SHIP_SPEED 3
#define FOCUS_SPEED 1
#define SHIP_MAX_X (BUFFER_W - SHIP_W)
#define SHIP_MAX_Y (BUFFER_H - SHIP_H)
#define MAX_HAIR_LEN 30

// globals
long frames;
long score;
float master_volume = 1;
float sfx_volume = 1;
float music_volume = 1;
int hair_len = 15;
const int ALIEN_W[] = {15, 19, 18, 25};
const int ALIEN_H[] = {15, 23, 31, 33};
const int ALIEN_SHOT_W[] = {5, 9, 5, 28};
const int ALIEN_SHOT_H[] = {5, 9, 20, 8};
const int SHIP_SHOT_W[] = {3, 5, 7};
const int SHIP_SHOT_H[] = {11, 5, 7};
int hitstop_timer = 0;
bool is_paused = 0;

// structures
typedef struct SPRITES
{
    ALLEGRO_BITMAP* _sheet;

    ALLEGRO_BITMAP* ship;
    ALLEGRO_BITMAP* ship_shot[3];
    ALLEGRO_BITMAP* life;

    ALLEGRO_BITMAP* alien[4];
    ALLEGRO_BITMAP* alien_shot[4];

    ALLEGRO_BITMAP* explosion[EXPLOSION_FRAMES];
    ALLEGRO_BITMAP* splash[SPLASH_FRAMES];

    ALLEGRO_BITMAP* hitbox;
    ALLEGRO_BITMAP* hair;

    ALLEGRO_BITMAP* bomb;

} SPRITES;
SPRITES sprites;

typedef struct FX
{
    int x, y;
    int frame;
    bool splash;
    bool used;
} FX;
#define FX_N 1024
FX fx[FX_N];

typedef struct SHOT
{
    int x, y, dx, dy;
    int frame;
    int type; // for ship 0 is bolt 1 is spark, for aliens 0 is ball, 1 is ring, 2 is long and 3 is wide
    bool ship;
    bool grazed;
    bool used;
} SHOT;
#define SHOTS_N 1024
SHOT shots[SHOTS_N];

typedef struct SHIP
{
    int x, y;
    int shot_timer;
    int lives;
    int bombs;
    int respawn_timer;
    int invincible_timer;
    int frame;
} SHIP;
SHIP ship;

typedef enum ALIEN_TYPE
{
    ALIEN_TYPE_BLOB = 0,
    ALIEN_TYPE_YELLOW,
    ALIEN_TYPE_PURPLE,
    ALIEN_TYPE_GREEN,
    ALIEN_TYPE_N
} ALIEN_TYPE;

typedef struct ALIEN
{
    int x, y;
    ALIEN_TYPE type;
    int shot_timer;
    int blink;
    int life;
    bool used;
} ALIEN;
#define ALIENS_N 16
ALIEN aliens[ALIENS_N];

typedef struct HAIR_STRUCT
// data structure for the hair physics
{
    int x, y;
    int last_x;
    int anim_state;
} HAIR_STRUCT;
HAIR_STRUCT hair[MAX_HAIR_LEN];

typedef struct STAR
{
    float y;
    float speed;
} STAR;
#define STARS_N ((BUFFER_W / 2) - 1)
STAR stars[STARS_N];


void must_init(bool test, const char *description)
{
    if(test) return;

    printf("couldn't initialize %s\n", description);
    exit(1);
}

int between(int lo, int hi)
{
    return lo + (rand() % (hi - lo));
}

float between_f(float lo, float hi)
{
    return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo);
}

bool rect_collide(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2)
{
    if(ax1 > bx2) return false;
    if(ax2 < bx1) return false;
    if(ay1 > by2) return false;
    if(ay2 < by1) return false;

    return true;
}

bool circle_collide(int ax, int ay, int ar, int bx, int by, int br)
// stolen from the internet because i want it to be optimal and am too lazy to reinvent the wheel
{
    if(((ax - bx)^2) + ((ay - by)^2) <= ((ar + br)^2)) return true;
    return false;
}

bool circle_rect_collide(int ax, int ay, int ar, int bx, int by, int bw, int bh)
// stolen from the internet because there is no way in hell i would have come up with something this efficient
{
    bx = (bx + (bw/2));
    by = (by + (bh/2));

    int xdiff = abs(ax - bx);
    int ydiff = abs(ay - by);

    if (xdiff > ((bw/2)+ar)) return false;
    if (ydiff > ((bh/2)+ar)) return false;

    if (xdiff <= (bw/2)) return true;
    if (ydiff <= (bh/2)) return true;

    if (((xdiff - (bw/2))^2) + ((ydiff - (bh/2))^2) <= (ar^2)) return true;
    return false;
}

float get_volume(bool is_music, float gain)
// returns sound volume adjusted for global volume settings
{
    if (is_music) return (gain * music_volume * master_volume);
    else return (gain * sfx_volume * master_volume);
}

float get_pan(float x)
// returns proper panning for a sound given it's x coordinate
{
    return ((x / (float)(BUFFER_W / 2)) - 1.0);
}

void disp_init()
{
    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_SAMPLES, 8, ALLEGRO_SUGGEST);

    disp = al_create_display(DISP_W, DISP_H);
    must_init(disp, "display");

    buffer = al_create_bitmap(BUFFER_W, BUFFER_H);
    must_init(buffer, "bitmap buffer");
}

void disp_deinit()
{
    al_destroy_bitmap(buffer);
    al_destroy_display(disp);
}

void disp_pre_draw()
{
    al_set_target_bitmap(buffer);
}

void disp_post_draw()
{
    al_set_target_backbuffer(disp);
    al_draw_scaled_bitmap(buffer, 0, 0, BUFFER_W, BUFFER_H, 0, 0, DISP_W, DISP_H, 0);

    al_flip_display();
}

#define KEY_SEEN     1
#define KEY_RELEASED 2
unsigned char key[ALLEGRO_KEY_MAX];
unsigned char keydown[ALLEGRO_KEY_MAX];
unsigned char keyup[ALLEGRO_KEY_MAX];

void keyboard_init()
{
    memset(key, 0, sizeof(key));
}

void keyboard_update(ALLEGRO_EVENT* event)
{
    switch(event->type)
    {
        case ALLEGRO_EVENT_TIMER:
            for(int i = 0; i < ALLEGRO_KEY_MAX; i++)
            {
                key[i] &= KEY_SEEN;
                keydown[i] &= KEY_SEEN;
                keyup[i] &= KEY_SEEN;
            }
            break;

        case ALLEGRO_EVENT_KEY_DOWN:
            key[event->keyboard.keycode] = KEY_SEEN | KEY_RELEASED;
            keydown[event->keyboard.keycode] = KEY_RELEASED;
            break;
        case ALLEGRO_EVENT_KEY_UP:
            key[event->keyboard.keycode] &= KEY_RELEASED;
            keyup[event->keyboard.keycode] = KEY_RELEASED;
            break;
    }
}

ALLEGRO_BITMAP* sprite_grab(int x, int y, int w, int h)
{
    ALLEGRO_BITMAP* sprite = al_create_sub_bitmap(sprites._sheet, x, y, w, h);
    must_init(sprite, "sprite grab");
    return sprite;
}

void sprites_init()
{
    sprites._sheet = al_load_bitmap("spritesheet.png");
    must_init(sprites._sheet, "spritesheet");

    sprites.ship = sprite_grab(0, 0, SHIP_W, SHIP_H);
    sprites.hitbox = sprite_grab(0, 36, 8, 8);

    sprites.bomb = sprite_grab(8, 36, ICON_W, ICON_H);
    sprites.life = sprite_grab(14, 36, ICON_W, ICON_H);

    sprites.ship_shot[0] = sprite_grab(20, 0, SHIP_SHOT_W[0], SHIP_SHOT_H[0]);
    sprites.ship_shot[1] = sprite_grab(23, 0, SHIP_SHOT_W[1], SHIP_SHOT_H[1]);
    sprites.ship_shot[2] = sprite_grab(20, 11, SHIP_SHOT_W[2], SHIP_SHOT_H[2]);

    sprites.alien[0] = sprite_grab(19, 44, ALIEN_BLOB_W, ALIEN_BLOB_H);
    sprites.alien[1] = sprite_grab(0, 44, ALIEN_YELLOW_W, ALIEN_YELLOW_H);
    sprites.alien[2] = sprite_grab(0, 67, ALIEN_PURPLE_W, ALIEN_PURPLE_H);
    sprites.alien[3] = sprite_grab(0, 98, ALIEN_GREEN_W, ALIEN_GREEN_H);

    sprites.alien_shot[0] = sprite_grab(18, 70, ALIEN_SHOT_W[0], ALIEN_SHOT_H[0]);
    sprites.alien_shot[1] = sprite_grab(23, 70, ALIEN_SHOT_W[1], ALIEN_SHOT_H[1]);
    sprites.alien_shot[2] = sprite_grab(18, 75, ALIEN_SHOT_W[2], ALIEN_SHOT_H[2]);
    sprites.alien_shot[3] = sprite_grab(23, 79, ALIEN_SHOT_W[3], ALIEN_SHOT_H[3]);

    sprites.explosion[0] = sprite_grab(28, 0, 12, 11);
    sprites.explosion[1] = sprite_grab(27, 11, 13, 12);
    sprites.explosion[2] = sprite_grab(20, 23, 15, 14);
    sprites.explosion[3] = sprite_grab(40, 0, 26, 24);
    sprites.explosion[4] = sprite_grab(66, 0, 30, 28);

    sprites.splash[0] = sprite_grab(35, 24, 7, 8);
    sprites.splash[1] = sprite_grab(42, 24, 11, 10);

    sprites.hair = sprite_grab(8, 7, 4, 1);
}

void sprites_deinit()
{
    al_destroy_bitmap(sprites.ship);
    al_destroy_bitmap(sprites.hitbox);

    al_destroy_bitmap(sprites.bomb);
    al_destroy_bitmap(sprites.life);

    al_destroy_bitmap(sprites.ship_shot[0]);
    al_destroy_bitmap(sprites.ship_shot[1]);
    al_destroy_bitmap(sprites.ship_shot[2]);

    al_destroy_bitmap(sprites.alien[0]);
    al_destroy_bitmap(sprites.alien[1]);
    al_destroy_bitmap(sprites.alien[2]);
    al_destroy_bitmap(sprites.alien[3]);

    al_destroy_bitmap(sprites.alien_shot[0]);
    al_destroy_bitmap(sprites.alien_shot[1]);
    al_destroy_bitmap(sprites.alien_shot[2]);
    al_destroy_bitmap(sprites.alien_shot[3]);

    al_destroy_bitmap(sprites.explosion[0]);
    al_destroy_bitmap(sprites.explosion[1]);
    al_destroy_bitmap(sprites.explosion[2]);
    al_destroy_bitmap(sprites.explosion[3]);
    al_destroy_bitmap(sprites.explosion[4]);

    al_destroy_bitmap(sprites.splash[0]);
    al_destroy_bitmap(sprites.splash[1]);

    al_destroy_bitmap(sprites.hair);

    al_destroy_bitmap(sprites._sheet);
}

ALLEGRO_SAMPLE* sample_shot;
ALLEGRO_SAMPLE* sample_explode[2];
ALLEGRO_SAMPLE* sample_hitmarker;
ALLEGRO_SAMPLE* sample_death;

void audio_init()
{
    al_install_audio();
    al_init_acodec_addon();
    al_reserve_samples(256);

    sample_shot = al_load_sample("shot.flac");
    must_init(sample_shot, "shot sample");

    sample_explode[0] = al_load_sample("explode1.flac");
    must_init(sample_explode[0], "explode[0] sample");
    sample_explode[1] = al_load_sample("explode2.flac");
    must_init(sample_explode[1], "explode[1] sample");

    sample_hitmarker = al_load_sample("hitmarker.flac");
    must_init(sample_hitmarker, "hitmarker sound");

    sample_death = al_load_sample("death.flac");
    must_init(sample_hitmarker, "death sound");

}

void audio_deinit()
{
    al_destroy_sample(sample_shot);
    al_destroy_sample(sample_explode[0]);
    al_destroy_sample(sample_explode[1]);
    al_destroy_sample(sample_hitmarker);
    al_destroy_sample(sample_death);
}

void fx_init()
{
    for(int i = 0; i < FX_N; i++)
        fx[i].used = false;
}

void fx_add(bool splash, int x, int y)
{
    if(!splash)
        al_play_sample(
            sample_explode[between(0, 2)],
            get_volume(false, .50),
            get_pan(x),
            1,
            ALLEGRO_PLAYMODE_ONCE,
            NULL
            );

    for(int i = 0; i < FX_N; i++)
    {
        if(fx[i].used)
            continue;

        fx[i].x = x;
        fx[i].y = y;
        fx[i].frame = 0;
        fx[i].splash = splash;
        fx[i].used = true;
        return;
    }
}

void fx_update()
{
    for(int i = 0; i < FX_N; i++)
    {
        if(!fx[i].used)
            continue;

        fx[i].frame++;

        if((!fx[i].splash && (fx[i].frame == (EXPLOSION_FRAMES * 2)))
        || ( fx[i].splash && (fx[i].frame == (SPLASH_FRAMES * 2)))
        )
            fx[i].used = false;
    }
}

void fx_draw()
{
    for(int i = 0; i < FX_N; i++)
    {
        if(!fx[i].used)
            continue;

        int frame_display = fx[i].frame / 2;
        ALLEGRO_BITMAP* bmp =
            fx[i].splash
            ? sprites.splash[frame_display]
            : sprites.explosion[frame_display]
        ;

        int x = fx[i].x - (al_get_bitmap_width(bmp) / 2);
        int y = fx[i].y - (al_get_bitmap_height(bmp) / 2);
        al_draw_bitmap(bmp, x, y, 0);
    }
}

void do_bomb()
// what happens when the player bombs
{
    for (int i = 0; i < ALIENS_N; i++)
    {
        aliens[i].life = 0;
    }
    for (int i = 0; i < SHOTS_N; i++)
    {
        shots[i].used = false;
    }
    printf("Bombing\n");
}

void do_deathbomb()
// a weaker bomb that activates if you bomb during hitstop
{
    hitstop_timer = 0;
    for (int i = 0; i < ALIENS_N; i++)
    {
        aliens[i].used = false;
    }
    for (int i = 0; i < SHOTS_N; i++)
    {
        shots[i].used = false;
    }
}

void player_dies()
// what happens when the player dies
{
    ship.lives--;
    ship.respawn_timer = 180;
    ship.invincible_timer = ship.respawn_timer + 90;
    ship.frame = 0;
    hair_len = 15;
    if (ship.bombs < 3)
        ship.bombs = 3;
    al_play_sample(
        sample_death,
        get_volume(false, 0.5),
        get_pan(ship.x),
        1,
        ALLEGRO_PLAYMODE_ONCE,
        NULL
    );
}

void shots_init()
{
    for(int i = 0; i < SHOTS_N; i++)
        shots[i].used = false;
}

bool shots_add(bool ship, int type, int dx, int dy, int x, int y)
{
    al_play_sample(
        sample_shot,
        get_volume(false, 0.3),
        get_pan(x),
        ship ? 1.0 : between_f(1.5, 1.6),
        ALLEGRO_PLAYMODE_ONCE,
        NULL
    );

    for(int i = 0; i < SHOTS_N; i++)
    {
        if(shots[i].used)
            continue;

        shots[i].ship = ship;
        shots[i].type = type;

        if(ship)
        {
            shots[i].x = x - (SHIP_SHOT_W[shots[i].type] / 2);
            shots[i].y = y;
        }
        else // alien
        {
            shots[i].x = x - (ALIEN_SHOT_W[shots[i].type] / 2);
            shots[i].y = y - (ALIEN_SHOT_H[shots[i].type] / 2);
        }

        shots[i].frame = 0;
        shots[i].used = true;
        shots[i].dx = dx;
        shots[i].dy = dy;

        return true;
    }
    printf("Couldn't generate shot\n");
    return false;
}

void shots_update()
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        if(shots[i].ship)
        {
            if(shots[i].y < -SHIP_SHOT_H[shots[i].type])
            {
                shots[i].used = false;
                continue;
            }
        }
        else // alien
        {

            if((shots[i].x < -ALIEN_SHOT_W[shots[i].type])
            || (shots[i].x > BUFFER_W)
            || (shots[i].y < -ALIEN_SHOT_H[shots[i].type])
            || (shots[i].y > BUFFER_H)
            ) {
                shots[i].used = false;
                continue;
            }
        }

        shots[i].x += shots[i].dx;
        shots[i].y += shots[i].dy;

        shots[i].frame++;
    }
}

bool shots_collide(bool ship, int x, int y, int w, int h)
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        // don't collide with one's own shots
        if(shots[i].ship == ship)
            continue;

        int sw, sh;
        if(ship)
        {
            sw = ALIEN_SHOT_W[shots[i].type];
            sh = ALIEN_SHOT_H[shots[i].type];
        }
        else
        {
            sw = SHIP_SHOT_W[shots[i].type];
            sh = SHIP_SHOT_H[shots[i].type];
        }

        if(rect_collide(x, y, x+w, y+h, shots[i].x, shots[i].y, shots[i].x+sw, shots[i].y+sh))
        {
            fx_add(true, shots[i].x + (sw / 2), shots[i].y + (sh / 2));
            shots[i].used = false;
            return true;
        }
    }

    return false;
}

bool shots_graze(bool ship, int x, int y, int r)
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        // you don't get to graze your own bullets
        if(shots[i].ship == ship)
            continue;
        
        // you only get to graze a bullet once
        if(shots[i].grazed)
            continue;

        int sw = ALIEN_SHOT_W[shots[i].type];
        int sh = ALIEN_SHOT_H[shots[i].type];
        
        if(circle_rect_collide(x, y, r, shots[i].x, shots[i].y, sw, sh))
        {
            shots[i].grazed = true;
            return true;
        }
    }

    return false;
}

void shots_draw()
{
    for(int i = 0; i < SHOTS_N; i++)
    {
        if(!shots[i].used)
            continue;

        int frame_display = ((shots[i].frame / 2) % 2) + 1;

        if(shots[i].ship)
            if (shots[i].type == 1)
            {
                al_draw_bitmap(sprites.ship_shot[frame_display], shots[i].x, shots[i].y, 0);
            }
            else
            {
                al_draw_bitmap(sprites.ship_shot[0], shots[i].x, shots[i].y, 0);
            }

        else // alien
        {
            al_draw_bitmap(sprites.alien_shot[shots[i].type], shots[i].x, shots[i].y, 0);
        }
    }
}

void ship_init()
{
    ship.x = (BUFFER_W / 2) - (SHIP_W / 2);
    ship.y = (BUFFER_H / 2) - (SHIP_H / 2);
    ship.shot_timer = 0;
    ship.lives = 3;
    ship.bombs = 3;
    ship.respawn_timer = 0;
    ship.invincible_timer = 120;
}

void ship_update()
{
    if(ship.lives < 0)
        return;

    if(ship.respawn_timer)
    {
        ship.respawn_timer--;
        return;
    }

    // movement
    if(key[ALLEGRO_KEY_LSHIFT])
    {
        if(key[ALLEGRO_KEY_LEFT])
            ship.x -= FOCUS_SPEED;
        if(key[ALLEGRO_KEY_RIGHT])
            ship.x += FOCUS_SPEED;
        if(key[ALLEGRO_KEY_UP])
            ship.y -= FOCUS_SPEED;
        if(key[ALLEGRO_KEY_DOWN])
            ship.y += FOCUS_SPEED;
    }
    else
    {
        if(key[ALLEGRO_KEY_LEFT])
            ship.x -= SHIP_SPEED;
        if(key[ALLEGRO_KEY_RIGHT])
            ship.x += SHIP_SPEED;
        if(key[ALLEGRO_KEY_UP])
            ship.y -= SHIP_SPEED;
        if(key[ALLEGRO_KEY_DOWN])
            ship.y += SHIP_SPEED;
    }

    if(ship.x < 0)
        ship.x = 0;
    if(ship.y < 0)
        ship.y = 0;

    if(ship.x > SHIP_MAX_X)
        ship.x = SHIP_MAX_X;
    if(ship.y > SHIP_MAX_Y)
        ship.y = SHIP_MAX_Y;

    // logic for handling if your ship gets hit
    if(ship.invincible_timer)
        ship.invincible_timer--;
    else
    {
        if(shots_collide(true, ship.x + 9, ship.y + 19, HIT_W, HIT_H))
        {
            int x = ship.x + (SHIP_W / 2);
            int y = ship.y + (SHIP_H / 2);
            al_play_sample(
                sample_explode[1],
                get_volume(false, 0.5),
                get_pan(ship.x),
                1,
                ALLEGRO_PLAYMODE_ONCE,
                NULL
            );
            hitstop_timer = (hair_len*2);
        }
        for(int i = 0; i < ALIENS_N; i++)
        {
            if(rect_collide(ship.x+9, ship.y+19, ship.x+9+HIT_W, ship.y+19+HIT_H, aliens[i].x, aliens[i].y, aliens[i].x+ALIEN_W[aliens[i].type], aliens[i].y+ALIEN_H[aliens[i].type])
            && aliens[i].used) // 202 character if statement lol
                {
                    int x = ship.x + (SHIP_W / 2);
                    int y = ship.y + (SHIP_H / 2);
                    al_play_sample(
                        sample_explode[1],
                        get_volume(false, 0.5),
                        get_pan(ship.x),
                        1,
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );

                    hitstop_timer = (hair_len*2);
                }
        }

    }

    // graze logic
    if(shots_graze(true, ship.x + 9, ship.y + 19, GRAZE_R))
    {
        // TODO; what happens when you graze?
        score += 10;
        if (hair_len < 15)
        {
            hair_len++;
        }
    }

    // logic for shooting
    if(ship.shot_timer)
        ship.shot_timer--;
    else if(key[ALLEGRO_KEY_Z])
    {
        //shot pattern stuff
        int x = ship.x + (SHIP_W / 2);

        if(shots_add(true, 0, 0, -5, x+3, ship.y)
        && shots_add(true, 0, 0, -5, x-3, ship.y)
        && shots_add(true, 1, 1, -5, x+3, ship.y)
        && shots_add(true, 1, -1, -5, x-3, ship.y))
            ship.shot_timer = 5;
    }
    // logic for bombing
    if (keydown[ALLEGRO_KEY_X] && (ship.bombs > 0))
    {
        ship.bombs -= 1;
        do_bomb();
    }
}

void ship_draw()
{
    if(ship.lives < 0)
        return;
    if(ship.respawn_timer)
    {
        int rotation_scaler = M_PI/8;
        // death animation
        float angle = 0.1 * ship.frame;
        int spacing = 2;
        int drawposx = (ship.x+(SHIP_W/2)) + ((spacing * angle) * cos(angle));
        int drawposy = (ship.y+(SHIP_H/2)) + ((spacing * angle) * sin(angle));
        al_draw_scaled_rotated_bitmap(sprites.ship, SHIP_W/2, SHIP_H/2, drawposx, drawposy, 1.0/((0.1*ship.frame)+1), 1.0/((0.1*ship.frame)+1), ship.frame+1*rotation_scaler, 0);
        ship.frame++;
        return;
    }
    if(((ship.invincible_timer / 2) % 3) == 1)
        return;

    al_draw_bitmap(sprites.ship, ship.x, ship.y, 0);
}



void hair_init()
// inits the hair
{
    hair[0].x = ship.x + 8;
    hair[0].y = ship.y + 11;
    for(int i = 0; i < MAX_HAIR_LEN; i++)
        hair[i].anim_state = i % 16;
}

int get_hair_anim(int anim_state)
// there's gotta be a math-ey way to do this, but it's probably only efficient at way higher numbers
// that's what i'm telling myself anyway
{
    if (anim_state < 8)
    {
        if (anim_state == 0)
            return 0;
        else if (anim_state > 2 && anim_state < 6)
            return 2;
        else
            return 1;
    }
    else
    {
        if (anim_state == 8)
            return 0;
        else if (anim_state > 10 && anim_state < 14)
            return -2;
        else
            return -1;
    }
}

void hair_draw()
// draws the hair
{   
    // dead men have no hair
    if(ship.lives < 0)
        return;
    if(ship.respawn_timer)
        return;
    if(((ship.invincible_timer / 2) % 3) == 1)
        return;
    // the first unit of hair is special, it is always attatched to your head
    hair[0].last_x = hair[0].x;
    hair[0].x = ship.x + 8;
    hair[0].y = ship.y + 11;
    int anim_offset = get_hair_anim(hair[0].anim_state);
    hair[0].anim_state = ((hair[0].anim_state + 15) % 16);
    al_draw_bitmap(sprites.hair, hair[0].x + anim_offset, hair[0].y, 0);

    for(int i = 1; i < MAX_HAIR_LEN; i++)
    // fucking unhinged hair physics bullshit
    {   
        int anim_offset = get_hair_anim(hair[i].anim_state);
        hair[i].y = hair[i-1].y + 1; // this piece of hair is 1 pixel longer than the last
        hair[i].last_x = hair[i].x; // before we change the x we need to record what it was

        // i swear there is a more efficient/more math-ey way to do this, but i am too stupid to find it
        // on the other hand, is it really worth my time to perfectly optimise hair movement in a bootleg touhou game?
        if (hair[i-1].x - hair[i-1].last_x > 1)
            hair[i].x = hair[i-1].x - 1;
        else if (hair[i-1].x - hair[i-1].last_x < -1)
            hair[i].x = hair[i-1].x + 1;
        else
            hair[i].x = hair[i-1].last_x;
        
        hair[i].anim_state = ((hair[i].anim_state + 15) % 16);
        if (i <= hair_len)
            al_draw_bitmap(sprites.hair, (hair[i].x + anim_offset), hair[i].y, 0);
    }
}

void hitbox_draw()
// draws the hitbox
{   
    // dead men don't have hitboxes
    if(ship.lives < 0)
        return;
    if(ship.respawn_timer)
        return;
    if(((ship.invincible_timer / 2) % 3) == 1)
        return;
    // can only see hitbox if sneaking
    if(key[ALLEGRO_KEY_LSHIFT])
        al_draw_bitmap(sprites.hitbox, ship.x+6, ship.y+16, 0);
}

void aliens_init()
{
    for(int i = 0; i < ALIENS_N; i++)
        aliens[i].used = false;
}

void aliens_update()
{
    int new_quota =
        (frames % 120)
        ? 0
        : between(2, 4)
    ;
    int new_x = between(10, BUFFER_W-50);

    for(int i = 0; i < ALIENS_N; i++)
    {
        if(!aliens[i].used)
        {
            // if this alien is unused, should it spawn?
            if(new_quota > 0)
            {
                new_x += between(40, 80);
                if(new_x > (BUFFER_W - 60))
                    new_x -= (BUFFER_W - 60);

                aliens[i].x = new_x;

                aliens[i].y = between(-40, -30);
                aliens[i].type = (between(0, ALIEN_TYPE_N));
                aliens[i].shot_timer = between(1, 99);
                aliens[i].blink = 0;
                aliens[i].used = true;

                switch(aliens[i].type) // defines alien health values
                {
                    case ALIEN_TYPE_BLOB:
                        aliens[i].life = 8;
                        break;
                    case ALIEN_TYPE_YELLOW:
                        aliens[i].life = 16;
                        break;
                    case ALIEN_TYPE_PURPLE:
                        aliens[i].life = 24;
                        break;
                    case ALIEN_TYPE_GREEN:
                        aliens[i].life = 48;
                        break;
                }

                new_quota--;
            }
            continue;
        }

        switch(aliens[i].type) // defines alien movement speed
        {
            case ALIEN_TYPE_BLOB:
                if(frames % 2)
                    aliens[i].y++;
                break;

            case ALIEN_TYPE_YELLOW:
                    aliens[i].y++;
                break;

            case ALIEN_TYPE_PURPLE:
                if(frames % 2)
                    aliens[i].y++;
                break;
            
            case ALIEN_TYPE_GREEN:
                if(!(frames % 4))
                    aliens[i].y++;
                break;
        }

        if(aliens[i].y >= BUFFER_H)
        {
            aliens[i].used = false;
            continue;
        }

        if(aliens[i].blink)
            aliens[i].blink--;

        if(shots_collide(false, aliens[i].x, aliens[i].y, ALIEN_W[aliens[i].type], ALIEN_H[aliens[i].type]))
        {
            al_play_sample(
            sample_hitmarker,
            get_volume(false, .75),
            get_pan(aliens[i].x),
            1,
            ALLEGRO_PLAYMODE_ONCE,
            NULL
            );
            aliens[i].life--;
            aliens[i].blink = 4;
        }

        int cx = aliens[i].x + (ALIEN_W[aliens[i].type] / 2);
        int cy = aliens[i].y + (ALIEN_H[aliens[i].type] / 2);

        // alien death logic
        if(aliens[i].life <= 0)
        {
            fx_add(false, cx, cy);

            switch(aliens[i].type) // defines how much score you get for killing an alien
            {
                case ALIEN_TYPE_BLOB:
                    score += 100;
                    break;

                case ALIEN_TYPE_YELLOW:
                    score += 200;
                    break;

                case ALIEN_TYPE_PURPLE:
                    score += 400;
                    break;
                
                case ALIEN_TYPE_GREEN:
                    score += 800;
                    break;
            }

            aliens[i].used = false;
            continue;
        }

        // alien bullet logic
        aliens[i].shot_timer--;
        if(aliens[i].shot_timer == 0)
        {
            switch(aliens[i].type) // defines how often aliens shoot
            {
                case ALIEN_TYPE_BLOB:
                    // Blobs are not proper Wiznerds and cannot use magic
                    aliens[i].shot_timer = 200;
                    break;
                case ALIEN_TYPE_YELLOW:
                    // TODO: what kind of shots does yellow use?
                    shots_add(false, 0, 0, 2, cx, cy);
                    aliens[i].shot_timer = 80;
                    break;
                case ALIEN_TYPE_PURPLE:
                    // TODO: what kind of shots does purple use?
                    shots_add(false, 2, 0, 2, cx+6, cy);
                    shots_add(false, 2, 0, 2, cx-6, cy);
                    aliens[i].shot_timer = 160;
                    break;
                case ALIEN_TYPE_GREEN:
                    // TODO: what kind of shots does green use?
                    shots_add(false, 3, 0, 1, cx, cy);
                    aliens[i].shot_timer = 200;
                    break;
            }
        }
    }
}

void aliens_draw()
{
    for(int i = 0; i < ALIENS_N; i++)
    {
        if(!aliens[i].used)
            continue;
        if(aliens[i].blink > 2)
            continue;

        al_draw_bitmap(sprites.alien[aliens[i].type], aliens[i].x, aliens[i].y, 0);
    }
}

void stars_init()
{
    for(int i = 0; i < STARS_N; i++)
    {
        stars[i].y = between_f(0, BUFFER_H);
        stars[i].speed = between_f(0.1, 1);
    }
}

void stars_update()
{
    for(int i = 0; i < STARS_N; i++)
    {
        stars[i].y += stars[i].speed;
        if(stars[i].y >= BUFFER_H)
        {
            stars[i].y = 0;
            stars[i].speed = between_f(0.1, 1);
        }
    }
}

void stars_draw()
{
    float star_x = 1.5;
    for(int i = 0; i < STARS_N; i++)
    {
        float l = stars[i].speed * 0.8;
        al_draw_pixel(star_x, stars[i].y, al_map_rgb_f(l,l,l));
        star_x += 2;
    }
}

ALLEGRO_FONT* font;
long score_display;

void hud_init()
{
    font = al_create_builtin_font();
    must_init(font, "font");

    score_display = 0;
}

void hud_deinit()
{
    al_destroy_font(font);
}

void hud_update()
{
    if(frames % 2)
        return;

    for(long i = 5; i > 0; i--)
    {
        long diff = 1 << i;
        if(score_display <= (score - diff))
            score_display += diff;
    }
}

void hud_draw()
{
    al_draw_textf(
        font,
        al_map_rgb_f(1,1,1),
        1, 1,
        0,
        "%06ld",
        score_display
    );

    int spacing = ICON_W + 1;
    for(int i = 0; i < ship.lives; i++)
        al_draw_bitmap(sprites.life, 1 + (i * spacing), 10, 0);
    for(int i = 0; i < ship.bombs; i++)
        al_draw_bitmap(sprites.bomb, 1 + (i * spacing), 20, 0);

    if(ship.lives < 0)
        al_draw_text(
            font,
            al_map_rgb_f(1,1,1),
            BUFFER_W / 2, BUFFER_H / 2,
            ALLEGRO_ALIGN_CENTER,
            "G A M E  O V E R"
        );
    
}

int main()
{
    must_init(al_init(), "allegro");
    must_init(al_install_keyboard(), "keyboard");

    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    must_init(timer, "timer");

    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    must_init(queue, "queue");

    disp_init();

    audio_init();

    must_init(al_init_image_addon(), "image");
    sprites_init();

    hud_init();

    must_init(al_init_primitives_addon(), "primitives");

    must_init(al_install_audio(), "audio");
    must_init(al_init_acodec_addon(), "audio codecs");
    must_init(al_reserve_samples(16), "reserve samples");

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    keyboard_init();
    fx_init();
    shots_init();
    ship_init();
    hair_init();
    aliens_init();
    stars_init();

    frames = 0;
    score = 0;

    bool done = false;
    bool redraw = true;
    ALLEGRO_EVENT event;

    al_start_timer(timer);

    while(1)
    {
        al_wait_for_event(queue, &event);

        switch(event.type)
        {
            case ALLEGRO_EVENT_TIMER:
                if (is_paused)
                {
                    //do pausing stuff
                }
                else if (hitstop_timer > 0)
                {
                    if (hitstop_timer == 1)
                        player_dies();

                    if (keydown[ALLEGRO_KEY_X] && (ship.bombs > 0))
                    {
                        ship.bombs -= 1;
                        do_deathbomb();
                    }
                    if (hitstop_timer % 2 == 1)
                    {
                        hair_len -= 1;
                    }
                    hitstop_timer -= 1;
                }
                else
                {
                    fx_update();
                    shots_update();
                    stars_update();
                    ship_update();
                    aliens_update();
                    hud_update();
                }
                
                if(keydown[ALLEGRO_KEY_P])
                    is_paused = !is_paused;
                if(key[ALLEGRO_KEY_ESCAPE])
                    done = true;

                redraw = true;
                frames++;
                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                done = true;
                break;
        }

        if(done)
            break;

        keyboard_update(&event);

        if(redraw && al_is_event_queue_empty(queue))
        {
            disp_pre_draw();
            al_clear_to_color(al_map_rgb(0,0,0));

            stars_draw();
            aliens_draw();
            fx_draw();
            ship_draw();
            hair_draw(); // i have decided the hair waving even while paused is an ascended bug
            hitbox_draw();
            shots_draw();

            hud_draw();

            disp_post_draw();
            redraw = false;
        }
    }

    sprites_deinit();
    hud_deinit();
    audio_deinit();
    disp_deinit();
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    return 0;
}