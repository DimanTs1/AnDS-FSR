#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"

// ФУНКЦИЯ ДЛЯ ГЛАВНОЙ ЗОНЫ
// Алгоритм проверяет, попал ли пиксель внутрь нашего "кривого" многоугольника.
int is_in_main_strait(int x, int y) {
    // Координаты выписал из паинта ручками, расставлял вдоль берега
    static int px[] = {584, 530, 500, 500, 490, 465, 517, 540, 715, 750, 1100, 1120};
    static int py[] = {634, 550, 400, 310, 230, 223, 132, 0, 0, 170, 310, 632};
    int nvert = 12;
    int i, j, c = 0;
    for (i = 0, j = nvert - 1; i < nvert; j = i++) {
        // стандартная проверка: если луч пересекает грань нечетное число раз, то мы внутри.
        if (((py[i] > y) != (py[j] > y)) &&
            (x < (px[j] - px[i]) * (y - py[i]) / (py[j] - py[i]) + px[i]))
            c = !c;
    }
    return c;
}

// ДОПОЛНИТЕЛЬНАЯ ЗОНА (ВЕРХНИЙ ЛЕВЫЙ УГОЛ)
// Чтобы не считать материк, вынес этот кусок отдельно
int is_in_extra_zone(int x, int y) {
    static int ex[] = {303, 268, 418, 340};
    static int ey[] = {192, 0, 0, 165};
    int nvert = 4;
    int i, j, c = 0;
    for (i = 0, j = nvert - 1; i < nvert; j = i++) {
        if (((ey[i] > y) != (ey[j] > y)) &&
            (x < (ex[j] - ex[i]) * (y - ey[i]) / (ey[j] - ey[i]) + ex[i]))
            c = !c;
    }
    return c;
}

// Поиск точек метдом "зажигания"
// Сделал на массивах, чтобы программа не вылетала от рекурсии
int clear_and_count(unsigned char *bw, int start_x, int start_y, int w, int h) {
    int max_p = w * h; // Памяти берем с запасом
    int *st_x = (int *) malloc(max_p * sizeof(int));
    int *st_y = (int *) malloc(max_p * sizeof(int));
    int top = 0;
    // кладем первую точку
    st_x[top] = start_x;
    st_y[top] = start_y;
    top++;
    bw[start_y * w + start_x] = 0; // Сразу стираем, чтобы не считать дважды
    int size = 0;
    while (top > 0) {
        top--;
        int x = st_x[top], y = st_y[top];
        size++;
        // Проверяем соседей (8 направлений)
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h && bw[ny * w + nx] == 255) {
                    bw[ny * w + nx] = 0; // Стираем найденный пиксель
                    st_x[top] = nx;
                    st_y[top] = ny;
                    top++;
                }
            }
        }
    }
    free(st_x);
    free(st_y);
    return size;
}

int main() {
    unsigned int w, h;
    unsigned char *pic = NULL;
    // Грузим нашу картинку
    if (lodepng_decode32_file(&pic, &w, &h, "strait_image.png")) {
        printf("Gde kartinka? Polozhi strait_image.png v papku!");
        return 0;
    }
    unsigned char *bw = (unsigned char *) malloc(w * h);
    unsigned char *out = (unsigned char *) malloc(w * h * 4);
    // ПОРОГ ЯРКОСТИ. Поставил 75, потому что корабли в центре тускловаты, при этом все равно не все видны
    // Если понижать, то начнет считать шумы картинки на танкеры
    // Если поставить больше, то половина пропадет
    int threshold = 75;
    // ПЕРВЫЙ ПРОХОД: ищем точки
    // Оставляем только яркие, которые попали в наши зоны воды
    for (int i = 0; i < (int) (w * h); i++) {
        int x = i % w, y = i / w;
        out[i * 4 + 0] = pic[i * 4 + 0]; // Копируем оригинал для фона
        out[i * 4 + 1] = pic[i * 4 + 1];
        out[i * 4 + 2] = pic[i * 4 + 2];
        out[i * 4 + 3] = 255;

        if ((is_in_main_strait(x, y) || is_in_extra_zone(x, y)) && pic[i * 4] >= threshold) {
            bw[i] = 255; // Белый - подозрение на танкер
        } else {
            bw[i] = 0; // Черный - вода
        }
    }
    int tankers = 0;
    // ВТОРОЙ ПРОХОД: Считаем объекты
    for (int y = 0; y < (int) h; y++) {
        for (int x = 0; x < (int) w; x++) {
            if (bw[y * w + x] == 255) {
                int start_x = x, start_y = y;
                // Стираем пятно и смотрим, сколько в нем пикселей
                int spot_size = clear_and_count(bw, x, y, w, h);
                // ФИЛЬТР: Танкер - это точка от 1 до 15 пикселей
                // Если больше - значит зацепили какой-то текст или берег
                if (spot_size >= 1 && spot_size <= 15) {
                    tankers++;
                    // Рисуем красную точку 3х3 на итоговой картинке
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int py = start_y + dy, px = start_x + dx;
                            if (px >= 0 && px < (int) w && py >= 0 && py < (int) h) {
                                int p = (py * w + px) * 4;
                                out[p] = 255;
                                out[p + 1] = 0;
                                out[p + 2] = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    // Выводим результат и сохраняем картинку
    printf("Ura! Nashli stolko tankerov: %d", tankers);
    lodepng_encode32_file("result.png", out, w, h);
    free(pic);
    free(bw);
    free(out);
    return 0;
}
