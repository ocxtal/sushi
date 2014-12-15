
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>

enum sushi_ret { SUSHI_SUCCESS, SUSHI_QUIT, SUSHI_GAME_OVER };
enum sushi_key { SUSHI_KEY_RIGHT, SUSHI_KEY_LEFT, SUSHI_KEY_DOWN, SUSHI_KEY_UP };
enum sushi_dir { SUSHI_RIGHT, SUSHI_LEFT , SUSHI_DOWN, SUSHI_UP};

struct sushi_pos {
	int x, y;
	int attr;					// 魚の属性を格納。寿司には使わない
};

struct sushi_ctx {
	char const *sushi, *fish[50], *eraser;
	int width, height;
	int sushi_len, sushi_max, dir;
	struct sushi_pos *sushi_pos;
	double fish_per_call;
	int fish_cnt, fish_max, fish_attr_cnt;
	struct sushi_pos *fish_pos;
};

struct sushi_ctx *sushi_init(char const *sushi, char const *fish[], int len, double fish_per_call)
{
	int i;
	struct sushi_ctx *sc;
	char const *eraser = " ";

	/*
	 * cursesの初期化
	 */
	setlocale(LC_ALL, "");		// UTF-8文字列を正しく表示するため
	if(initscr() == NULL) {
		return NULL;
	}
	keypad(stdscr, TRUE);
	timeout(0);					// getcがすぐにリターンするように

	/*
	 * sushi構造体の初期化
	 * posは 50*(width + height)を確保しておく。これだけあればたぶん十分
	 */
	sc = (struct sushi_ctx *)malloc(sizeof(struct sushi_ctx));
	if(sc == NULL) { return NULL; }
	sc->sushi = sushi;
	sc->eraser = eraser;
	sc->fish_attr_cnt = 0;
	while(*fish != NULL && sc->fish_attr_cnt < 50) {
		sc->fish[sc->fish_attr_cnt++] = *fish++;
	}
	printf("%s, %s\n", sushi, sc->sushi);
	sc->width = COLS/2;			// 寿司1つで半角2文字分を消費するので、x座標は2文字ごとに1進める
	sc->height = LINES;

	// 寿司の位置の初期化 (最初は(0, 0)に縮重している)
	sc->sushi_len = len;				// 最初の寿司の長さは1
	sc->dir = SUSHI_RIGHT;
	sc->sushi_max = 50 * (sc->width + sc->height);
	sc->sushi_pos = (struct sushi_pos *)malloc((sc->sushi_max + 1) * sizeof(struct sushi_pos));
	if(sc->sushi_pos == NULL) {
		free(sc);
		return NULL;
	}
	sc->sushi_pos[0].x = sc->sushi_pos[0].y = sc->sushi_pos[0].attr = 0;
	for(i = 1; i < sc->sushi_max; i++) {
		sc->sushi_pos[i].x = sc->sushi_pos[i].y = -1;
		sc->sushi_pos[i].attr = 0;
	}

	// 魚の位置の初期化 (最初は魚はいない)
	sc->fish_per_call = fish_per_call;
	sc->fish_cnt = 0;
	sc->fish_max = (sc->width * sc->height) / 100;
	sc->fish_pos = (struct sushi_pos *)malloc((sc->fish_max + 1) * sizeof(struct sushi_pos));
	if(sc->fish == NULL) {
		free(sc->sushi_pos);
		free(sc);
		return NULL;
	}
	memset(sc->fish_pos, 0, (sc->fish_max + 1) * sizeof(struct sushi_pos));

	return(sc);
}

int sushi_show_game_over(struct sushi_ctx *sc)
{
	erase();
	/* ここにゲームオーバーの画面を表示 */

	refresh();
	usleep(1000000);
	return SUSHI_GAME_OVER;
}

int sushi_update_fish(struct sushi_ctx *sc)
{
	if(((double)rand() / (double)RAND_MAX < sc->fish_per_call)
		&& (sc->fish_cnt < sc->fish_max)) {
		/* 魚を一つ増やす */
		sc->fish_pos[sc->fish_cnt].x = rand() % sc->width;
		sc->fish_pos[sc->fish_cnt].y = rand() % sc->height;
		sc->fish_pos[sc->fish_cnt].attr = rand() % sc->fish_attr_cnt;
		sc->fish_cnt++;
	}
	return SUSHI_SUCCESS;
}

