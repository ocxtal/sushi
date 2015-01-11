
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include "ncurses.h"
#include <stdarg.h>

#define DEFAULT_SUSHI_LEN	( 5 )
#define SUSHI_XSIZE 		( 80 )
#define SUSHI_YSIZE 		( 24 )
#define SUSHI_XWINDOW 		( 40 )
#define SUSHI_YWINDOW 		( 22 )

enum sushi_ret { SUSHI_SUCCESS, SUSHI_QUIT, SUSHI_GAME_OVER };
enum sushi_key { SUSHI_KEY_RIGHT, SUSHI_KEY_LEFT, SUSHI_KEY_DOWN, SUSHI_KEY_UP };
enum sushi_dir { SUSHI_RIGHT, SUSHI_LEFT , SUSHI_DOWN, SUSHI_UP};

struct sushi_pos {
	int x, y;
	int attr;					// 魚の属性を格納。寿司には使わない
};

struct sushi_view {
	struct sushi_pos org;
	struct sushi_pos wsize, gvsize;
};

struct sushi_game {
	char sushi[10], fish[50][10], eraser[10];
	int width, height;
	int sushi_len, sushi_max, dir;
	struct sushi_pos *sushi_pos;
	double fish_per_call;
	int fish_cnt, fish_max, fish_attr_cnt;
	struct sushi_pos *fish_pos;
};

struct sushi_ctx {
	struct sushi_view *sv;
	struct sushi_game *sg;
};

/*
 * 画面描画関連 cursesはここからアクセス
 */
struct sushi_view *sushi_init_view()
{
	struct sushi_view *sv;

	/*
	 * cursesの初期化
	 */
	setlocale(LC_ALL, "");		// UTF-8文字列を正しく表示するため
	if(initscr() == NULL) {
		return NULL;
	}
	curs_set(0);
	keypad(stdscr, TRUE);
	timeout(0);					// getcがすぐにリターンするように

	sv = (struct sushi_view *)malloc(sizeof(struct sushi_view));
	if(sv == NULL) {
		return NULL;
	}
	sv->org.x = (COLS - SUSHI_XSIZE) / 2;
	sv->org.y = (LINES - SUSHI_YSIZE) / 2;
	sv->wsize.x = SUSHI_XSIZE;
	sv->wsize.y = SUSHI_YSIZE;
	sv->gvsize.x = SUSHI_XWINDOW;
	sv->gvsize.y = SUSHI_YWINDOW;
	return sv;
}

/*
 * 画面 (80 * 24)の中で指定
 */
int sushi_printf(struct sushi_view *sv, int x, int y, char const *fmt, ...)
{
	char str[100];
	va_list arg;
	va_start(arg, fmt);
	vsprintf(str, fmt, arg);
	va_end(arg);
	move(sv->org.y + y, sv->org.x + x);
	addstr(str);
	return SUSHI_SUCCESS;
}

/*
 * ゲーム画面 (40 * 20)の中で指定
 */
int sushi_emoji(struct sushi_view *sv, int x, int y, char const *str)
{
	move(sv->org.y + y, sv->org.x + 2*x);
	addstr(str);
	return SUSHI_SUCCESS;
}

int sushi_draw_game_over(struct sushi_view *sv, int len)
{
	char const *str_game_over = "GAME OVER";

	/* ここにゲームオーバーの画面を表示 */
	sushi_printf(sv,
		(sv->wsize.x - strlen(str_game_over)) / 2, 10,
		str_game_over);
	sushi_printf(sv,
		(sv->wsize.x - 8) / 2, 11,
		"%3d 🍣 s", len);
	refresh();
	usleep(3000000);
	return SUSHI_GAME_OVER;
}

