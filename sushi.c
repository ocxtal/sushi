
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>

enum sushi_key { SUSHI_KEY_RIGHT, SUSHI_KEY_LEFT, SUSHI_KEY_DOWN, SUSHI_KEY_UP };
enum sushi_dir { SUSHI_RIGHT, SUSHI_LEFT , SUSHI_DOWN, SUSHI_UP};

struct sushi_pos {
	int x, y;
};

struct sushi_ctx {
	char sushi[10], eraser[10];
	int width, height;
	int len;
	int hist_len;
	struct sushi_pos *hist;
	int dir;
};

struct sushi_ctx *sushi_init(char *sushi)
{
	int i;
	struct sushi_ctx *sc;

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
	strcpy(sc->sushi, sushi);
	strcpy(sc->eraser, "  ");
	printf("%s, %s\n", sushi, sc->sushi);
	sc->width = COLS/2;			// 寿司1つで半角2文字分を消費するので、x座標は2文字ごとに1進める
	sc->height = LINES;
	sc->len = 10;				// 最初の寿司の長さは1
	sc->hist_len = 50 * (sc->width + sc->height);
	sc->hist = (struct sushi_pos *)malloc(sc->hist_len * sizeof(struct sushi_pos));
	if(sc->hist == NULL) {
		free(sc);
		return NULL;
	}
	for(i = 0; i < sc->hist_len; i++) {
		sc->hist[i].x = sc->hist[i].y = 0;
	}
	sc->dir = SUSHI_RIGHT;

	return(sc);
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
	
	int i, key;

	/* キー入力により寿司の方向を変更 */
	switch(key = getch()) {
		case KEY_RIGHT: 	if(sc->dir != SUSHI_LEFT) { sc->dir = SUSHI_RIGHT; } break;
		case KEY_LEFT: 		if(sc->dir != SUSHI_RIGHT) { sc->dir = SUSHI_LEFT; } break;
		case KEY_DOWN: 		if(sc->dir != SUSHI_UP) { sc->dir = SUSHI_DOWN; } break;
		case KEY_UP: 		if(sc->dir != SUSHI_DOWN) { sc->dir = SUSHI_UP; } break;
		case 'q': return 1;
		default: break;
	}

	/* 寿司を1つ進める */
#define CLIP(a, b) 			( ((a) % (b) < 0) ? (((a) % (b)) + (b)) : ((a) % (b)) )
	for(i = sc->len; i > 0; i--) {
		sc->hist[i] = sc->hist[i-1];
	}
	switch(sc->dir) {
		case SUSHI_RIGHT: 	sc->hist[0].x = CLIP(sc->hist[0].x+1, sc->width); break;
		case SUSHI_LEFT: 	sc->hist[0].x = CLIP(sc->hist[0].x-1, sc->width); break;
		case SUSHI_DOWN: 	sc->hist[0].y = CLIP(sc->hist[0].y+1, sc->height); break;
		case SUSHI_UP: 		sc->hist[0].y = CLIP(sc->hist[0].y-1, sc->height); break;
		default: break;
	}
#undef CLIP
	
	/* 寿司の再描画 */
	move(sc->hist[0].y, 2 * sc->hist[0].x);
	addstr(sc->sushi);
	move(sc->hist[sc->len].y, 2 * sc->hist[sc->len].x);
	addstr(sc->eraser);
	move(0, 0);
	refresh();
	return 0;
}

void sushi_close(struct sushi_ctx *sc)
{
	endwin();
	free(sc->hist);
	free(sc);
	return;
}


int main(int argc, char *argv[])
{
	struct sushi_ctx *sc;

	sc = sushi_init("🍣");
	if(sc == NULL) {
		fprintf(stderr, "memory allocation error\n");
		exit(1);
	}
	while(1) {
		if(sushi_proc(sc) == 1) {
			break;
		}
		usleep(100000);
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
