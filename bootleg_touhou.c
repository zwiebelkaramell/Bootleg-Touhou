#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>



// pointers
ALLEGRO_DISPLAY* disp;
ALLEGRO_BITMAP* buffer;
ALLEGRO_FONT* font;
ALLEGRO_BITMAP* playarea;
ALLEGRO_BITMAP* text_buffer;
FILE* scorefile;

// constants
#define PLAYAREA_W 576
#define PLAYAREA_H 672
#define PLAYAREA_OFFSET_X 48
#define PLAYAREA_OFFSET_Y 24

#define BUFFER_W 960
#define BUFFER_H 720

#define DISP_SCALE 1
#define DISP_W (BUFFER_W * DISP_SCALE)
#define DISP_H (BUFFER_H * DISP_SCALE)

#define HIT_W 2
#define HIT_H 2

#define SHIP_W 20
#define SHIP_H 36

#define ICON_W 6
#define ICON_H 6

#define GRAZE_R 21
#define HIGHSCORE_N 10
#define POD_N 32

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
#define SHIP_MAX_X (PLAYAREA_W - SHIP_W)
#define SHIP_MAX_Y (PLAYAREA_H - SHIP_H)
#define MAX_HAIR_LEN 30

#define POW_LVL_1 10
#define POW_LVL_2 20
#define POW_LVL_3 30
#define MAX_POWER 40

#define MAX_SCORE_MULT 200

#define BOMB_LENGTH 216
#define DEATHBOMB_LENGTH 90
#define BOMB_SIZE 100.0
#define DEATHBOMB_SIZE 60.0

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
int mango_timer = 0;
int reset_timer = 0;
int hs_input_n = 0;
int highscore_index = 0;
int endgame_animation_state = 0;
bool is_paused = 0;
bool can_restart = 0;
bool bombing = 0;
int score_mult = 1;
float speed_mult = 1;
char highscores[256];
char hud_buffer[200];
char input_buffer[10];

// structures
typedef struct SPRITES
{
    ALLEGRO_BITMAP* _sheet;
    ALLEGRO_BITMAP* background;

    ALLEGRO_BITMAP* ship;
    ALLEGRO_BITMAP* ship_shot[3];
    ALLEGRO_BITMAP* life;
    ALLEGRO_BITMAP* potion;

    ALLEGRO_BITMAP* alien[4];
    ALLEGRO_BITMAP* alien_shot[4];

    ALLEGRO_BITMAP* explosion[EXPLOSION_FRAMES];
    ALLEGRO_BITMAP* splash[SPLASH_FRAMES];

    ALLEGRO_BITMAP* hitbox;
    ALLEGRO_BITMAP* hair;

    ALLEGRO_BITMAP* bomb;
    ALLEGRO_BITMAP* mango;

} SPRITES;
SPRITES sprites;

typedef struct FX
{
    float x, y;
    int frame;
    bool splash;
    bool used;
} FX;
#define FX_N 1024
FX fx[FX_N];

typedef struct SHOT
{
    float x, y, dx, dy;
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
    float x, y;
    int shot_timer;
    int lives;
    int bombs;
    int power;
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
    float x, y;
    ALIEN_TYPE type;
    int shot_timer;
    int shot_count;
    float aimx, aimy;
    int blink;
    int life;
    bool used;
} ALIEN;
#define ALIENS_N 16
ALIEN aliens[ALIENS_N];

typedef struct HAIR_STRUCT
// data structure for the hair physics
{
    float x, y;
    float last_x;
    int anim_state;
} HAIR_STRUCT;
HAIR_STRUCT hair[MAX_HAIR_LEN];

typedef struct ITEM
// structure for things the player can pickup (powerups, points, etc)
{
    float x, y;
    int type; /*0 is small powerup, 1 is mango*/
    float size; /*collision radius for pickup logic*/
    bool used;

} ITEM;
#define ITEM_N 1024
ITEM items[ITEM_N];

typedef struct BOMB
// structure for the object that handles player bombing
{
    float x, y;
    int frame;
    int length;
    float size; /*collision radius for destroying aliens*/
    float max_size;
} BOMB;
BOMB bomb;


//typedef struct POD
// structure for spawning enemy pods
//{
//	float offset; /*relative spawn location*/
//	int location; /*0 is left, 1 is top, 2 is right*/
//	int type; /*what kind of enemy spawns. Maybe add weight to harder enemies as game gets harder. mixed pods as well*/
//	int path; /*bake in several movement paths, make them with forier circles?*/
//	int number; /*how many enemies in the pod, scales with time*/
//	int health_mult; /*multiplier to enemy health, scales with time*/
  //  bool used;
//} POD;
//POD pod[POD_N];


typedef struct STAR
{
    float y;
    float speed;
} STAR;
#define STARS_N ((PLAYAREA_W / 2) - 1)
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

bool rect_collide(float ax1, float ay1, float ax2, float ay2, float bx1, float by1, float bx2, float by2)
{
    if(ax1 > bx2) return false;
    if(ax2 < bx1) return false;
    if(ay1 > by2) return false;
    if(ay2 < by1) return false;

    return true;
}