int sushi_draw_game_base(struct sushi_view *sv)
{
	if((COLS > sv->wsize.x+2) && (LINES > sv->wsize.y+2)) {
		move(sv->org.y-1, sv->org.x-1);
		addch('+');
		move(sv->org.y-1, sv->org.x);
		hline('-', sv->wsize.x);
		move(sv->org.y-1, sv->org.x + sv->wsize.x);
		addch('+');
		move(sv->org.y + sv->wsize.y, sv->org.x-1);
		addch('+');
		move(sv->org.y + sv->wsize.y, sv->org.x);
		hline('-', sv->wsize.x);
		move(sv->org.y + sv->wsize.y, sv->org.x + sv->wsize.x);
		addch('+');

		move(sv->org.y, sv->org.x-1);
		vline('|', sv->wsize.y);
		move(sv->org.y, sv->org.x + sv->wsize.x);
		vline('|', sv->wsize.y);
	}
	move(sv->org.y + sv->gvsize.y, sv->org.x);
	hline('=', sv->wsize.x);
	refresh();
	return SUSHI_SUCCESS;
}

int sushi_redraw_window(struct sushi_view *sv, struct sushi_game *sg)
{
	char const *fish[30] = {"🐟🐟🐟", "🍚🍚🍚"};

	/* 魚の再描画 (末尾の魚のみ描画) */
	sushi_emoji(sv,
		sg->fish_pos[sg->fish_cnt-1].x, sg->fish_pos[sg->fish_cnt-1].y,
		sg->fish[sg->fish_pos[sg->fish_cnt-1].attr]);

	/* 寿司の再描画 (食った魚は上書きする) */
	sushi_emoji(sv,
		sg->sushi_pos[0].x, sg->sushi_pos[0].y,
		sg->sushi);
	sushi_emoji(sv,
		sg->sushi_pos[sg->sushi_len].x, sg->sushi_pos[sg->sushi_len].y,
		sg->eraser);
	sushi_printf(sv,
		0, sv->gvsize.y+1,
		"🍣 %d", sg->sushi_len);
	sushi_printf(sv,
		6, sv->gvsize.y+1,
		"%s", "🐟🐟🐟");
	sushi_printf(sv,
		14, sv->gvsize.y+1,
		"%s", "🍚🍚🍚");
	move(0, 0);
	refresh();

	return SUSHI_SUCCESS;
}

int sushi_get_key(struct sushi_view *sv)
{
	return getch();
}

/*
 * ゲーム関連
 * 寿司の位置を更新したり、ポイントを計算したりする
 */
struct sushi_game *sushi_init_game(char const *sushi, char const *fish[], int len, double fish_per_call)
{
	int i;
	char const *eraser = " ";
	struct sushi_game *sg;

	sg = (struct sushi_game *)malloc(sizeof(struct sushi_game));
	if(sg == NULL) {
		return NULL;
	}
//#define STRNCPY 	mbstowcs
#define STRNCPY 	strncpy

	STRNCPY(sg->sushi, sushi, 10);
	STRNCPY(sg->eraser, eraser, 10);
	sg->fish_attr_cnt = 0;
	while(*fish != NULL && sg->fish_attr_cnt < 50) {
		STRNCPY(sg->fish[sg->fish_attr_cnt++], *fish++, 10);
	}
#undef STRNCPY
	sg->width = SUSHI_XWINDOW;			// 寿司1つで半角2文字分を消費するので、x座標は2文字ごとに1進める
	sg->height = SUSHI_YWINDOW;

	// 寿司の位置の初期化 (最初は(0, 0)に縮重している)
	sg->sushi_len = len;				// 最初の寿司の長さは1
	sg->dir = SUSHI_RIGHT;
	sg->sushi_max = 50 * (sg->width + sg->height);
	sg->sushi_pos = (struct sushi_pos *)malloc((sg->sushi_max + 1) * sizeof(struct sushi_pos));
	if(sg->sushi_pos == NULL) {
		free(sg);
		return NULL;
	}
	sg->sushi_pos[0].x = sg->sushi_pos[0].y = sg->sushi_pos[0].attr = 0;
	for(i = 1; i < sg->sushi_max; i++) {
		sg->sushi_pos[i].x = sg->sushi_pos[i].y = -1;
		sg->sushi_pos[i].attr = 0;
	}

	// 魚の位置の初期化 (最初は魚はいない)
	sg->fish_per_call = fish_per_call;
	sg->fish_cnt = 0;
	sg->fish_max = (sg->width * sg->height) / 100;
	sg->fish_pos = (struct sushi_pos *)malloc((sg->fish_max + 1) * sizeof(struct sushi_pos));
	if(sg->fish == NULL) {
		free(sg->sushi_pos);
		free(sg);
		return NULL;
	}
	memset(sg->fish_pos, 0, (sg->fish_max + 1) * sizeof(struct sushi_pos));