int sushi_update_pos(struct sushi_ctx *sc)
{
	int i;

	/* 寿司を1つ進める */
#define CLIP(a, b) 			( ((a) % (b) < 0) ? (((a) % (b)) + (b)) : ((a) % (b)) )
	for(i = sc->sushi_len; i > 0; i--) {
		sc->sushi_pos[i] = sc->sushi_pos[i-1];
	}
	switch(sc->dir) {
		case SUSHI_RIGHT: 	sc->sushi_pos[0].x = CLIP(sc->sushi_pos[0].x+1, sc->width); break;
		case SUSHI_LEFT: 	sc->sushi_pos[0].x = CLIP(sc->sushi_pos[0].x-1, sc->width); break;
		case SUSHI_DOWN: 	sc->sushi_pos[0].y = CLIP(sc->sushi_pos[0].y+1, sc->height); break;
		case SUSHI_UP: 		sc->sushi_pos[0].y = CLIP(sc->sushi_pos[0].y-1, sc->height); break;
		default: break;
	}
#undef CLIP

	/* 自分との当たり判定 */
	for(i = 1; i < sc->sushi_len; i++) {
		if(sc->sushi_pos[0].x == sc->sushi_pos[i].x && sc->sushi_pos[0].y == sc->sushi_pos[i].y) {
			sushi_show_game_over(sc);
			return SUSHI_GAME_OVER;
		}
	}

	/* 魚とのあたり判定 */
	for(i = 0; i < sc->fish_cnt; i++) {
		if(sc->sushi_pos[0].x == sc->fish_pos[i].x && sc->sushi_pos[0].y == sc->fish_pos[i].y) {
			/* 当たった */
			sc->sushi_len = (sc->sushi_len < sc->sushi_max) ? sc->sushi_len+1 : sc->sushi_len;
			sc->fish_cnt = (sc->fish_cnt > 0) ? sc->fish_cnt-1 : sc->fish_cnt;
			memcpy(&sc->fish_pos[i], &sc->fish_pos[sc->fish_cnt], sizeof(struct sushi_pos));
		}
	}
	return SUSHI_SUCCESS;
}

int sushi_redraw(struct sushi_ctx *sc)
{
	/* 魚の再描画 (末尾の魚のみ描画) */
	move(sc->fish_pos[sc->fish_cnt-1].y, 2 * sc->fish_pos[sc->fish_cnt-1].x);
	addstr(sc->fish[sc->fish_pos[sc->fish_cnt-1].attr]);

	/* 寿司の再描画 (食った魚は上書きする) */
	move(sc->sushi_pos[0].y, 2 * sc->sushi_pos[0].x);
	addstr(sc->sushi);
	move(sc->sushi_pos[sc->sushi_len].y, 2 * sc->sushi_pos[sc->sushi_len].x);
	addstr(sc->eraser);
	move(0, 0);
	refresh();

	return SUSHI_SUCCESS;
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
	switch(key = getch()) {
		case KEY_RIGHT:
		case 'l':
			if(sc->dir != SUSHI_LEFT) { sc->dir = SUSHI_RIGHT; } break;
		case KEY_LEFT:
		case 'h':
			if(sc->dir != SUSHI_RIGHT) { sc->dir = SUSHI_LEFT; } break;
		case KEY_DOWN:
		case 'j':
			if(sc->dir != SUSHI_UP) { sc->dir = SUSHI_DOWN; } break;
		case KEY_UP:
		case 'k':
			if(sc->dir != SUSHI_DOWN) { sc->dir = SUSHI_UP; } break;
		case 'q': return SUSHI_QUIT;
		default: break;
	}
	/* 魚を発生させる */
	if((ret = sushi_update_fish(sc)) != SUSHI_SUCCESS) {
		return ret;
	}
	/* 位置の更新 */
	if((ret = sushi_update_pos(sc)) != SUSHI_SUCCESS) {
		return ret;
	}
	/* 再描画 */
	if((ret = sushi_redraw(sc)) != SUSHI_SUCCESS) {
		return ret;
	}
	return SUSHI_SUCCESS;
}

void sushi_close(struct sushi_ctx *sc)
{
	endwin();
	free(sc->sushi_pos);
	free(sc);
	return;
}


int main(int argc, char *argv[])
{
	int result;
	int len = 10;
	long tick = 100000;
	double fish_per_call = 0.1;
	char const *sushi = "🍣";
	char const *fish[] = {"🐟", "🍚", NULL};
	struct sushi_ctx *sc;

	while((result = getopt(argc, argv, "s:l:f:")) != -1) {
		switch(result) {
			case 's':
				tick = (long)((double)tick*10.0 / atof(optarg));
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