bool circle_collide(float ax, float ay, float ar, float bx, float by, float br)
// stolen from the internet because i want it to be optimal and am too lazy to reinvent the wheel
{
    if((pow((ax - bx), 2)) + (pow((ay - by), 2)) <= (pow((ar + br), 2))) return true;
    return false;
}

bool circle_rect_collide(float ax, float ay, float ar, float bx, float by, int bw, int bh)
// stolen from the internet because there is no way in hell i would have come up with something this efficient
// a is the cricle, b is the rectangle
{
    bx = (bx + (bw/2));
    by = (by + (bh/2));

    float xdiff = abs(ax - bx);
    float ydiff = abs(ay - by);

    if (xdiff > ((bw/2)+ar)) return false;
    if (ydiff > ((bh/2)+ar)) return false;

    if (xdiff <= (bw/2)) return true;
    if (ydiff <= (bh/2)) return true;

    if ((pow((xdiff - (bw/2)), 2)) + (pow((ydiff - (bh/2)), 2)) <= pow(ar, 2)) return true;
    return false;
}

float Q_rsqrt( float number )
// :3
{
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;						// evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}

void find_vector(float ax, float ay, float bx, float by, float* vectx, float* vecty)
// returns a normalized vector pointing from bx, by to ax, ay
// output is two pointers, not an actual return value
{
    float xdiff = (ax - bx);
    float ydiff = (ay - by);

    float norm_fact = Q_rsqrt((pow(xdiff,2) + pow(ydiff,2)));
    *vectx = xdiff * norm_fact;
    *vecty = ydiff * norm_fact;
}

void empty_string(char* str, int len)
// why is this not in the default library?
{
    for(int i = 0; i < len; i++)
    {
        str[i] = '\0';
    }
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
    return ((x / (float)(PLAYAREA_W / 2)) - 1.0);
}
    
 int get_score(int n)
 // returns the integer value of the score in position "n" in the highscores
 {
    char buf[16];
    int num_start = (n*11)+4;
    for(int i = 0; i < 6; i++)
    {
        buf[i] = highscores[num_start + i];
    }
    int res = atoi(buf);
    printf("got score: %d\n", res);
    return res;
}

bool check_score(int i_score)
// returns true if the given score is higher than one in the scorefile
{
    for(int i = 0; i < HIGHSCORE_N; i++)
    {
        if(i_score > get_score(i))
        {
            highscore_index = i;
            return true;
        }
    }
    return false;
}

void set_score(int index)
// sets highscores to the new value, then writes them to the scorefile
{
    for(int i = 0; i < 10; i++)
    {
        highscores[(index*11) + i] = input_buffer[i];
    }
    bool cursormove = fseek(scorefile, 0, SEEK_SET);
    size_t write_size = fwrite(highscores, sizeof(char), strlen(highscores), scorefile);
    printf("%d\n", cursormove);
    printf("%d\n", strlen(highscores));
    printf("set score: %d\n", write_size);
}

void draw_scaled_text(
    float color_R, float color_G, float color_B,
    float x, float y, float dx, float dy, int flags, char* text)
// draws bitmap fonts with size scaling
// some flags don't work properly, this is not enough of a problem for me to bother fixing right now
{
    al_set_target_bitmap(text_buffer);
    al_clear_to_color(al_map_rgba(0,0,0,0));
    float xpos_offset, ypos_offset, w, h = 0;
    switch(flags)
    {
        case 0:
            xpos_offset = DISP_W/2;
            ypos_offset = DISP_H/2;
            break;
        case ALLEGRO_ALIGN_CENTRE:
            w = (dx * al_get_text_width(font, text));
            h = (dy * al_get_font_line_height(font));
            xpos_offset = (DISP_W/2) - (w/2);
            ypos_offset = (DISP_H/2) - (h/2);
            break;
    }
    al_draw_text(
        font,
        al_map_rgb_f(color_R,color_G,color_B),
        DISP_W/2, DISP_H/2,
        flags,
        text
    );
    al_set_target_bitmap(buffer);
    al_draw_scaled_bitmap(text_buffer, xpos_offset, ypos_offset, DISP_W, DISP_H, x-w, y-h, (DISP_W * dx), (DISP_H * dy), 0);
}

void disp_init()
{
    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_SAMPLES, 8, ALLEGRO_SUGGEST);

    disp = al_create_display(DISP_W, DISP_H);
    must_init(disp, "display");

    buffer = al_create_bitmap(BUFFER_W, BUFFER_H);
    must_init(buffer, "bitmap buffer");

    playarea = al_create_bitmap(PLAYAREA_W, PLAYAREA_H);
    must_init(playarea, "playarea bitmap");

    text_buffer = al_create_bitmap(DISP_W, DISP_H);
    must_init(text_buffer, "text buffer");

    scorefile = fopen("data.dat", "r+");
    must_init(scorefile, "high score file");
}

