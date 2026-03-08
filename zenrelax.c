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

void resize_handler(int sig) {
    (void)sig;
    resize_flag = 1;
}

void int_handler(int sig) {
    (void)sig;
    running = 0;
}

// --- Framebuffer for flicker-free particle modes ---
static unsigned char fb_int[MAX_ROWS][MAX_COLS];

void fb_clear() {
    memset(fb_int, 0, sizeof(fb_int));
}

void fb_fade(int amount) {
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++)
            fb_int[y][x] = fb_int[y][x] > amount ? fb_int[y][x] - amount : 0;
}

void fb_stamp(int x, int y, int intensity) {
    if (x >= 0 && x < cols && y >= 0 && y < rows) {
        if (intensity > 15) intensity = 15;
        if (intensity > fb_int[y][x])
            fb_int[y][x] = intensity;
    }
}

void fb_render() {
    char trail_ch[] = " .,:;+*oO#@";
    int nch = sizeof(trail_ch) - 1;
    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            int intensity = fb_int[y][x];
            char ch = trail_ch[intensity * (nch - 1) / 15];
            int color = palette[intensity];
            if (color != prev_color) {
                pos += sprintf(buf + pos, "\x1b[38;5;%dm", color);
                prev_color = color;
            }
            buf[pos++] = ch;
        }
        pos += sprintf(buf + pos, "\x1b[0m");
        fwrite(buf, 1, pos, stdout);
    }
}

// --- Mode 1: Plasma Wave ---
void render_plasma() {
    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            double cx = (double)x / cols * 4 * PI;
            double cy = (double)y / rows * 4 * PI;
            // 4 interference layers including radial wave from center
            double v1 = sin(cx + time_step * 0.7);
            double v2 = sin(cy + time_step * 0.5);
            double v3 = sin((cx + cy + time_step) * 0.5);
            double dx = cx - 2 * PI, dy = cy - 2 * PI;
            double v4 = sin(sqrt(dx * dx + dy * dy) + time_step * 0.8);
            double value = (v1 + v2 + v3 + v4) / 4.0;

            int ci = (int)((value + 1.0) * 6.499);
            if (ci > 12) ci = 12;
            if (ci < 0) ci = 0;
            char ch = " .,-~:;=!*#$@"[ci];

            // Per-pixel color from value + slow cycling
            double t = fmod((value + 1.0) * 0.5 + time_step * 0.025, 1.0);
            int color_idx = (int)(t * 15.999);
            int color = palette[color_idx];
            if (color != prev_color) {
                pos += sprintf(buf + pos, "\x1b[38;5;%dm", color);
                prev_color = color;
            }
            buf[pos++] = ch;
        }
        pos += sprintf(buf + pos, "\x1b[0m");
        fwrite(buf, 1, pos, stdout);
    }
}

// --- Mode 2: Julia set fractal with smooth per-pixel coloring ---
void render_mandelbrot() {
    // Orbit through interesting Julia set parameter space (near Douady rabbit)
    double cr = -0.7269 + 0.18 * sin(time_step * 0.067);
    double ci =  0.1889 + 0.18 * cos(time_step * 0.053);
    // Gentle zoom breathing
    double zoom = 2.0 + 0.8 * sin(time_step * 0.031);
    double aspect = 2.0; // terminal chars are ~2x tall as wide
    int max_iter = 64;
    double log2 = log(2.0);

    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            double zr = (x - cols / 2.0) / cols * zoom * aspect;
            double zi = (y - rows / 2.0) / rows * zoom;
            int iter = 0;
            while (iter < max_iter && zr * zr + zi * zi < 4.0) {
                double temp = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = temp;
                iter++;
            }
            int color_idx;
            char ch;
            if (iter == max_iter) {
                ch = ' ';
                color_idx = 0;
            } else {
                // Smooth iteration count eliminates color banding
                double abs_z = sqrt(zr * zr + zi * zi);
                double smooth = iter + 1.0 - log(log(abs_z)) / log2;
                // Animated color cycling for breathing effect
                double t = fmod(smooth * 0.1 + time_step * 0.04, 1.0);
                if (t < 0) t += 1.0;
                color_idx = (int)(t * 15.999);
                ch = " .:-=+*#%@"[(int)fabs(fmod(smooth, 10.0))];
            }
            int color = palette[color_idx];
            if (color != prev_color) {
                pos += sprintf(buf + pos, "\x1b[38;5;%dm", color);
                prev_color = color;
            }
            buf[pos++] = ch;
        }
        pos += sprintf(buf + pos, "\x1b[0m");
        fwrite(buf, 1, pos, stdout);
    }
}