	return(sg);
}

int sushi_update_dir(struct sushi_game *sg, int key)
{
	switch(key) {
		case KEY_RIGHT:
		case 'l':
			if(sg->dir != SUSHI_LEFT) { sg->dir = SUSHI_RIGHT; } break;
		case KEY_LEFT:
		case 'h':
			if(sg->dir != SUSHI_RIGHT) { sg->dir = SUSHI_LEFT; } break;
		case KEY_DOWN:
		case 'j':
			if(sg->dir != SUSHI_UP) { sg->dir = SUSHI_DOWN; } break;
		case KEY_UP:
		case 'k':
			if(sg->dir != SUSHI_DOWN) { sg->dir = SUSHI_UP; } break;
		case 'q': return SUSHI_QUIT;
		default: break;
	}
	return SUSHI_SUCCESS;
}

int sushi_update_fish(struct sushi_game *sg)
{
	if(((double)rand() / (double)RAND_MAX < sg->fish_per_call)
		&& (sg->fish_cnt < sg->fish_max)) {
		/* 魚を一つ増やす */
		sg->fish_pos[sg->fish_cnt].x = rand() % sg->width;
		sg->fish_pos[sg->fish_cnt].y = rand() % sg->height;
		sg->fish_pos[sg->fish_cnt].attr = rand() % sg->fish_attr_cnt;
		sg->fish_cnt++;
	}
	return SUSHI_SUCCESS;
}

int sushi_update_pos(struct sushi_game *sg)
{
	int i;

	/* 寿司を1つ進める */
#define CLIP(a, b) 			( ((a) % (b) < 0) ? (((a) % (b)) + (b)) : ((a) % (b)) )
	for(i = sg->sushi_len; i > 0; i--) {
		sg->sushi_pos[i] = sg->sushi_pos[i-1];
	}
	switch(sg->dir) {
		case SUSHI_RIGHT: 	sg->sushi_pos[0].x = CLIP(sg->sushi_pos[0].x+1, sg->width); break;
		case SUSHI_LEFT: 	sg->sushi_pos[0].x = CLIP(sg->sushi_pos[0].x-1, sg->width); break;
		case SUSHI_DOWN: 	sg->sushi_pos[0].y = CLIP(sg->sushi_pos[0].y+1, sg->height); break;
		case SUSHI_UP: 		sg->sushi_pos[0].y = CLIP(sg->sushi_pos[0].y-1, sg->height); break;
		default: break;
	}
#undef CLIP

	/* 自分との当たり判定 */
	for(i = 1; i < sg->sushi_len; i++) {
		if(sg->sushi_pos[0].x == sg->sushi_pos[i].x && sg->sushi_pos[0].y == sg->sushi_pos[i].y) {
			return SUSHI_GAME_OVER;
		}
	}

	/* 魚とのあたり判定 */
	for(i = 0; i < sg->fish_cnt; i++) {
		if(sg->sushi_pos[0].x == sg->fish_pos[i].x && sg->sushi_pos[0].y == sg->fish_pos[i].y) {
			/* 当たった */
			sg->sushi_len = (sg->sushi_len < sg->sushi_max) ? sg->sushi_len+1 : sg->sushi_len;
			sg->fish_cnt = (sg->fish_cnt > 0) ? sg->fish_cnt-1 : sg->fish_cnt;
			memcpy(&sg->fish_pos[i], &sg->fish_pos[sg->fish_cnt], sizeof(struct sushi_pos));
		}
	}
	return SUSHI_SUCCESS;
}

/*
 * 寿司コンテキスト
 */
struct sushi_ctx *sushi_init(char const *sushi, char const *fish[], int len, double fish_per_call)
{
	struct sushi_ctx *sc;