void disp_deinit()
{
    al_destroy_bitmap(playarea);
    al_destroy_bitmap(buffer);
    al_destroy_display(disp);
    al_destroy_bitmap(text_buffer);
    fclose(scorefile);
}

void disp_pre_draw()
{
    al_set_target_bitmap(playarea);
}

void disp_pre_post_draw()
// TODO: come up with better name than pre_post_draw
{
    al_set_target_bitmap(buffer);
    al_draw_bitmap(sprites.background, 0, 0, 0);
    al_draw_bitmap(playarea, PLAYAREA_OFFSET_X, PLAYAREA_OFFSET_Y, 0);
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

    sprites.background = al_load_bitmap("BG.png");
    must_init(sprites.background, "background");

    sprites.ship = sprite_grab(0, 0, SHIP_W, SHIP_H);
    sprites.hitbox = sprite_grab(0, 36, 8, 8);

    sprites.bomb = sprite_grab(8, 36, ICON_W, ICON_H);
    sprites.life = sprite_grab(14, 36, ICON_W, ICON_H);
    sprites.potion = sprite_grab(20, 37, 6, 6);

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

    sprites.mango = sprite_grab(26, 38, 2, 3);

    sprites.hair = sprite_grab(8, 7, 4, 1);
}

void sprites_deinit()
{
    al_destroy_bitmap(sprites.ship);
    al_destroy_bitmap(sprites.hitbox);

    al_destroy_bitmap(sprites.bomb);
    al_destroy_bitmap(sprites.life);
    al_destroy_bitmap(sprites.potion);

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

    al_destroy_bitmap(sprites.mango);

    al_destroy_bitmap(sprites.hair);

    al_destroy_bitmap(sprites.background);
    al_destroy_bitmap(sprites._sheet);
}

ALLEGRO_SAMPLE* sample_player_shot;
ALLEGRO_SAMPLE* sample_enemy_shot[2];
ALLEGRO_SAMPLE* sample_bomb;
ALLEGRO_SAMPLE* sample_deathbomb;
ALLEGRO_SAMPLE* sample_powerup;
ALLEGRO_SAMPLE* sample_lifeget;
ALLEGRO_SAMPLE* sample_bombget;
ALLEGRO_SAMPLE* sample_explode[2];
ALLEGRO_SAMPLE* sample_hitmarker;
ALLEGRO_SAMPLE* sample_death;
ALLEGRO_SAMPLE* sample_hairloss;
ALLEGRO_SAMPLE* sample_graze;

void audio_init()
{
    al_install_audio();
    al_init_acodec_addon();
    al_reserve_samples(256);

    sample_player_shot = al_load_sample("player_shot.flac");
    must_init(sample_player_shot, "player shot sample");

    sample_enemy_shot[0] = al_load_sample("enemy_shot_1.flac");
    must_init(sample_enemy_shot[0], "enemy shot sample 1");
    sample_enemy_shot[1] = al_load_sample("enemy_shot_2.flac");
    must_init(sample_enemy_shot[1], "enemy shot sample 2");

    sample_bomb = al_load_sample("bomb.flac");
    must_init(sample_bomb, "bomb sample");
    sample_deathbomb = al_load_sample("deathbomb.flac");
    must_init(sample_deathbomb, "deathbomb sample");

    sample_powerup = al_load_sample("powerup.flac");
    must_init(sample_powerup, "powerup sample");

    sample_lifeget = al_load_sample("lifeget.flac");
    must_init(sample_lifeget, "lifeget sample");
    sample_bombget = al_load_sample("bombget.flac");
    must_init(sample_bombget, "bombget sample");

    sample_explode[0] = al_load_sample("explode1.flac");
    must_init(sample_explode[0], "explode[0] sample");
    sample_explode[1] = al_load_sample("explode2.flac");
    must_init(sample_explode[1], "explode[1] sample");

    sample_hitmarker = al_load_sample("hitmarker.flac");
    must_init(sample_hitmarker, "hitmarker sound");

    sample_death = al_load_sample("death.flac");
    must_init(sample_death, "death sound");

    sample_hairloss = al_load_sample("hairloss.flac");
    must_init(sample_hairloss, "hairloss sound");

    sample_graze = al_load_sample("graze.flac");
    must_init(sample_graze, "graze sound");

}

void audio_deinit()
{
    al_destroy_sample(sample_player_shot);
    al_destroy_sample(sample_enemy_shot[0]);
    al_destroy_sample(sample_enemy_shot[1]);
    al_destroy_sample(sample_bomb);
    al_destroy_sample(sample_deathbomb);
    al_destroy_sample(sample_powerup);
    al_destroy_sample(sample_lifeget);
    al_destroy_sample(sample_bombget);
    al_destroy_sample(sample_explode[0]);
    al_destroy_sample(sample_explode[1]);
    al_destroy_sample(sample_hitmarker);
    al_destroy_sample(sample_death);
    al_destroy_sample(sample_hairloss);
    al_destroy_sample(sample_graze);
}

void fx_init()
{
    for(int i = 0; i < FX_N; i++)
        fx[i].used = false;
}

