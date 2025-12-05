 #include <stdbool.h>

extern void enable_interrupt(void);

#define VGA_BASE ((volatile char*)0x08000000)
#define VGA_CTRL ((volatile int*)0x04000100)
#define BTN ((volatile int*)0x040000d0)
#define SW ((volatile int*)0x04000010)
#define DISPLAY ((volatile int*)0x04000030) 


// Timer registers
volatile unsigned short *TMR1_STAT  = (unsigned short*)0x04000020;
volatile unsigned short *TMR1_CTRL  = (unsigned short*)0x04000024;
volatile unsigned short *TMR1_PERLO = (unsigned short*)0x04000028;
volatile unsigned short *TMR1_PERHI = (unsigned short*)0x0400002C;


#define SCREEN_W 320
#define SCREEN_H 240
#define CELL_SIZE 8
#define MAX_SNAKE_LENGTH 512

#define GREEN 0x10
#define RED  0xE0
#define BLACK 0x00
#define DARKBLUE 0x03

int get_sw(void) { return *SW & 0x3FF; }  
int get_btn(void) { return *BTN & 0x1; } 

// 7-segment display control 
void set_displays(int display_number, int value) {
    if (display_number < 0 || display_number > 6) {
        return; 
    }
    static const unsigned char seg_patterns[10] = {
        0xC0, // 0
        0xF9, // 1
        0xA4, // 2
        0xB0, // 3
        0x99, // 4
        0x92, // 5
        0x82, // 6
        0xF8, // 7
        0x80, // 8
        0x90  // 9
    };
    volatile unsigned char *display =
        (volatile unsigned char *)(0x04000050 + (display_number * 0x10));  

    if (value >= 0 && value <= 9)
        *display = seg_patterns[value];
    else
        *display = 0xFF; 
}

///  Display numeric score on 7-seg display
void display_score(int score) {
    for (int i = 0; i < 7; i++) {
        int digit = score % 10;  
        score /= 10;
        set_displays(i, digit); 
    }
}


void draw_pixel(int x, int y, char color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    VGA_BASE[y * SCREEN_W + x] = color; 
}


void fill_rectangle(int x, int y, int w, int h, char color) { 
    for (int j = y; j < y + h; j++) 
        for (int i = x; i < x + w; i++) 
            draw_pixel(i, j, color); 
}


void clear_screen(char color) {
    for (int y = 0; y < SCREEN_H; y++) 
        for (int x = 0; x < SCREEN_W; x++) 
            VGA_BASE[y * SCREEN_W + x] = color; 
} 

// Snake data
typedef struct {
    int x, y;
} Point;

Point snake[MAX_SNAKE_LENGTH];  
int snake_length = 5; 
int dir = 0; // 0=up, 1=right, 2=down, 3=left
Point apple;
bool game_over = false;
int score = 0;
bool game_started = false;


unsigned int frukt = 12345;
int randnum() {
    frukt = (frukt * 1103515245 + 12345) & 0x7fffffff; 
    return frukt;
}


void draw_snake() {
    for (int i = 0; i < snake_length; i++) 
        fill_rectangle(snake[i].x, snake[i].y, CELL_SIZE, CELL_SIZE, DARKBLUE);
}

void draw_apple() {
    fill_rectangle(apple.x, apple.y, CELL_SIZE, CELL_SIZE, RED);
}


void place_apple() {
    apple.x = (randnum() % (SCREEN_W / CELL_SIZE)) * CELL_SIZE; 
    apple.y = (randnum() % (SCREEN_H / CELL_SIZE)) * CELL_SIZE; 
}


void reset_game() {
    clear_screen(GREEN);
    snake_length = 5;
    score = 0;
    dir = 1;
    game_over = false;
    game_started = true;
    for (int i = 0; i < snake_length; i++) {
        snake[i].x = SCREEN_W / 2 - i * CELL_SIZE; 
        snake[i].y = SCREEN_H / 2; 
    }
    place_apple(); 

    display_score(score);  
}


void move_snake() {
    if (game_over || !game_started) return;

    Point new_head = snake[0];
    switch (dir) {
        case 0: new_head.y -= CELL_SIZE; break; 
        case 1: new_head.x += CELL_SIZE; break; 
        case 2: new_head.y += CELL_SIZE; break; 
        case 3: new_head.x -= CELL_SIZE; break; 
    }
    
 
    if (new_head.x < 0 || new_head.x >= SCREEN_W || new_head.y < 0 || new_head.y >= SCREEN_H) {
        game_over = true;
        return;
    }


    for (int i = 0; i < snake_length; i++) {
        if (snake[i].x == new_head.x && snake[i].y == new_head.y) {
            game_over = true;
            return;
        }
    }
    

    for (int i = snake_length; i > 0; i--)
        snake[i] = snake[i - 1]; 
    snake[0] = new_head;

    
    if (new_head.x == apple.x && new_head.y == apple.y) {
    snake_length++;
    score++;
    place_apple();
    display_score(score);  
    } else {
        
        fill_rectangle(snake[snake_length].x, snake[snake_length].y, CELL_SIZE, CELL_SIZE, GREEN);
    }
    draw_snake();
    draw_apple();
}


void handle_input() {
    static int last_btn = 0;
    int btn = get_btn(); 

    if (btn && !last_btn) {
        int sw = get_sw();
        if (sw % 2 == 0) 
            dir = (dir + 1) % 4; // turn right
        else
            dir = (dir + 3) % 4; // turn left
    }
    last_btn = btn; 
}

// Timer interrupt handler
int timeoutcount = 0;
void handle_interrupt(unsigned cause) {
    *TMR1_STAT = 0; 
    timeoutcount++;  
    if (timeoutcount >= 2) { 
        timeoutcount = 0;
        if (!game_over && game_started)
            move_snake();
    }
}

// Sets how often timer interrupt happens
void labinit(void) {
    unsigned int period = (30000000 * 0.05) - 1; 
    *TMR1_CTRL = 0x0;  
    *TMR1_STAT = 0; 
    *TMR1_PERLO = (unsigned short)(period & 0xFFFF); 
    *TMR1_PERHI = (unsigned short)(period >> 16);
    *TMR1_CTRL = 0x7 | (1 << 4); //timer enable, auto-reload, interrupt enable
    enable_interrupt();
}

int main(void) {
    labinit();
    clear_screen(GREEN);

    *(VGA_CTRL + 0) = 0;
    *(VGA_CTRL + 1) = (unsigned int)VGA_BASE;  

    while (1) {
        int btn = get_btn();
        if (btn) {
            if (!game_started || game_over)
                reset_game();
        }
        handle_input();
        if (game_over) {
            clear_screen(BLACK); 
            }
            }

    return 0;
}