	/*
	 * sushi構造体の初期化
	 * posは 50*(width + height)を確保しておく。これだけあればたぶん十分
	 */
	sc = (struct sushi_ctx *)malloc(sizeof(struct sushi_ctx));
	if(sc == NULL) { return NULL; }
	/*
	 * sushi_viewの作成
	 */
	if((sc->sv = sushi_init_view()) == NULL) {
		free(sc);
	}
	sushi_draw_game_base(sc->sv);
	/*
	 * sushi_gameの作成
	 */
	if((sc->sg = sushi_init_game(sushi, fish, len, fish_per_call)) == NULL) {
		free(sc->sv);
		free(sc);
		return NULL;
	}
	return sc;
}

int sushi_proc(struct sushi_ctx *sc)
{
	/*
	 * 寿司を1つ進め、入力キーの処理を行う
	 *
	 * keyが矢印キーだった場合、それに従って寿司の方向を変化させる
	 *
	 * 【未実装】一定時間ごとにランダムな場所に魚とご飯を発生させる
	 *
	 * 【未実装】魚かご飯にぶつかると寿司が伸びる
	 */
	
	int key, ret = SUSHI_SUCCESS;

	/* キー入力により寿司の方向を変更 */
	key = sushi_get_key(sc->sv);
	if((ret = sushi_update_dir(sc->sg, key)) != SUSHI_SUCCESS) {
		goto _sushi_error_handler;
	}
	/* 魚を発生させる */
	if((ret = sushi_update_fish(sc->sg)) != SUSHI_SUCCESS) {
		goto _sushi_error_handler;
	}
	/* 位置の更新 */
	if((ret = sushi_update_pos(sc->sg)) != SUSHI_SUCCESS) {
		goto _sushi_error_handler;
	}
	/* 再描画 */
	if((ret = sushi_redraw_window(sc->sv, sc->sg)) != SUSHI_SUCCESS) {
		goto _sushi_error_handler;
	}
	return SUSHI_SUCCESS;

_sushi_error_handler:
	switch(ret) {
		case SUSHI_GAME_OVER:
			sushi_draw_game_over(sc->sv, sc->sg->sushi_len);
			break;
		default:
			break;
	}
	return key;
}

void sushi_close(struct sushi_ctx *sc)
{
	endwin();
	free(sc->sg->sushi_pos);
	free(sc->sg->fish_pos);
	free(sc->sg);
	free(sc->sv);
	free(sc);
	return;
}


int main(int argc, char *argv[])
{
	int result;
	int len = DEFAULT_SUSHI_LEN;
	long tick = 100000;
	double fish_per_call = 0.1;
	char const *sushi = "🍣";
	char const *fish[] = {"🐟", "🍚", NULL};
	struct sushi_ctx *sc;

	while((result = getopt(argc, argv, "s:l:f:")) != -1) {
		switch(result) {
			case 's':
				tick = (long)((double)tick*10.0 / atof(optarg));
				break;
			case 'l':
				len = atoi(optarg);
				break;
			case 'f':
				fish_per_call = atof(optarg);
				break;
			default:
				break;
		}
	}

	srand(time(NULL));
	sc = sushi_init(sushi, fish, len, fish_per_call);
	if(sc == NULL) {
		fprintf(stderr, "memory allocation error\n");
		exit(1);
	}
	while(1) {
		if(sushi_proc(sc) != SUSHI_SUCCESS) {
			break;
		}
		usleep(tick);
	}
	sushi_close(sc);
	return 0;
}

#if 0
int main(int argc, char *argv[])
{
	int result;
	int const wait = 100000;
	char str[256];
	int x, y;

	strcpy(str, "🐡");
	while((result = getopt(argc, argv, "alR")) != -1) {
		switch(result) {
			case 'a':
				strcpy(str, "🐠");
				break;
			case 'l':
				strcpy(str, "🐟");
				break;
			case 'R':
				break;
			default:
				break;
		}
	}

	srand(time(NULL));
	setlocale(LC_ALL, "");
	initscr();

	y = rand() % LINES;
	for(x = COLS-1; x > 0; x--) {
		erase();
		move(y, x);
		addstr(str);
		move(0, 0);
		refresh();
		usleep(wait);

		y += ((rand() % 3) - 1);
		if(y < 0) { y = 0; }
		if(y >= LINES) { y = LINES-1; }

//		printf("%d, %d\n", x, y);
	}
	return 0;
}
#endif