void fx_add(bool splash, float x, float y)
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

        float x = fx[i].x - (al_get_bitmap_width(bmp) / 2);
        float y = fx[i].y - (al_get_bitmap_height(bmp) / 2);
        al_draw_bitmap(bmp, x, y, 0);
    }
}

void do_bomb(int length, float max_size)
// adds a new bomb
{
    bombing = true;
    bomb.x = ship.x+(SHIP_W/2);
    bomb.y = ship.y+(SHIP_H/2);
    bomb.frame = 0;
    bomb.size = 0;
    bomb.length = length;
    bomb.max_size = max_size;
    ship.invincible_timer = length;
    speed_mult = 0.5;
}

void bomb_update()
// does internal logic for the bomb
{
    if(bomb.frame < bomb.length
    && bombing)
    {
        bomb.x = ship.x+(SHIP_W/2);
        bomb.y = ship.y+(SHIP_H/2);

        for(int i = 0; i < ALIENS_N; i++)
        {   
            if(aliens[i].used)
            {
                if(circle_rect_collide(bomb.x, bomb.y, bomb.size, aliens[i].x, aliens[i].y, ALIEN_W[aliens[i].type], ALIEN_H[aliens[i].type]))
                {
                    aliens[i].life = 0;
                    printf("aliem hit\n");
                }
            }
        }
        for(int i = 0; i < SHOTS_N; i++)
        {
            if(shots[i].used && !shots[i].ship)
            {
                if(circle_rect_collide(bomb.x, bomb.y, bomb.size, shots[i].x, shots[i].y, ALIEN_SHOT_W[shots[i].type], ALIEN_SHOT_H[shots[i].type]))
                {
                    shots[i].used = false;
                    printf("shot hit\n");
                }
            }
        }

        if(bomb.size < bomb.max_size)
            bomb.size += 1;
        bomb.frame += 1;
    }
    else if(bombing)
    {
        bombing = false;
        speed_mult = 1.0;
    }
}

