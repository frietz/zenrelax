/* zenrelax - Soothing nerdy terminal screensaver in pure C
 * Modes: 1-Plasma, 2-Julia Set, 3-Particles, 4-Quantum Flow, 5-Orbitals
 *        6-Rainfall, 7-Aurora, 8-Starfield, 9-Metaballs, 10-Game of Life
 * Usage: ./zenrelax [mode]  (no arg = random mode, 'q'/ESC/Ctrl+C to quit)
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

#define MAX_MODES 10
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

void get_term_size() {
    struct winsize w;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &w) == 0) {
        rows = w.ws_row;
        cols = w.ws_col;
    }
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (cols > MAX_COLS) cols = MAX_COLS;
}

void resize_handler(int sig) { (void)sig; resize_flag = 1; }
void int_handler(int sig) { (void)sig; running = 0; }

// --- Framebuffer for flicker-free particle modes ---
static unsigned char fb_int[MAX_ROWS][MAX_COLS];

void fb_clear() { memset(fb_int, 0, sizeof(fb_int)); }

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

// ===== Mode 1: Plasma Wave =====
void render_plasma() {
    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            double cx = (double)x / cols * 4 * PI;
            double cy = (double)y / rows * 4 * PI;
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

// ===== Mode 2: Julia Set Fractal =====
void render_mandelbrot() {
    double cr = -0.7269 + 0.18 * sin(time_step * 0.067);
    double ci =  0.1889 + 0.18 * cos(time_step * 0.053);
    double zoom = 2.0 + 0.8 * sin(time_step * 0.031);
    double aspect = 2.0;
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
                double abs_z = sqrt(zr * zr + zi * zi);
                double smooth = iter + 1.0 - log(log(abs_z)) / log2;
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

// ===== Mode 3: Particle Physics =====
#define NPART 128
static double px[NPART], py[NPART], pvx[NPART], pvy[NPART];

void init_particles() {
    for (int i = 0; i < NPART; i++) {
        px[i] = rand() % cols;
        py[i] = rand() % rows;
        pvx[i] = (rand() % 2000 - 1000) / 1000.0 * 0.5;
        pvy[i] = (rand() % 2000 - 1000) / 1000.0 * 0.5;
    }
}

void render_particles() {
    static int inited = 0;
    if (!inited) { init_particles(); fb_clear(); inited = 1; }

    fb_fade(1);

    double ax1 = cols / 2.0 + cols / 3.0 * sin(time_step * 0.13);
    double ay1 = rows / 2.0 + rows / 3.0 * cos(time_step * 0.11);
    double ax2 = cols / 2.0 + cols / 4.0 * cos(time_step * 0.09);
    double ay2 = rows / 2.0 + rows / 4.0 * sin(time_step * 0.17);
    double soft = 20.0;

    for (int i = 0; i < NPART; i++) {
        double dx1 = ax1 - px[i], dy1 = ay1 - py[i];
        double dx2 = ax2 - px[i], dy2 = ay2 - py[i];
        double d1_sq = dx1 * dx1 + dy1 * dy1 + soft * soft;
        double d2_sq = dx2 * dx2 + dy2 * dy2 + soft * soft;
        pvx[i] += dx1 / d1_sq * 3.0 + dx2 / d2_sq * 2.0;
        pvy[i] += dy1 / d1_sq * 3.0 + dy2 / d2_sq * 2.0;
        pvx[i] += sin(time_step + py[i] * 0.05) * 0.008;
        pvy[i] += cos(time_step * 1.3 + px[i] * 0.07) * 0.008;
        pvx[i] *= 0.97; pvy[i] *= 0.97;
        px[i] += pvx[i]; py[i] += pvy[i];

        if (px[i] < 0) px[i] += cols;
        if (px[i] >= cols) px[i] -= cols;
        if (py[i] < 0) py[i] += rows;
        if (py[i] >= rows) py[i] -= rows;

        double speed = sqrt(pvx[i] * pvx[i] + pvy[i] * pvy[i]);
        int intensity = 10 + (int)(fmin(speed * 5, 1.0) * 5);
        fb_stamp((int)px[i], (int)py[i], intensity);
    }
    fb_render();
}

// ===== Mode 4: Quantum Flow =====
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

            double v = sin(fx * 1.5 + t * 0.3) * cos(fy * 1.2 + t * 0.2)
                     + sin(fx * 3.0 + fy * 2.0 + t * 0.5) * 0.5
                     + cos(fx * 0.7 - t * 0.15) * sin(fy * 2.5 + t * 0.25) * 0.7
                     + sin((fx + fy) * 2.0 + t * 0.4) * 0.3;

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
                double angle = atan2(vdy, vdx);
                int dir = ((int)round(angle * 4.0 / PI) % 8 + 8) % 8;
                char dirs[] = "-\\|/-\\|/";
                ch = dirs[dir];
            } else {
                int di = (int)(density * 7.999);
                ch = " .~:;=!*"[di];
            }

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

// ===== Mode 5: Orbital Harmony =====
void render_orbitals() {
    #define N_ORBIT 12
    static double otheta[N_ORBIT], orr[N_ORBIT], oomega[N_ORBIT], ophase[N_ORBIT];
    static double oecc[N_ORBIT];
    static int inited = 0;
    if (!inited) {
        fb_clear();
        double max_r = fmin(rows, cols) / 2.5;
        for (int i = 0; i < N_ORBIT; i++) {
            orr[i] = 8 + max_r * (0.15 + 0.5 * i / (N_ORBIT - 1.0));
            oomega[i] = 0.18 / ((i % 4) + 1.5);
            ophase[i] = i * 2 * PI / N_ORBIT;
            otheta[i] = ophase[i];
            oecc[i] = 0.1 + 0.15 * (i % 3);
        }
        inited = 1;
    }

    fb_fade(1);
    double cx = cols / 2.0, cy = rows / 2.0;
    double r_cap = fmin(rows, cols) / 2.2;

    double glow_r = 3.0 + 1.5 * sin(time_step * 0.8);
    for (int dy = -(int)glow_r - 1; dy <= (int)glow_r + 1; dy++)
        for (int dx = -(int)(glow_r * 2) - 1; dx <= (int)(glow_r * 2) + 1; dx++) {
            double d = sqrt(dx * dx / 4.0 + dy * dy);
            if (d <= glow_r)
                fb_stamp((int)cx + dx, (int)cy + dy, (int)((1.0 - d / glow_r) * 15));
        }

    for (int i = 0; i < N_ORBIT; i++) {
        double puls = 0.03 * sin(time_step * 2.0 + ophase[i]);
        orr[i] += puls;
        orr[i] = fmax(5.0, fmin(r_cap, orr[i] * 0.998));
        otheta[i] += oomega[i] + 0.008 * sin(time_step * 0.7 + i * 0.5);

        double orbit_r = orr[i] * (1.0 - oecc[i] * oecc[i])
                       / (1.0 + oecc[i] * cos(otheta[i]));
        double opx = cx + orbit_r * cos(otheta[i]) * 2.0;
        double opy = cy + orbit_r * sin(otheta[i]);

        fb_stamp((int)opx, (int)opy, 15);
        fb_stamp((int)opx - 1, (int)opy, 10);
        fb_stamp((int)opx + 1, (int)opy, 10);
        fb_stamp((int)opx, (int)opy - 1, 8);
        fb_stamp((int)opx, (int)opy + 1, 8);
    }
    fb_render();
}

// ===== Mode 6: Rainfall =====
#define MAX_DROPS 150
#define MAX_RIPPLES 24
static double drop_x[MAX_DROPS], drop_y[MAX_DROPS], drop_speed[MAX_DROPS];
static double ripple_x[MAX_RIPPLES], ripple_r[MAX_RIPPLES], ripple_int[MAX_RIPPLES];

void render_rainfall() {
    static int inited = 0;
    if (!inited) {
        fb_clear();
        for (int i = 0; i < MAX_DROPS; i++) {
            drop_x[i] = rand() % cols;
            drop_y[i] = -(rand() % rows);
            drop_speed[i] = 0.3 + (rand() % 70) / 100.0;
        }
        memset(ripple_int, 0, sizeof(ripple_int));
        inited = 1;
    }

    fb_fade(2);

    for (int i = 0; i < MAX_DROPS; i++) {
        drop_y[i] += drop_speed[i];
        drop_x[i] += sin(time_step * 0.3 + drop_x[i] * 0.01) * 0.15;

        int ix = (int)drop_x[i], iy = (int)drop_y[i];
        int bright = 6 + (int)(drop_speed[i] * 12);
        if (bright > 15) bright = 15;

        fb_stamp(ix, iy, bright);
        fb_stamp(ix, iy - 1, bright * 2 / 3);
        fb_stamp(ix, iy - 2, bright / 3);

        if (drop_y[i] >= rows - 1) {
            for (int r = 0; r < MAX_RIPPLES; r++) {
                if (ripple_int[r] <= 0.05) {
                    ripple_x[r] = drop_x[i];
                    ripple_r[r] = 1.0;
                    ripple_int[r] = 1.0;
                    break;
                }
            }
            drop_y[i] = -(rand() % (rows / 2));
            drop_x[i] = rand() % cols;
            drop_speed[i] = 0.3 + (rand() % 70) / 100.0;
        }
    }

    int ry = rows - 1;
    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (ripple_int[r] > 0.05) {
            int rad = (int)ripple_r[r];
            int intensity = (int)(ripple_int[r] * 10);
            int rcx = (int)ripple_x[r];
            fb_stamp(rcx - rad, ry, intensity);
            fb_stamp(rcx + rad, ry, intensity);
            if (rad > 1) {
                fb_stamp(rcx - rad + 1, ry, intensity / 2);
                fb_stamp(rcx + rad - 1, ry, intensity / 2);
            }
            ripple_r[r] += 0.6;
            ripple_int[r] *= 0.88;
        }
    }
    fb_render();
}

// ===== Mode 7: Aurora Borealis =====
void render_aurora() {
    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            double fx = (double)x / cols;
            double fy = (double)y / rows;

            // Layered curtain sway
            double curtain = 0;
            for (int k = 1; k <= 5; k++) {
                double freq = k * 2.0;
                double phase = time_step * (0.1 + k * 0.03) + k * 1.7;
                curtain += sin(fx * freq * PI + phase) * (0.3 / k);
            }

            // Vertical falloff: bright at top, dim at bottom
            double vertical = exp(-fy * 2.5)
                             * (1.0 + 0.3 * sin(fy * PI * 3 + time_step * 0.2));

            // High-frequency shimmer
            double shimmer = 0.7 + 0.3 * sin(fx * 20 + fy * 5 + time_step * 2.0);

            double intensity = (curtain + 0.5) * vertical * shimmer;
            if (intensity < 0) intensity = 0;
            if (intensity > 1) intensity = 1;

            int ci = (int)(intensity * 12.999);
            if (ci > 12) ci = 12;
            char ch = " .,-~:;=!*#$@"[ci];

            double ct = fmod(fx * 0.3 + curtain * 0.5 + time_step * 0.02, 1.0);
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

// ===== Mode 8: Starfield =====
#define MAX_STARS 256
static double star_x[MAX_STARS], star_y[MAX_STARS], star_z[MAX_STARS];

void render_starfield() {
    static int inited = 0;
    if (!inited) {
        fb_clear();
        for (int i = 0; i < MAX_STARS; i++) {
            star_x[i] = (rand() % 2000 - 1000) / 100.0;
            star_y[i] = (rand() % 2000 - 1000) / 100.0;
            star_z[i] = 0.5 + (rand() % 200) / 10.0;
        }
        inited = 1;
    }

    fb_fade(3);

    double cx = cols / 2.0, cy = rows / 2.0;
    double scale = rows * 0.6;

    for (int i = 0; i < MAX_STARS; i++) {
        star_z[i] -= 0.08;

        if (star_z[i] <= 0.3) {
            star_x[i] = (rand() % 2000 - 1000) / 100.0;
            star_y[i] = (rand() % 2000 - 1000) / 100.0;
            star_z[i] = 15.0 + (rand() % 100) / 10.0;
        }

        double sx = cx + star_x[i] / star_z[i] * scale * 2.0;
        double sy = cy + star_y[i] / star_z[i] * scale;
        int ix = (int)sx, iy = (int)sy;

        int bright = (int)((1.0 - star_z[i] / 25.0) * 15);
        if (bright < 1) bright = 1;
        if (bright > 15) bright = 15;

        fb_stamp(ix, iy, bright);

        // Motion streak for close stars
        if (star_z[i] < 4.0) {
            double prev_z = star_z[i] + 0.08;
            int pix = (int)(cx + star_x[i] / prev_z * scale * 2.0);
            int piy = (int)(cy + star_y[i] / prev_z * scale);
            fb_stamp(pix, piy, bright / 2);
        }
    }
    fb_render();
}

// ===== Mode 9: Metaballs (Lava Lamp) =====
#define N_BLOBS 6
static double blob_x[N_BLOBS], blob_y[N_BLOBS];
static double blob_vx[N_BLOBS], blob_vy[N_BLOBS], blob_r[N_BLOBS];

void render_metaballs() {
    static int inited = 0;
    if (!inited) {
        for (int i = 0; i < N_BLOBS; i++) {
            blob_x[i] = cols * (0.2 + 0.6 * (rand() % 100) / 100.0);
            blob_y[i] = rows * (0.2 + 0.6 * (rand() % 100) / 100.0);
            blob_vx[i] = (rand() % 200 - 100) / 200.0;
            blob_vy[i] = (rand() % 200 - 100) / 200.0;
            blob_r[i] = 5.0;
        }
        inited = 1;
    }

    for (int i = 0; i < N_BLOBS; i++) {
        blob_vx[i] += sin(time_step * 0.3 + i * 2.0) * 0.02;
        blob_vy[i] += cos(time_step * 0.2 + i * 1.5) * 0.02;
        blob_vx[i] *= 0.99; blob_vy[i] *= 0.99;
        blob_x[i] += blob_vx[i]; blob_y[i] += blob_vy[i];
        if (blob_x[i] < 2 || blob_x[i] > cols - 2) blob_vx[i] *= -1;
        if (blob_y[i] < 2 || blob_y[i] > rows - 2) blob_vy[i] *= -1;
        blob_x[i] = fmax(1, fmin(cols - 1, blob_x[i]));
        blob_y[i] = fmax(1, fmin(rows - 1, blob_y[i]));
        blob_r[i] = 5.0 + 2.0 * sin(time_step * 0.5 + i * PI / 3);
    }

    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            double field = 0;
            for (int i = 0; i < N_BLOBS; i++) {
                double dx = (x - blob_x[i]) / 2.0; // aspect correction
                double dy = y - blob_y[i];
                double d_sq = dx * dx + dy * dy + 0.1;
                field += blob_r[i] * blob_r[i] / d_sq;
            }

            char ch;
            int color_idx;
            if (field < 0.5) {
                ch = ' ';
                color_idx = 0;
            } else if (field < 1.0) {
                double edge = (field - 0.5) * 2.0;
                ch = " .,:;+"[(int)(edge * 5.999)];
                color_idx = ((int)(edge * 8 + time_step * 0.5)) % 16;
            } else {
                double inner = fmin(field - 1.0, 2.0) / 2.0;
                ch = "*oO@"[(int)(inner * 3.999)];
                color_idx = ((int)(inner * 8 + 8 + time_step * 0.3)) % 16;
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

// ===== Mode 10: Game of Life =====
static unsigned char life_grid[MAX_ROWS][MAX_COLS];
static unsigned char life_age[MAX_ROWS][MAX_COLS];

void render_life() {
    static int inited = 0;
    static int frame = 0;

    if (!inited) {
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < cols; x++) {
                life_grid[y][x] = (rand() % 100 < 20) ? 1 : 0;
                life_age[y][x] = 0;
            }
        inited = 1;
    }

    // Evolve every 3 frames (~7 gen/sec at 20fps)
    if (frame % 3 == 0) {
        // Periodic random seeding to prevent extinction
        if (frame % 300 == 0) {
            int sx = rand() % cols, sy = rand() % rows;
            for (int dy = -5; dy <= 5; dy++)
                for (int dx = -5; dx <= 5; dx++)
                    if (rand() % 100 < 30) {
                        int nx = (sx + dx + cols) % cols;
                        int ny = (sy + dy + rows) % rows;
                        life_grid[ny][nx] = 1;
                        life_age[ny][nx] = 0;
                    }
        }

        static unsigned char next[MAX_ROWS][MAX_COLS];
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < cols; x++) {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        neighbors += life_grid[(y + dy + rows) % rows]
                                              [(x + dx + cols) % cols];
                    }
                if (life_grid[y][x])
                    next[y][x] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
                else
                    next[y][x] = (neighbors == 3) ? 1 : 0;
            }

        for (int y = 0; y < rows; y++)
            for (int x = 0; x < cols; x++) {
                if (next[y][x]) {
                    if (life_grid[y][x])
                        life_age[y][x] = life_age[y][x] < 15 ? life_age[y][x] + 1 : 15;
                    else
                        life_age[y][x] = 0;
                }
                life_grid[y][x] = next[y][x];
            }
    }

    // Render
    for (int y = 0; y < rows; y++) {
        char buf[MAX_COLS * 16 + 32];
        int pos = 0;
        int prev_color = -1;
        pos += sprintf(buf + pos, "\x1b[%d;1f", y + 1);
        for (int x = 0; x < cols; x++) {
            char ch;
            int color_idx;
            if (life_grid[y][x]) {
                int age = life_age[y][x];
                color_idx = 15 - age;
                if (color_idx < 1) color_idx = 1;
                ch = age < 2 ? '@' : age < 5 ? '#' : age < 10 ? '*' : '.';
            } else {
                ch = ' ';
                color_idx = 0;
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
    frame++;
}

// ===== Mode dispatch =====
void render_mode(int m) {
    switch (m) {
        case 1:  render_plasma(); break;
        case 2:  render_mandelbrot(); break;
        case 3:  render_particles(); break;
        case 4:  render_quantum_flow(); break;
        case 5:  render_orbitals(); break;
        case 6:  render_rainfall(); break;
        case 7:  render_aurora(); break;
        case 8:  render_starfield(); break;
        case 9:  render_metaballs(); break;
        case 10: render_life(); break;
    }
}

int main(int argc, char **argv) {
    srand(time(NULL));
    setvbuf(stdout, NULL, _IOFBF, 8192);
    printf(HIDE_CURSOR);
    get_term_size();

    signal(SIGWINCH, resize_handler);
    signal(SIGINT, int_handler);

    const char *mode_names[] = {
        NULL,
        "Plasma Wave", "Fractal Mandelbrot", "Particle Physics",
        "Quantum Flow", "Orbital Harmony", "Rainfall",
        "Aurora Borealis", "Starfield", "Metaballs", "Game of Life"
    };

    if (argc > 1) {
        mode = atoi(argv[1]);
        if (mode < 1 || mode > MAX_MODES) mode = DEFAULT_MODE;
    } else {
        mode = (rand() % MAX_MODES) + 1;
    }

    printf("\x1b[?1049h" CLEAR_SCREEN MOVE_HOME);
    printf("ZenRelax Mode %d: %s - Press 'q' or ESC to quit\n",
           mode, mode_names[mode]);
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
        struct timeval timeout = {0, 50000};
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int activity = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (activity > 0) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                if (ch == 'q' || ch == 'Q' || ch == 27)
                    running = 0;
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSADRAIN, &oldt);
    printf("\x1b[?1049l" SHOW_CURSOR CLEAR_SCREEN MOVE_HOME);
    return 0;
}
