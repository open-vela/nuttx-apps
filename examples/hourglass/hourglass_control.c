#include "hourglass_control.h"

#define ROWS 8
#define CLOUMS 8
#define MATRIX_SIZE 8

static void demo_start(lv_timer_t *timer1);
unsigned long iTimeout = 0;

extern struct post1{
    lv_obj_t *obj;
    bool state;
};

extern struct post1 (*matrix_top_ptr)[CLOUMS];
extern struct post1 (*matrix_bottom_ptr)[CLOUMS];

lv_timer_t *timer = NULL;

static void setLed(int addr, int row, int column, bool state) {

    if(addr<0 || addr>=2)
        return;
    if(row<0 || row>7 || column<0 || column>7)
        return;

    if (addr == 0)
    {
        if (state)
        {
            lv_obj_set_style_bg_color(matrix_top_ptr[row][column].obj, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
            matrix_top_ptr[row][column].state = true;
        }
        else
        {
            lv_obj_set_style_bg_color(matrix_top_ptr[row][column].obj, lv_color_white(), LV_PART_MAIN);
            matrix_top_ptr[row][column].state = false;
        }
    }
    else
    {
        if (state)
        {
            lv_obj_set_style_bg_color(matrix_bottom_ptr[row][column].obj, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
            matrix_bottom_ptr[row][column].state = true;
            LV_LOG_USER("%s %d, bottom led on x %d, y %d", __func__, __LINE__, row, column);
        }
        else
        {
            lv_obj_set_style_bg_color(matrix_bottom_ptr[row][column].obj, lv_color_white(), LV_PART_MAIN);
            matrix_bottom_ptr[row][column].state = false;
        }
    }
}

static bool getLed(int addr, int row, int column)
{
    if(addr<0 || addr>=2)
        return false;
    if(row<0 || row>7 || column<0 || column>7)
        return false;

    if (addr == 0)
    {
        return matrix_top_ptr[row][column].state;
    }
    return matrix_bottom_ptr[row][column].state;
}

static bool canGoLeft(int addr, int x, int y) {
  if (x == 0) return false; // not available
//   return !lc.getXY(addr, getLeft(x, y)); // you can go there if this is empty
    return !getLed(addr, x - 1, y);
}
static bool canGoRight(int addr, int x, int y) {
  if (y == 7) return false; // not available
//   return !lc.getXY(addr, getRight(x, y)); // you can go there if this is empty
    return !getLed(addr, x, y + 1);
}
static bool canGoDown(int addr, int x, int y) {
  if (y == 7) return false; // not available
  if (x == 0) return false; // not available
  if (!canGoLeft(addr, x, y)) return false;
  if (!canGoRight(addr, x, y)) return false;
//   return !lc.getXY(addr, getDown(x, y)); // you can go there if this is empty
    return !getLed(addr, x - 1, y + 1);
}

static void goDown(int addr, int x, int y) {
//   lc.setXY(addr, x, y, false);
//   lc.setXY(addr, getDown(x, y), true);
    setLed(addr, x, y, false);
    setLed(addr, x - 1, y + 1, true);
}
static void goLeft(int addr, int x, int y) {
//   lc.setXY(addr, x, y, false);
//   lc.setXY(addr, getLeft(x, y), true);
    setLed(addr, x, y, false);
    setLed(addr, x - 1, y, true);
}
static void goRight(int addr, int x, int y) {
//   lc.setXY(addr, x, y, false);
//   lc.setXY(addr, getRight(x, y), true);
    setLed(addr, x, y, false);
    setLed(addr, x, y + 1, true);
}

static void checking(int addr)
{
    if (!getLed(addr, 5, 2))
    {
        setLed(addr, 5, 2, true);
        // usleep(800 * 1000);
        for (int i = 0; i < 100000; i++)
        {

        }
        LV_LOG_USER("%s %d, getLed is no", __func__, __LINE__);
        setLed(addr, 5, 2, false);
    }
}

static bool moveParticle(int addr, int x, int y) {

  bool can_GoLeft = canGoLeft(addr, x, y);
  bool can_GoRight = canGoRight(addr, x, y);

  if (!can_GoLeft && !can_GoRight) {
    return false; // we're stuck
  }
    checking(addr);
  bool can_GoDown = canGoDown(addr, x, y);
  if (can_GoDown) {
    goDown(addr, x, y);
  } else if (can_GoLeft && !can_GoRight) {
    goLeft(addr, x, y);
  } else if (can_GoRight && !can_GoLeft) {
    goRight(addr, x, y);
  } else if (random() % 2 == 1) { // we can go left and right, but not down
    goLeft(addr, x, y);
  } else {
    goRight(addr, x, y);
  }
  return true;
}

/**
   Traverse matrix and check if particles need to be moved
*/
static bool updateMatrix(void) {
  int n = 8;
  bool somethingMoved = false;
  int x, y;
  bool direction;
  for (int slice = 0; slice < 2 * n - 1; ++slice) {
    direction = (random() % 2 == 1); // randomize if we scan from left to right or from right to left, so the grain doesn't always fall the same direction
    int z = slice < n ? 0 : slice - n + 1;
    for (int j = z; j <= slice - z; ++j) {
      y = direction ? (7 - j) : (7 - (slice - j));
      x = direction ? (slice - j) : j;
      if (moveParticle(1, x, y)) {
        somethingMoved = true;

        goto find;
      }
      else
      {
        LV_LOG_USER("%s %d, moveParticle 1 is no", __func__, __LINE__);
      }
    }
  }

find:

  return somethingMoved;
}
/**
   Let a particle go from one matrix to the other
*/
extern int time_value;

// 重置沙漏
void reset_sand_hourglass(void) {
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    // 重置上方矩阵 (全亮)
    for (uint8_t y = 0; y < MATRIX_SIZE; y++) {
        for (uint8_t x = 0; x < MATRIX_SIZE; x++) {
            if ((x == 0 || x == 1) && (y == 6 || y == 7))
            {
                lv_obj_set_style_bg_color(matrix_top_ptr[y][x].obj, lv_color_white(), LV_PART_MAIN);
                matrix_top_ptr[y][x].state = false;
            }
            else
            {
                lv_obj_set_style_bg_color(matrix_top_ptr[y][x].obj, lv_palette_main(LV_PALETTE_RED), 0);
                matrix_top_ptr[y][x].state = true;
            }
        }
    }

    // 重置下方矩阵 (全暗)
    for (uint8_t y = 0; y < MATRIX_SIZE; y++) {
        for (uint8_t x = 0; x < MATRIX_SIZE; x++) {
            lv_obj_set_style_bg_color(matrix_bottom_ptr[y][x].obj, lv_color_white(), 0);
            matrix_bottom_ptr[y][x].state = 0;
        }
    }

    // 重新创建定时器
    timer = lv_timer_create(demo_start, time_value * 1000, NULL);
}

// 尝试移动沙粒（优化后的对角线扫描+随机方向）
static uint8_t move_sand_particle(uint8_t from_top) {
    uint8_t something_moved = 0;
    uint8_t x, y;
    uint8_t direction;
    LV_LOG_USER("%s %d", __func__, __LINE__);
    // 对角线扫描（类似您提供的算法）
    for (uint8_t slice = 0; slice < 2 * ROWS - 1; ++slice) {
        direction = (rand() % 2); // 随机扫描方向

        uint8_t z = slice < ROWS ? 0 : slice - ROWS + 1;
        for (uint8_t j = z; j <= slice - z; ++j) {
            y = direction ? (ROWS - 1 - j) : (ROWS - 1 - (slice - j));
            x = direction ? (slice - j) : j;

            // 只在上矩阵寻找亮起的元素，或在下矩阵寻找暗的元素
            if ((from_top && matrix_top_ptr[y][x].state) || (!from_top && !matrix_bottom_ptr[y][x].state)) {
                // 检查边界
                if (x >= 0 && x < ROWS && y >= 0 && y < ROWS) {
                    // 上矩阵：尝试移动亮起的元素
                    if (from_top && matrix_top_ptr[y][x].state) {
                        matrix_top_ptr[y][x].state = 0;
                        lv_obj_set_style_bg_color(matrix_top_ptr[y][x].obj, lv_color_white(), 0);
                        something_moved = 1;
                        return something_moved; // 每次只移动一粒沙子
                    }
                    // 下矩阵：尝试填充暗的元素
                    else if (!from_top && !matrix_bottom_ptr[y][x].state) {
                        matrix_bottom_ptr[y][x].state = 1;
                        lv_obj_set_style_bg_color(matrix_bottom_ptr[y][x].obj, lv_palette_main(LV_PALETTE_RED), 0);
                        something_moved = 1;
                        return something_moved; // 每次只填充一粒沙子
                    }
                }
            }
        }
    }

    return something_moved;
}

static void demo_start(lv_timer_t *timer1)
{
    LV_LOG_USER("%s %d, Clicked", __func__, __LINE__);
    // 1. 从上矩阵移动一粒沙子（随机对角线扫描）
    uint8_t moved = move_sand_particle(1);
    // 2. 如果成功移除了沙子，在下矩阵添加一粒沙子
    if (moved) {
        // move_sand_particle(0);
        updateMatrix();
    }
    // 3. 检查上矩阵是否还有沙子
    uint8_t has_sand = 0;
    for (uint8_t y = 0; y < ROWS && !has_sand; y++) {
        for (uint8_t x = 0; x < ROWS && !has_sand; x++) {
            if (matrix_top_ptr[y][x].state) has_sand = 1;
        }
    }
    if (!has_sand) {
        if (timer){
            lv_timer_del(timer);
            timer = NULL;
        }
    }
}