void bomb_draw()
// draws the bomb to the screen
{
    //TODO: rework this to make bombing look really epic
    if(bombing)
    {
        al_draw_filled_circle(bomb.x, bomb.y, bomb.size, al_map_rgb_f(1,0,0));
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
    score_mult = 1;
    speed_mult = 1.0;
    ship.power = (ship.power/2);
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

/*
bool pod_add()
// adds a pod, should be called with increasing frequency and difficulty by another function
{
    for(int i = 0; i < POD_N; i++)
    {
        if(pod[i].used)
            continue;

        
        pod[i].used = true;
        return true;
    }
}
*/
void shots_init()
{
    for(int i = 0; i < SHOTS_N; i++)
        shots[i].used = false;
}

bool shots_add(bool ship, int type, float dx, float dy, float x, float y)
{
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
            if((shots[i].x < -ALIEN_SHOT_W[shots[i].type])
            || (shots[i].x > PLAYAREA_W)
            || (shots[i].y < -ALIEN_SHOT_H[shots[i].type])
            || (shots[i].y > PLAYAREA_H)
            ) {
                shots[i].used = false;
                continue;
            }
        }
        else // alien
        {

            if((shots[i].x < -ALIEN_SHOT_W[shots[i].type])
            || (shots[i].x > PLAYAREA_W)
            || (shots[i].y < -ALIEN_SHOT_H[shots[i].type])
            || (shots[i].y > PLAYAREA_H)
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

bool shots_collide(bool ship, float x, float y, int w, int h)
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

bool shots_graze(bool ship, float x, float y, float r)
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

void items_init()
{
    for(int i = 0; i < ITEM_N; i++)
    {
        items[i].used = false;
    }
}

void items_update()
{
    for(int i = 0; i < ITEM_N; i++)
    {
        if(items[i].used)
        {
            if(circle_collide(ship.x+(SHIP_W/2), ship.y+(SHIP_H/2), GRAZE_R, items[i].x, items[i].y, items[i].size))
                {
                    float vectx, vecty;
                    find_vector(items[i].x, items[i].y, (ship.x+(SHIP_W/2)), (ship.y+(SHIP_H/2)), &vectx, &vecty);
                    items[i].x -= 2*vectx;
                    items[i].y -= 2*vecty;

                    if(circle_rect_collide(items[i].x, items[i].y, items[i].size, ship.x+(SHIP_W/2)-(HIT_W/2), ship.y+(SHIP_H/2)-(HIT_H/2), ship.x+(SHIP_W/2)+(HIT_W/2), ship.y+(SHIP_H/2)+(HIT_H/2)))
                    // i enjoy very long if statements
                    {
                        items[i].used = false;
                        // TODO: make this a case statement depending on item type, when you have more than one item
                        switch(items[i].type)
                        {
                            case 0: /* potion */
                                if(ship.power < MAX_POWER)
                                {
                                    if((ship.power == (POW_LVL_1-1))||(ship.power == (POW_LVL_2-1))||(ship.power == (POW_LVL_3-1))||(ship.power == (MAX_POWER-1)))
                                    {
                                        al_play_sample(
                                            sample_powerup,
                                            get_volume(false, 0.5),
                                            get_pan(ship.x),
                                            1,
                                            ALLEGRO_PLAYMODE_ONCE,
                                            NULL
                                        );
                                    }

                                    ship.power += 1;
                                }
                                else
                                {
                                    score += score_mult;
                                    if(score_mult < MAX_SCORE_MULT)
                                    {
                                        score_mult *= 2;
                                    }
                                }
                                break;
                            case 1: /* mango */
                                if(speed_mult = 1.0)
                                    speed_mult = 2.0;
                                break;
                        }
                        
                    }
                    continue;
                }

            items[i].y += 1;
            if(items[i].y >= PLAYAREA_H)
                items[i].used = false;
        }
    }
}

bool items_add(float x, float y, int type, float size)
{
    for(int i = 0; i < ITEM_N; i++)
    {
        if(items[i].used)
        {
            continue;
        }

        items[i].x = x;
        items[i].y = y;
        items[i].type = type;
        items[i].size = size;
        items[i].used = true;

        return true;
    }
    printf("couldn't generate item\n");
    return false;
}

void items_draw()
{
    for(int i = 0; i < ITEM_N; i++)
    {
        if(!items[i].used)
        {
            continue;
        }

        switch(items[i].type)
        {
            case 0:
                al_draw_bitmap(sprites.potion, items[i].x, items[i].y, 0);
                break;
            case 1:
                al_draw_bitmap(sprites.mango, items[i].x, items[i].y, 0);
                break;
        }
    }
}

void ship_init()
{
    ship.x = (PLAYAREA_W - (PLAYAREA_W / 2)) - (SHIP_W / 2);
    ship.y = (PLAYAREA_H - (PLAYAREA_H / 4)) - (SHIP_H / 2);
    ship.shot_timer = 0;
    ship.lives = 3;
    ship.bombs = 3;
    ship.respawn_timer = 0;
    ship.invincible_timer = 120;
    ship.power = 0;
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
            ship.x -= (int)(FOCUS_SPEED * speed_mult);
        if(key[ALLEGRO_KEY_RIGHT])
            ship.x += (int)(FOCUS_SPEED * speed_mult);
        if(key[ALLEGRO_KEY_UP])
            ship.y -= (int)(FOCUS_SPEED * speed_mult);
        if(key[ALLEGRO_KEY_DOWN])
            ship.y += (int)(FOCUS_SPEED * speed_mult);
    }
    else
    {
        if(key[ALLEGRO_KEY_LEFT])
            ship.x -= (int)(SHIP_SPEED * speed_mult);
        if(key[ALLEGRO_KEY_RIGHT])
            ship.x += (int)(SHIP_SPEED * speed_mult);
        if(key[ALLEGRO_KEY_UP])
            ship.y -= (int)(SHIP_SPEED * speed_mult);
        if(key[ALLEGRO_KEY_DOWN])
            ship.y += (int)(SHIP_SPEED * speed_mult);
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
            float x = ship.x + (SHIP_W / 2);
            float y = ship.y + (SHIP_H / 2);
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
                    float x = ship.x + (SHIP_W / 2);
                    float y = ship.y + (SHIP_H / 2);
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
        al_play_sample(
            sample_graze,
            get_volume(false, 0.3),
            0,
            1.0,
            ALLEGRO_PLAYMODE_ONCE,
            NULL
        );
    }

    // logic for shooting
    if(ship.shot_timer)
        ship.shot_timer--;
    else if(key[ALLEGRO_KEY_Z])
    {
        //shot pattern stuff
        float x = ship.x + (SHIP_W / 2);
        if(ship.power < POW_LVL_1)
        {
            if(shots_add(true, 0, 0, -5, x, ship.y))
                ship.shot_timer = 5;

            al_play_sample(
                sample_player_shot,
                get_volume(false, 0.3),
                get_pan(x),
                1.0,
                ALLEGRO_PLAYMODE_ONCE,
                NULL
            );
        }
        else if(ship.power < POW_LVL_2)
        {
            if(shots_add(true, 0, 0, -5, x+3, ship.y)
            && shots_add(true, 0, 0, -5, x-3, ship.y))
                ship.shot_timer = 5;

            al_play_sample(
                sample_player_shot,
                get_volume(false, 0.3),
                get_pan(x),
                1.0,
                ALLEGRO_PLAYMODE_ONCE,
                NULL
            );    
        }
        else if(ship.power < POW_LVL_3)
        {
            if(shots_add(true, 0, 0, -5, x+3, ship.y)
            && shots_add(true, 0, 0, -5, x-3, ship.y)
            && shots_add(true, 1, 1, -5, x+3, ship.y)
            && shots_add(true, 1, -1, -5, x-3, ship.y))
                ship.shot_timer = 5;

            al_play_sample(
                sample_player_shot,
                get_volume(false, 0.3),
                get_pan(x),
                1.0,
                ALLEGRO_PLAYMODE_ONCE,
                NULL
            );
        }
        else
        {
            if(shots_add(true, 0, 0, -5, x+3, ship.y)
            && shots_add(true, 0, 0, -5, x-3, ship.y)
            && shots_add(true, 1, 1, -5, x+3, ship.y)
            && shots_add(true, 1, -1, -5, x-3, ship.y)
            && shots_add(true, 1, 2, -5, x+3, ship.y)
            && shots_add(true, 1, -2, -5, x-3, ship.y))
            ship.shot_timer = 5;

            al_play_sample(
                sample_player_shot,
                get_volume(false, 0.3),
                get_pan(x),
                1.0,
                ALLEGRO_PLAYMODE_ONCE,
                NULL
            );
        }
        mango_timer = 0;
    }
    else
    {
        mango_timer += 1;
        if((mango_timer > 600) && (ship.power == MAX_POWER))
        {
            items_add(between(0, PLAYAREA_W-2), 0, 1, 1);
        }
    }
    // logic for bombing
    if (keydown[ALLEGRO_KEY_X] && (ship.bombs > 0))
    {
        ship.bombs -= 1;
        do_bomb(BOMB_LENGTH, BOMB_SIZE);

        al_play_sample(
                        sample_bomb,
                        get_volume(false, .50),
                        get_pan(ship.y),
                        1.0,
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );
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
        float spacing = 2;
        float drawposx = (ship.x+(SHIP_W/2)) + ((spacing * angle) * cos(angle));
        float drawposy = (ship.y+(SHIP_H/2)) + ((spacing * angle) * sin(angle));
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

float get_hair_anim(int anim_state)
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
    float anim_offset = get_hair_anim(hair[0].anim_state);
    hair[0].anim_state = ((hair[0].anim_state + 15) % 16);
    al_draw_bitmap(sprites.hair, hair[0].x + anim_offset, hair[0].y, 0);

    for(int i = 1; i < MAX_HAIR_LEN; i++)
    // fucking unhinged hair physics bullshit
    {   
        float anim_offset = get_hair_anim(hair[i].anim_state);
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
    {
        aliens[i].used = false;
        aliens[i].shot_count = 0;
    }
}

void aliens_update()
{
    int new_quota =
        (frames % 120)
        ? 0
        : between(2, 4)
    ;
    float new_x = between(10, PLAYAREA_W-50);

    for(int i = 0; i < ALIENS_N; i++)
    {

        float cx = aliens[i].x + (ALIEN_W[aliens[i].type] / 2);
        float cy = aliens[i].y + (ALIEN_H[aliens[i].type] / 2);

        if(!aliens[i].used)
        {
            // if this alien is unused, should it spawn?
            if(new_quota > 0)
            {
                new_x += between(40, 80);
                if(new_x > (PLAYAREA_W - 60))
                    new_x -= (PLAYAREA_W - 60);

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

        if(aliens[i].y >= PLAYAREA_H)
        {
            aliens[i].used = false;
            continue;
        }

        if(aliens[i].blink)
            aliens[i].blink--;

        if(shots_collide(false, aliens[i].x, aliens[i].y, ALIEN_W[aliens[i].type], ALIEN_H[aliens[i].type]))
        {
            if(ship.power < POW_LVL_1)
            {
                al_play_sample(
                    sample_hitmarker,
                    get_volume(false, 1),
                    get_pan(aliens[i].x),
                    1,
                    ALLEGRO_PLAYMODE_ONCE,
                    NULL
                );
                aliens[i].life -= 3;
                aliens[i].blink = 4;
            }
            else if(ship.power < POW_LVL_2)
            {
                al_play_sample(
                    sample_hitmarker,
                    get_volume(false, .75),
                    get_pan(aliens[i].x),
                    1,
                    ALLEGRO_PLAYMODE_ONCE,
                    NULL
                );
                aliens[i].life -= 2;
                aliens[i].blink = 4;
            }
            else if(ship.power < POW_LVL_3)
            {
                al_play_sample(
                    sample_hitmarker,
                    get_volume(false, .5),
                    get_pan(aliens[i].x),
                    1,
                    ALLEGRO_PLAYMODE_ONCE,
                    NULL
                );
                aliens[i].life -= 2;
                aliens[i].blink = 4;
            }
            else
            {
                al_play_sample(
                    sample_hitmarker,
                    get_volume(false, .30),
                    get_pan(aliens[i].x),
                    1,
                    ALLEGRO_PLAYMODE_ONCE,
                    NULL
                );
                aliens[i].life -= 2;
                aliens[i].blink = 4;
            }
        }

        // alien death logic
        if(aliens[i].life <= 0)
        {
            fx_add(false, cx, cy);
            items_add(cx-3, cy-3, 0, 1);

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
                    float vectx, vecty;
                    if(aliens[i].shot_count == 0)
                    {
                        find_vector(ship.x+(SHIP_W/2), ship.y+(SHIP_H/2), cx, cy, &vectx, &vecty);
                        aliens[i].aimx = (2 * vectx);
                        aliens[i].aimy = (2 * vecty);
                        shots_add(false, 0, aliens[i].aimx, aliens[i].aimy, cx, cy);
                        aliens[i].shot_timer = 10;
                        aliens[i].shot_count += 1;
                    }
                    else if(aliens[i].shot_count < 2)
                    {
                        shots_add(false, 0, aliens[i].aimx, aliens[i].aimy, cx, cy);
                        aliens[i].shot_timer = 10;
                        aliens[i].shot_count += 1;
                    }
                    else
                    {
                        shots_add(false, 0, aliens[i].aimx, aliens[i].aimy, cx, cy);
                        aliens[i].shot_timer = 80;
                        aliens[i].shot_count = 0;
                    }

                    al_play_sample(
                        sample_enemy_shot[between(0,1)],
                        get_volume(false, 0.3),
                        get_pan(cx),
                        between_f(0.9, 1.1),
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );

                    break;
                case ALIEN_TYPE_PURPLE:
                    // TODO: what kind of shots does purple use?
                    shots_add(false, 2, 0, 2, cx+6, cy);
                    shots_add(false, 2, 0, 2, cx-6, cy);
                    aliens[i].shot_timer = 160;

                    al_play_sample(
                        sample_enemy_shot[between(0,1)],
                        get_volume(false, 0.3),
                        get_pan(cx),
                        between_f(0.9, 1.1),
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );

                    break;
                case ALIEN_TYPE_GREEN:
                    // TODO: what kind of shots does green use?
                    shots_add(false, 3, 0, 1, cx, cy);
                    aliens[i].shot_timer = 200;

                    al_play_sample(
                        sample_enemy_shot[between(0,1)],
                        get_volume(false, 0.3),
                        get_pan(cx),
                        between_f(0.9, 1.1),
                        ALLEGRO_PLAYMODE_ONCE,
                        NULL
                    );

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
        stars[i].y = between_f(0, PLAYAREA_H);
        stars[i].speed = between_f(0.1, 1);
    }
}

void stars_update()
{
    for(int i = 0; i < STARS_N; i++)
    {
        stars[i].y += stars[i].speed;
        if(stars[i].y >= PLAYAREA_H)
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

long score_display;

void hud_init()
{
    font = al_create_builtin_font();
    must_init(font, "font");

    score_display = 0;
    reset_timer = 600;
    hs_input_n = 0;
    endgame_animation_state = 0;
    empty_string(hud_buffer, 200);
    empty_string(input_buffer, 10);

}

void hud_deinit()
{
    al_destroy_font(font);
}

void hud_update()
{
    if(frames % 2)
    {
        for(long i = 5; i > 0; i--)
        {
            long diff = 1 << i;
            if(score_display <= (score - diff))
                score_display += diff;
        }
    }

    if(ship.lives < 0)
    {
        if(endgame_animation_state == 2)
        {
            //feels inefficient, but making it better requires writing a lot of code. change if it becomes a problem
            for(int i = 0; i < ALLEGRO_KEY_MAX; i++)
            {
                if(keydown[i] && isalpha((char)(i+64)))
                {
                    input_buffer[hs_input_n] = (char)(i+64);
                    hs_input_n++;
                }
            }
        }
    }
}

void hud_draw()
{

    sprintf(hud_buffer, "Score: %06ld", score_display);
    draw_scaled_text(
        1,1,1,
        640, 100,
        2,2,
        0,
        hud_buffer
    );

    sprintf(hud_buffer, "Power: %02ld/%d", ship.power, MAX_POWER);
    draw_scaled_text(
        1,1,1,
        640, 120,
        2,2,
        0,
        hud_buffer
    );

    int spacing = (ICON_W*2) + 4;

    draw_scaled_text(
        1,1,1,
        640, 140,
        2,2,
        0,
        "Lives: "
    );

    for(int i = 0; i < ship.lives; i++)
        al_draw_scaled_bitmap(sprites.life, 0, 0, ICON_W, ICON_H, 752 + (i * spacing), 142, (ICON_W*2), (ICON_H*2), 0);

    draw_scaled_text(
        1,1,1,
        640, 160,
        2,2,
        0,
        "Bombs: "
    );
    for(int i = 0; i < ship.bombs; i++)
        al_draw_scaled_bitmap(sprites.bomb, 0, 0, ICON_W, ICON_H, 752 + (i * spacing), 162, (ICON_W*2), (ICON_H*2), 0);
    
    if(is_paused)
    {
        draw_scaled_text(
            1,1,1,
            (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, (PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y,
            2,2,
            ALLEGRO_ALIGN_CENTRE,
            "PAUSED"
        );
    }
    
    if(ship.lives < 0)
    {
        /*USE A SWITCH SATEMENT BASED ON AN ANIMATIONSTATE INTEGER, DONT GET TOO FANCY WITH IT*/
        // make sure check_score() only gets called once.
        switch(endgame_animation_state)
        {
            case 0:
                //initial "game over" screen
                draw_scaled_text(
                    1,1,1,
                    (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, (PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y,
                    2,2,
                    ALLEGRO_ALIGN_CENTRE,
                    "G A M E  O V E R"
                );
                if(reset_timer < 300)
                {
                    if(check_score(score))
                        endgame_animation_state = 1;
                    else
                    {
                        endgame_animation_state = 3;
                        reset_timer = 600;
                        can_restart = true;
                    }
                }
                reset_timer -= 1;
                break;

            case 1:
                //you have a highscore
                draw_scaled_text(
                    1,1,1,
                    (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, (PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y,
                    2,2,
                    ALLEGRO_ALIGN_CENTRE,
                    "H I G H S C O R E !"
                );
                if(reset_timer < 0)
                {
                    endgame_animation_state = 2;
                    sprintf(input_buffer, "___ %06ld", score);
                    printf("%c\n", input_buffer);
                }
                reset_timer -= 1;
                break;
            
            case 2:
                //input state            
                for(int i = 0; i < HIGHSCORE_N; i++)
                {
                    empty_string(hud_buffer, 200);
                    if(i == highscore_index)
                    {
                        for(int n = 0; n < 10; n++)
                        {
                            hud_buffer[n] = input_buffer[n];   
                        }
                        if((frames % 60) < 30)
                        {
                            draw_scaled_text(
                                1,1,0,
                                (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, ((PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y) + ((i+1)*20),
                                2,2,
                                ALLEGRO_ALIGN_CENTRE,
                                hud_buffer
                            );
                        }
                    }
                    else
                    {
                        for(int n = 0; n < 10; n++)
                        {
                            hud_buffer[n] = highscores[(n+(i*11))];  
                        }
                        draw_scaled_text(
                            1,1,1,
                            (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, ((PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y) + ((i+1)*20),
                            2,2,
                            ALLEGRO_ALIGN_CENTRE,
                            hud_buffer
                        );
                    }
                    //TODO find where i want to put the highscores on the screen
                }

                if(hs_input_n == 3)
                {
                    endgame_animation_state = 3;
                    reset_timer = 600;
                    can_restart = true;
                    set_score(highscore_index);
                }
                break;

            case 3:
                //display highscores
                for(int i = 0; i < HIGHSCORE_N; i++)
                {
                    empty_string(hud_buffer, 200);
                    for(int n = 0; n < 10; n++)
                    {
                        hud_buffer[n] = highscores[(n+(i*11))];
                    }
                    //TODO find where i want to put the highscores on the screen
                    draw_scaled_text(
                        1,1,1,
                        (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, ((PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y) + ((i+1)*20),
                        2,2,
                        ALLEGRO_ALIGN_CENTRE,
                        hud_buffer
                    );
                }
                if(reset_timer == 0)
                {
                    reset_timer = 600;
                    endgame_animation_state = 4;
                }
                reset_timer -= 1;
                break;
            
            case 4:
                //restart message
                draw_scaled_text(
                    1,1,1,
                    (PLAYAREA_W / 2) + PLAYAREA_OFFSET_X, (PLAYAREA_H / 2) + PLAYAREA_OFFSET_Y,
                    2,2,
                    ALLEGRO_ALIGN_CENTRE,
                    "PRESS \"R\" TO RESTART"
                );
                if(reset_timer < 300)
                {
                    endgame_animation_state = 3;
                }
                reset_timer -= 1;
                break;
        }
    }
    
}

void restart_game()
// restarts the game
{
    aliens_init();
    shots_init();
    items_init();
    fx_init();
    ship_init();
    hair_init();
    hud_init();
    stars_init();
    frames = 0;
    score = 0;
    can_restart = 0;
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

    fread(highscores, sizeof(char), 110, scorefile);

    must_init(al_init_primitives_addon(), "primitives");

    must_init(al_install_audio(), "audio");
    must_init(al_init_acodec_addon(), "audio codecs");
    must_init(al_reserve_samples(256), "reserve samples");

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    keyboard_init();
    fx_init();
    shots_init();
    ship_init();
    hair_init();
    aliens_init();
    items_init();
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
                        do_bomb(DEATHBOMB_LENGTH, DEATHBOMB_SIZE);

                        al_play_sample(
                                        sample_deathbomb,
                                        get_volume(false, 0.5),
                                        get_pan(ship.y),
                                        1.0,
                                        ALLEGRO_PLAYMODE_ONCE,
                                        NULL
                                    );

                        hitstop_timer = 0;
                    }
                    if (hitstop_timer % 2 == 1)
                    {
                        hair_len -= 1;
                        al_play_sample(
                            sample_hairloss,
                            get_volume(false, 0.3),
                            get_pan(ship.x),
                            1,
                            ALLEGRO_PLAYMODE_ONCE,
                            NULL
                        );
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
                    bomb_update();
                    items_update();
                    hud_update();
                }
                
                if(can_restart && keydown[ALLEGRO_KEY_R])
                    restart_game();
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
            bomb_draw();
            aliens_draw();
            fx_draw();
            items_draw();
            ship_draw();
            hair_draw(); // i have decided the hair waving even while paused is an ascended bug
            hitbox_draw();
            shots_draw();

            disp_pre_post_draw();

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