// --- Mode 3: Particle Physics with trailing framebuffer ---
#define NPART 128
double px[NPART], py[NPART], vx[NPART], vy[NPART];

void init_particles() {
    for (int i = 0; i < NPART; i++) {
        px[i] = rand() % cols;
        py[i] = rand() % rows;
        vx[i] = (rand() % 2000 - 1000) / 1000.0 * 0.5;
        vy[i] = (rand() % 2000 - 1000) / 1000.0 * 0.5;
    }
}

void render_particles() {
    static int inited = 0;
    if (!inited) {
        init_particles();
        fb_clear();
        inited = 1;
    }

    fb_fade(1); // slow fade creates comet trails

    // Two moving attractors for organic swirling motion
    double ax1 = cols / 2.0 + cols / 3.0 * sin(time_step * 0.13);
    double ay1 = rows / 2.0 + rows / 3.0 * cos(time_step * 0.11);
    double ax2 = cols / 2.0 + cols / 4.0 * cos(time_step * 0.09);
    double ay2 = rows / 2.0 + rows / 4.0 * sin(time_step * 0.17);
    double soft = 20.0; // softening radius prevents extreme forces

    for (int i = 0; i < NPART; i++) {
        // Softened gravitational attraction to both points
        double dx1 = ax1 - px[i], dy1 = ay1 - py[i];
        double dx2 = ax2 - px[i], dy2 = ay2 - py[i];
        double d1_sq = dx1 * dx1 + dy1 * dy1 + soft * soft;
        double d2_sq = dx2 * dx2 + dy2 * dy2 + soft * soft;
        vx[i] += dx1 / d1_sq * 3.0 + dx2 / d2_sq * 2.0;
        vy[i] += dy1 / d1_sq * 3.0 + dy2 / d2_sq * 2.0;

        // Gentle wave perturbation
        vx[i] += sin(time_step + py[i] * 0.05) * 0.008;
        vy[i] += cos(time_step * 1.3 + px[i] * 0.07) * 0.008;

        vx[i] *= 0.97;
        vy[i] *= 0.97;
        px[i] += vx[i];
        py[i] += vy[i];

        // Wrap around edges for continuous flow
        if (px[i] < 0) px[i] += cols;
        if (px[i] >= cols) px[i] -= cols;
        if (py[i] < 0) py[i] += rows;
        if (py[i] >= rows) py[i] -= rows;

        // Stamp with intensity based on speed
        double speed = sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
        int intensity = 10 + (int)(fmin(speed * 5, 1.0) * 5);
        fb_stamp((int)px[i], (int)py[i], intensity);
    }

    fb_render();
}

