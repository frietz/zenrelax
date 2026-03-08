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
#define PI 3.14159265359

// ANSI escape helpers
#define CLEAR_LINE "\x1b[2K"
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
    if (rows > 100) rows = 100; // cap for perf
    if (cols > 200) cols = 200;
}

// Render char with color
void render_char(char ch, int x, int y, int intensity) {
    int color = palette[intensity % 16];
    printf("\x1b[%d;%df\x1b[38;5;%dm%c\x1b[0m", y+1, x+1, color, ch);
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
    char buf[256];
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
    char buf[256];
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
    printf("\x1b[2J\x1b[H"); // Gentle clear
    for (int i = 0; i < NPART; i++) {
        trail[i] *= 0.95;
        int ix = px[i], iy = py[i];
        if (ix >= 0 && ix < cols && iy >= 0 && iy < rows) {
            render_char('*', ix, iy, (int)(trail[i] * 15));
            trail[i] = fmin(1.0, trail[i] + 0.1);
        }
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
    char buf[256];
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
    static double ox[4], oy[4], ovx[4], ovy[4];
    static int inited = 0;
    if (!inited) {
        ox[0] = cols / 4.0; ox[1] = 3 * cols / 4.0; ox[2] = cols / 2.0; ox[3] = cols / 2.0;
        oy[0] = rows / 4.0; oy[1] = rows / 4.0; oy[2] = 3 * rows / 4.0; oy[3] = rows / 2.0;
        ovx[0] = 0.08; ovx[1] = -0.08; ovx[2] = 0.05; ovx[3] = -0.05;
        ovy[0] = 0.08; ovy[1] = 0.08; ovy[2] = -0.05; ovy[3] = 0.12;
        inited = 1;
    }
    printf("\x1b[2J\x1b[H");
    for (int i = 0; i < 4; i++) {
        double dx = 0, dy = 0;
        for (int j = 0; j < 4; j++) {
            if (i == j) continue;
            double ddx = ox[j] - ox[i], ddy = oy[j] - oy[i];
            double dist = sqrt(ddx*ddx + ddy*ddy) + 0.1;
            dx += ddx / dist * 0.0005;
            dy += ddy / dist * 0.0005;
        }
        ovx[i] += dx + sin(time_step + i) * 0.001;
        ovy[i] += dy + cos(time_step + i) * 0.001;
        ox[i] += ovx[i]; oy[i] += ovy[i];
        ovx[i] *= 0.999; ovy[i] *= 0.999;
        if (ox[i] < 0 || ox[i] > cols) ovx[i] *= -1;
        if (oy[i] < 0 || oy[i] > rows) ovy[i] *= -1;
        render_char('@', (int)ox[i], (int)oy[i], 15);
        // Trails
        for (double t = 0.9; t > 0.1; t -= 0.2) {
            int tx = (int)(ox[i]-ovx[i]/t), ty = (int)(oy[i]-ovy[i]/t);
            if (tx >= 0 && tx < cols && ty >= 0 && ty < rows)
                render_char('.', tx, ty, 5);
        }
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

    printf("\x1b[?1049h\x1b[2J\x1b[H");
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
            printf("\x1b[2J\x1b[H");
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
    printf("\x1b[?1049l\x1b[?25h\x1b[2J\x1b[H");
    return 0;
}
