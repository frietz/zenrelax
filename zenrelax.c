/* zenrelax - Soothing nerdy terminal screensaver in pure C
 * Modes: 1-Plasma, 2-Mandelbrot, 3-Particles, 4-Quantum Flow, 5-Orbitals
 * Usage: ./zenrelax [mode]  ('q'/ESC/Ctrl+C to quit)
 * Tmux-optimized: SIGWINCH resize, alt screen, raw input, 20fps
 * Compile: gcc zenrelax.c -o zenrelax -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/select.h>

#define MAX_MODES 5
#define DEFAULT_MODE 1
#define WIDTH 80
#define HEIGHT 24
#define MAX_COLS 200
#define MAX_ROWS 100
#define PI 3.14159265359

// ANSI escape helpers
#define CLEAR_LINE "\x1b[2K"
#define CLEAR_SCREEN "\x1b[2J"
#define MOVE_HOME "\x1b[H"
#define HIDE_CURSOR "\x1b[?25l"
#define SHOW_CURSOR "\x1b[?25h"

// Color palette for soothing blues/greens (ANSI 256)
int palette[16] = {16,19,21,34,35,36,39,43,44,49,73,149,152,153,154,155};

// Global state
int rows = HEIGHT, cols = WIDTH;
int mode = DEFAULT_MODE;
double time_step = 0.0;
volatile int running = 1;
volatile sig_atomic_t resize_flag = 0;


// Get terminal size
void get_term_size() {
    struct winsize w;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &w) == 0) {
        rows = w.ws_row;
        cols = w.ws_col;
    }
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (cols > MAX_COLS) cols = MAX_COLS;
}

// Render char with color
void render_char(char ch, int x, int y, int intensity) {
    int color = palette[intensity % 16];
    printf("\x1b[%d;%df\x1b[38;5;%dm%c\x1b[0m", y+1, x+1, color, ch);
}

// Bounds-checked render
void render_char_safe(char ch, int x, int y, int intensity) {
    if (x >= 0 && x < cols && y >= 0 && y < rows)
        render_char(ch, x, y, intensity);
}

void resize_handler(int sig) {
    (void)sig;
    resize_flag = 1;
}

void int_handler(int sig) {
    (void)sig;
    running = 0;
}

// Simple plasma mode
void render_plasma() {
    char buf[MAX_COLS + 1];
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            double cx = (double)x / cols * 4 * PI;
            double cy = (double)y / rows * 4 * PI;
            double value = sin(cx) * cos(cy) * 0.5 +
                           sin(cx*2 + time_step) * cos(cy*2 + time_step*1.3) * 0.25 +
                           sin(cx*3 + time_step*0.7) * 0.125;
            int i = (int)(fabs(value) * 12);
            char ch = " .,-~:;=!*#$@"[i];
            buf[x] = ch;
        }
        buf[cols] = 0;
        int color_idx = (int)(sin(time_step + y * 0.05) * 7.5 + 8) % 16;
        printf("\x1b[%d;1f\x1b[38;5;%dm%s\x1b[0m", y+1, palette[color_idx], buf);
    }
}

// Mandelbrot fractal
void render_mandelbrot() {
    double cr = -0.7 + 0.27 * sin(time_step * 0.1);
    double ci = sin(time_step * 0.05) * 0.27;
    char buf[MAX_COLS + 1];
    for (int y = 0; y < rows; y++) {
        int max_iter = 0;
        for (int x = 0; x < cols; x++) {
            double zr = (x - cols/2.0) * 3.0 / cols + cr;
            double zi = (y - rows/2.0) * 3.0 / rows + ci;
            int iter = 0;
            double zt = zr, zi0 = zi;
            while (iter < 32 && zr * zr + zi * zi < 4) {
                double temp = zr * zr - zi * zi + zt;
                zi = 2 * zr * zi + zi0;
                zr = temp;
                iter++;
            }
            char ch = " .:-=+*#%@"[iter / 4];
            buf[x] = ch;
            if (iter > max_iter) max_iter = iter;
        }
        buf[cols] = '\0';
        int color_idx = (int)((double)max_iter * 15.0 / 32.0);
        printf("\x1b[%d;1f\x1b[38;5;%dm%s\x1b[0m", y+1, palette[color_idx % 16], buf);
    }
}

// Simple particle physics (Verlet integration)
#define NPART 128
double px[NPART], py[NPART], vx[NPART], vy[NPART], trail[NPART];

void init_particles() {
    for (int i = 0; i < NPART; i++) {
        px[i] = rand() % cols;
        py[i] = rand() % rows;
        vx[i] = (rand() % 2000 - 1000) / 1000.0 * 0.5;
        vy[i] = (rand() % 2000 - 1000) / 1000.0 * 0.5;
        trail[i] = 0;
    }
}

void render_particles() {
    static int inited = 0;
    if (!inited) {
        init_particles();
        inited = 1;
    }
    // Fade trails
    printf(CLEAR_SCREEN MOVE_HOME);
    for (int i = 0; i < NPART; i++) {
        trail[i] *= 0.95;
        int ix = px[i], iy = py[i];
        render_char_safe('*', ix, iy, (int)(trail[i] * 15));
        if (ix >= 0 && ix < cols && iy >= 0 && iy < rows)
            trail[i] = fmin(1.0, trail[i] + 0.1);
        // Physics: gravity + waves
        double wave = sin(px[i]*0.1 + time_step) * 0.3;
        vx[i] += sin(time_step + py[i]*0.05) * 0.01 + wave * 0.02;
        vy[i] += 0.005 + sin(time_step * 1.3 + px[i]*0.07) * 0.01;
        px[i] += vx[i];
        py[i] += vy[i];
        vx[i] *= 0.98; vy[i] *= 0.98; // drag
        if (px[i] < 0 || px[i] > cols) vx[i] *= -0.8;
        if (py[i] < 0 || py[i] > rows) vy[i] *= -0.8;
    }
}

// Quantum Flow (Perlin-like)
void render_quantum_flow() {
    char buf[MAX_COLS + 1];
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            double n1 = sin(x*0.1 + time_step*0.5) * cos(y*0.1 + time_step*0.3);
            double n2 = sin(x*0.05 + time_step*0.2) * cos(y*0.15 + time_step*0.4);
            double value = (n1 + n2 + 1.0) / 2.0;
            int i = (int)(value * 8);
            buf[x] = " .~:;=!*$"[i];
        }
        buf[cols] = 0;
        int color_idx = (int)(sin(time_step * 0.3 + y * 0.1) * 7 + 8) % 16;
        printf("\x1b[%d;1f\x1b[38;5;%dm%s\x1b[0m", y+1, palette[color_idx], buf);
    }
}

void render_orbitals() {
    #define N_ORBIT 12
    static double theta[N_ORBIT], r[N_ORBIT], omega[N_ORBIT], phase[N_ORBIT], otrail[N_ORBIT];
    static int inited = 0;
    if (!inited) {
        double max_r = fmin(rows, cols) / 2.5;
        for (int i = 0; i < N_ORBIT; i++) {
            r[i] = 8 + max_r * (0.15 + 0.5 * i / (N_ORBIT - 1.0));
            omega[i] = 0.18 / ((i % 4) + 1.5);
            phase[i] = i * 2 * PI / N_ORBIT;
            theta[i] = phase[i];
            otrail[i] = 1.0;
        }
        inited = 1;
    }
    printf(CLEAR_SCREEN MOVE_HOME);
    double cx = cols / 2.0;
    double cy = rows / 2.0;
    double r_cap = fmin(rows, cols) / 2.2;
    // Glow offsets: dx, dy, char, intensity_scale
    static const int glow_dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    static const int glow_dy[] = { 0, 0,-1, 1, -1,-1,  1, 1};
    static const char glow_ch[] = {'.','.','.','.',',',',',',',',' };
    static const double glow_scale[] = {6,6,6,6, 3,3,3,3};
    for (int i = 0; i < N_ORBIT; i++) {
        // Update orbit
        double puls = 0.03 * sin(time_step * 2.0 + phase[i]);
        r[i] += puls;
        r[i] = fmax(5.0, fmin(r_cap, r[i] * 0.998));
        theta[i] += omega[i] + 0.008 * sin(time_step * 0.7 + i * 0.5);
        double opx = cx + r[i] * cos(theta[i]);
        double opy = cy + r[i] * sin(theta[i]);
        int ix = (int)opx;
        int iy = (int)opy;
        int pidx = (int)(otrail[i] * 4.0);
        char pch = "o*O@"[pidx > 3 ? 3 : pidx];
        render_char_safe(pch, ix, iy, (int)(otrail[i] * 15));
        // Glow
        for (int g = 0; g < 8; g++)
            render_char_safe(glow_ch[g], ix + glow_dx[g], iy + glow_dy[g],
                             (int)(otrail[i] * glow_scale[g]));
        otrail[i] = fmin(1.0, otrail[i] * 0.96 + 0.12);
    }
}


void render_mode(int m) {
    switch (m) {
        case 1: render_plasma(); break;
        case 2: render_mandelbrot(); break;
        case 3: render_particles(); break;
        case 4: render_quantum_flow(); break;
        case 5: render_orbitals(); break;
    }
}

void show_menu() {
    printf("\n\x1b[?25hZenRelax Modes:\n");
    printf("1. Plasma Wave\n2. Fractal Mandelbrot\n3. Particle Physics\n");
    printf("4. Quantum Flow\n5. Orbital Harmony\n");
    printf("Enter mode (1-%d) or q: ", MAX_MODES);
    fflush(stdout);
}

int main(int argc, char **argv) {
    srand(time(NULL));
    setvbuf(stdout, NULL, _IOFBF, 8192);
    printf(HIDE_CURSOR);
    get_term_size();

    signal(SIGWINCH, resize_handler);
    signal(SIGINT, int_handler);

    if (argc > 1) {
        mode = atoi(argv[1]);
        if (mode < 1 || mode > MAX_MODES) mode = DEFAULT_MODE;
    } else {
        show_menu();
        char input[10];
        fgets(input, sizeof(input), stdin);
        if (input[0] == 'q' || input[0] == 'Q') return 0;
        mode = atoi(input);
        if (mode < 1 || mode > MAX_MODES) mode = DEFAULT_MODE;
    }

    printf("\x1b[?1049h" CLEAR_SCREEN MOVE_HOME);
    printf("ZenRelax Mode %d - Press 'q' or ESC to quit\n", mode);
    fflush(stdout);

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_iflag &= ~(IXON | IXOFF | ICRNL);
    newt.c_oflag &= ~(OPOST);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &newt);

    while (running) {
        if (resize_flag) {
            get_term_size();
            printf(CLEAR_SCREEN MOVE_HOME);
            fflush(stdout);
            resize_flag = 0;
        }

        render_mode(mode);
        fflush(stdout);
        time_step += 0.1;

        fd_set readfds;
        struct timeval timeout = {0, 50000}; // ~20fps
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int activity = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (activity > 0) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = 0;
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSADRAIN, &oldt);
    printf("\x1b[?1049l" SHOW_CURSOR CLEAR_SCREEN MOVE_HOME);
    return 0;
}