// --- Mode 4: Quantum Flow with directional characters ---
void render_quantum_flow() {
    double t = time_step;
    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            double fx = (double)x / cols * 6.0;
            double fy = (double)y / rows * 6.0;

            // Multi-octave flow field
            double v = sin(fx * 1.5 + t * 0.3) * cos(fy * 1.2 + t * 0.2)
                     + sin(fx * 3.0 + fy * 2.0 + t * 0.5) * 0.5
                     + cos(fx * 0.7 - t * 0.15) * sin(fy * 2.5 + t * 0.25) * 0.7
                     + sin((fx + fy) * 2.0 + t * 0.4) * 0.3;

            // Flow direction from partial derivatives
            double vdx = cos(fx * 1.5 + t * 0.3) * 1.5 * cos(fy * 1.2 + t * 0.2)
                        + cos(fx * 3.0 + fy * 2.0 + t * 0.5) * 1.5;
            double vdy = -sin(fx * 1.5 + t * 0.3) * sin(fy * 1.2 + t * 0.2) * 1.2
                        + cos(fx * 3.0 + fy * 2.0 + t * 0.5) * 1.0;
            double speed = sqrt(vdx * vdx + vdy * vdy);

            double density = (v + 2.5) / 5.0;
            if (density < 0) density = 0;
            if (density > 1) density = 1;

            char ch;
            if (density < 0.08) {
                ch = ' ';
            } else if (speed > 1.5) {
                // High flow: directional characters
                double angle = atan2(vdy, vdx);
                int dir = ((int)round(angle * 4.0 / PI) % 8 + 8) % 8;
                char dirs[] = "-\\|/-\\|/";
                ch = dirs[dir];
            } else {
                int di = (int)(density * 7.999);
                ch = " .~:;=!*"[di];
            }

            // Per-pixel color
            double ct = fmod(density * 0.7 + v * 0.15 + t * 0.03, 1.0);
            if (ct < 0) ct += 1.0;
            int color_idx = (int)(ct * 15.999);
            int color = palette[color_idx];
            if (color != prev_color) {
                pos += sprintf(buf + pos, "\x1b[38;5;%dm", color);
                prev_color = color;
            }
            buf[pos++] = ch;
        }
        pos += sprintf(buf + pos, "\x1b[0m");
        fwrite(buf, 1, pos, stdout);
    }
}

// --- Mode 5: Orbital Harmony with trails and central body ---
void render_orbitals() {
    #define N_ORBIT 12
    static double theta[N_ORBIT], r[N_ORBIT], omega[N_ORBIT], phase[N_ORBIT];
    static double ecc[N_ORBIT];
    static int inited = 0;
    if (!inited) {
        fb_clear();
        double max_r = fmin(rows, cols) / 2.5;
        for (int i = 0; i < N_ORBIT; i++) {
            r[i] = 8 + max_r * (0.15 + 0.5 * i / (N_ORBIT - 1.0));
            omega[i] = 0.18 / ((i % 4) + 1.5);
            phase[i] = i * 2 * PI / N_ORBIT;
            theta[i] = phase[i];
            ecc[i] = 0.1 + 0.15 * (i % 3); // mild eccentricity variation
        }
        inited = 1;
    }

    fb_fade(1); // trails persist ~0.75s at 20fps

    double cx = cols / 2.0;
    double cy = rows / 2.0;
    double r_cap = fmin(rows, cols) / 2.2;

    // Pulsating central body
    double glow_r = 3.0 + 1.5 * sin(time_step * 0.8);
    for (int dy = -(int)glow_r - 1; dy <= (int)glow_r + 1; dy++) {
        for (int dx = -(int)(glow_r * 2) - 1; dx <= (int)(glow_r * 2) + 1; dx++) {
            double d = sqrt(dx * dx / 4.0 + dy * dy);
            if (d <= glow_r) {
                int intensity = (int)((1.0 - d / glow_r) * 15);
                fb_stamp((int)cx + dx, (int)cy + dy, intensity);
            }
        }
    }

    for (int i = 0; i < N_ORBIT; i++) {
        // Orbital dynamics
        double puls = 0.03 * sin(time_step * 2.0 + phase[i]);
        r[i] += puls;
        r[i] = fmax(5.0, fmin(r_cap, r[i] * 0.998));
        theta[i] += omega[i] + 0.008 * sin(time_step * 0.7 + i * 0.5);

        // Elliptical orbit (Kepler)
        double orbit_r = r[i] * (1.0 - ecc[i] * ecc[i])
                       / (1.0 + ecc[i] * cos(theta[i]));
        double opx = cx + orbit_r * cos(theta[i]) * 2.0; // aspect correction
        double opy = cy + orbit_r * sin(theta[i]);

        // Body and glow
        fb_stamp((int)opx, (int)opy, 15);
        fb_stamp((int)opx - 1, (int)opy, 10);
        fb_stamp((int)opx + 1, (int)opy, 10);
        fb_stamp((int)opx, (int)opy - 1, 8);
        fb_stamp((int)opx, (int)opy + 1, 8);
    }

    fb_render();
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
