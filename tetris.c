#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#define NCURSES_WIDECHAR 1
#include <curses.h>

#define PLAY_AREA_HEIGHT 18
#define PLAY_AREA_WIDTH 12
#define INITIAL_SPEED 20
#define NUM_TETROMINOS 7
const char *const tetrominos =
    ".X.."
    ".X.."
    ".X.."
    ".X.."

    ".X.."
    ".X.."
    ".XX."
    "...."

    "..X."
    "..X."
    ".XX."
    "...."

    "..X."
    ".XX."
    ".X.."
    "...."

    ".X.."
    ".XX."
    "..X."
    "...."

    "...."
    ".XX."
    ".XX."
    "...."

    ".X.."
    ".XX."
    ".X.."
    "....";
#define TETROMINO(t, x, y) tetrominos[16 * (t) + 4 * (y) + (x)]
const wchar_t *const textures[] = {
    L"  ",
    L"\u2B1B",
    L"\U0001F7E5",
    L"\U0001F7E6",
    L"\U0001F7E7",
    L"\U0001F7E8",
    L"\U0001F7E9",
    L"\U0001F7EA",
    L"\U0001F7EB"
};
#define KEYBIND_ROTATE_LEFT  '7'
#define KEYBIND_ROTATE_RIGHT '9'
#define KEYBIND_MOVE_LEFT    '4'
#define KEYBIND_MOVE_RIGHT   '6'
#define KEYBIND_ROTATE_180   '8'
#define KEYBIND_SOFT_DROP    '5'
#define KEYBIND_HARD_DROP    '2'
#define KEYBIND_SWAP_HOLD    '0'
#define KEYBIND_QUIT         KEY_END

typedef struct {
    int game_over;
    size_t tick_count;
    size_t speed;
    size_t piece_index;
    size_t hold_piece_index;
    size_t next_piece_index;
    size_t piece_x, piece_y, piece_rot;
    size_t score;
    char play_area[PLAY_AREA_HEIGHT * PLAY_AREA_WIDTH];
} gamestate_t;

void sleep_tick(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50000000;
    nanosleep(&ts, &ts);
}

void get_uv(size_t *u, size_t *v, size_t x, size_t y, size_t rot)
{
    *u = x;
    *v = y;
    if (rot == 1)
    {
        *u = 3 - y;
        *v = x;
    }
    else if (rot == 2)
    {
        *u = 3 - x;
        *v = 3 - y;
    }
    else if (rot == 3)
    {
        *u = y;
        *v = 3 - x;
    }
}

int fits(gamestate_t *state, int dx, int dy, int drot)
{
    size_t rot = (state->piece_rot + 4 + drot) % 4;
    for (size_t y = 0; y < 4; ++y)
        for (size_t x = 0; x < 4; ++x)
        {
            size_t u, v;
            get_uv(&u, &v, x, y, rot);
            if (TETROMINO(state->piece_index, u, v) == 'X'
                    && state->play_area[(state->piece_y + dy + y) * PLAY_AREA_WIDTH + state->piece_x + dx + x])
                return 0;
        }
    // TODO
    return 1;
}

void add_score(gamestate_t *state)
{
    size_t rows = 0;
    for (size_t y = 0; y < PLAY_AREA_HEIGHT - 1; ++y)
    {
        for (size_t x = 1; x < PLAY_AREA_WIDTH - 1; ++x)
            if (!state->play_area[y * PLAY_AREA_WIDTH + x])
                goto not_full_row;

        // full row
        ++rows;
        for (size_t i = y; i > 0; --i)
        {
            memcpy(
                    state->play_area + i * PLAY_AREA_WIDTH + 1,
                    state->play_area + (i - 1) * PLAY_AREA_WIDTH + 1,
                    PLAY_AREA_WIDTH - 2);
        }
not_full_row: {}
    }
    state->score += rows;
    if (rows == 4)
        state->score += 4;
}

void next_piece(gamestate_t *state)
{
    for (size_t y = 0; y < 4; ++y)
        for (size_t x = 0; x < 4; ++x)
        {
            size_t u, v;
            get_uv(&u, &v, x, y, state->piece_rot);
            if (TETROMINO(state->piece_index, u, v) == 'X')
                state->play_area[(state->piece_y + y) * PLAY_AREA_WIDTH + state->piece_x + x] = state->piece_index + 2;
        }

    add_score(state);

    state->piece_x = PLAY_AREA_WIDTH / 2 - 2;
    state->piece_y = 0;
    state->piece_rot = 0;
    state->piece_index = state->next_piece_index;
    state->next_piece_index = rand() % 7;
    if (state->hold_piece_index >= 8)
        state->hold_piece_index -= 8;

    if (!fits(state, 0, 0, 0))
        state->game_over = 1;
}

int try_move(gamestate_t *state, int dx, int dy, int drot)
{
    if (dy)
        state->tick_count = 0;
    if (fits(state, dx, dy, drot))
    {
        state->piece_x += dx;
        state->piece_y += dy;
        state->piece_rot += drot + 4;
        state->piece_rot %= 4;
        return 1;
    }
    else if (dy)
    {
        next_piece(state);
        return 0;
    }
    return 0;
}

int try_swap(gamestate_t *state)
{
    if (state->hold_piece_index < 7)
    {
        size_t hold_piece_index = state->piece_index;
        state->piece_x = PLAY_AREA_WIDTH / 2 - 2;
        state->piece_y = 0;
        state->piece_rot = 0;
        state->piece_index = state->hold_piece_index;
        state->hold_piece_index = hold_piece_index + 8;

        if (!fits(state, 0, 0, 0))
            state->game_over = 1;

        return 1;
    }
    else if (state->hold_piece_index == 7)
    {
        state->piece_x = PLAY_AREA_WIDTH / 2 - 2;
        state->piece_y = 0;
        state->piece_rot = 0;
        state->hold_piece_index = state->piece_index + 8;
        state->piece_index = state->next_piece_index;
        state->next_piece_index = rand() % 7;

        if (!fits(state, 0, 0, 0))
            state->game_over = 1;

        return 1;
    }
    return 0;
}

void draw_piece(size_t piece_index, size_t x, size_t y, size_t rot, char clear)
{
    for (size_t dy = 0; dy < 4; ++dy)
    {
        for (size_t dx = 0; dx < 4; ++dx)
        {
            size_t u, v;
            get_uv(&u, &v, dx, dy, rot);

            if (TETROMINO(piece_index, u, v) == 'X')
            {
                move(y + dy, 2 * (x + dx));
                addwstr(textures[piece_index + 2]);
            }
            else if (clear)
            {
                move(y + dy, 2 * (x + dx));
                addwstr(textures[0]);
            }
        }
    }
}

int main(void)
{
    srand(time(NULL));
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); nonl();
    intrflush(stdscr, FALSE); keypad(stdscr, TRUE); scrollok(stdscr, FALSE);
    nodelay(stdscr, TRUE); notimeout(stdscr, TRUE);

    /// configure play area
    gamestate_t state = {
        .game_over = 0,
        .tick_count = 0,
        .speed = INITIAL_SPEED,
        .piece_index = 0,
        .hold_piece_index = 8,
        .next_piece_index = 2,
        .piece_x = 3, .piece_y = 4, .piece_rot = 0,
        .score = 0,
        .play_area = { 0 }
    };
    for (size_t y = 0; y < PLAY_AREA_HEIGHT - 1; ++y)
        state.play_area[PLAY_AREA_WIDTH * y] = state.play_area[PLAY_AREA_WIDTH * y + PLAY_AREA_WIDTH - 1] = 1;
    for (size_t x = 0; x < PLAY_AREA_WIDTH; ++x)
        state.play_area[PLAY_AREA_WIDTH * (PLAY_AREA_HEIGHT - 1) + x] = 1;

    refresh();

    char buf[256];
    buf[255] = '\0';
    char need_redraw = 1;
    while (!state.game_over)
    {
        // input
        int c;
        while ((c = getch()) != ERR)
        {
            switch (c)
            {
                case KEYBIND_QUIT:
                    state.game_over = 1;
                    break;
                case KEYBIND_MOVE_LEFT:
                    if (try_move(&state, -1, 0, 0))
                        need_redraw = 1;
                    break;
                case KEYBIND_MOVE_RIGHT:
                    if (try_move(&state, 1, 0, 0))
                        need_redraw = 1;
                    break;
                case KEYBIND_ROTATE_LEFT:
                    if (try_move(&state, 0, 0, 1))
                        need_redraw = 1;
                    break;
                case KEYBIND_ROTATE_RIGHT:
                    if (try_move(&state, 0, 0, -1))
                        need_redraw = 1;
                    break;
                case KEYBIND_ROTATE_180:
                    if (try_move(&state, 0, 0, 2))
                        need_redraw = 1;
                    break;
                case KEYBIND_SOFT_DROP:
                    try_move(&state, 0, 1, 0);
                    need_redraw = 1;
                    break;
                case KEYBIND_HARD_DROP:
                    while (try_move(&state, 0, 1, 0)) {}
                    need_redraw = 1;
                    break;
                case KEYBIND_SWAP_HOLD:
                    if (try_swap(&state))
                        need_redraw = 1;
                    break;
                case ' ':
                    next_piece(&state);
                    need_redraw = 1;
                    break;
                default: {}
            }
        }

        // draw
        if (need_redraw)
        {
            // score
            snprintf(buf, 255, "Score: %ld", state.score);
            mvaddstr(0, 2 * PLAY_AREA_WIDTH + 2, buf);

            // play area
            for (size_t y = 0; y < PLAY_AREA_HEIGHT; ++y)
            {
                move(y, 0);
                for (size_t x = 0; x < PLAY_AREA_WIDTH; ++x)
                    addwstr(textures[(size_t) state.play_area[PLAY_AREA_WIDTH * y + x]]);
            }

            // current, next, hold
            draw_piece(state.piece_index, state.piece_x, state.piece_y, state.piece_rot, 0);
            mvaddstr(2, 2 * PLAY_AREA_WIDTH + 2, "Hold:");
            if (state.hold_piece_index < NUM_TETROMINOS)
                draw_piece(state.hold_piece_index, PLAY_AREA_WIDTH + 1, 3, 0, 1);
            else
                for (size_t y = 3; y < 3 + 4; ++y)
                    mvaddstr(y, 2 * PLAY_AREA_WIDTH + 1, "        ");
            mvaddstr(8, 2 * PLAY_AREA_WIDTH + 2, "Next:");
            draw_piece(state.next_piece_index, PLAY_AREA_WIDTH + 1, 9, 0, 1);

            move(PLAY_AREA_HEIGHT - 1, 2 * PLAY_AREA_WIDTH);

            refresh();
            need_redraw = 0;
        }

        // delay
        sleep_tick();
    }

    endwin();
    return 0;
}
