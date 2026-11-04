/****************************************************************************
 * apps/examples/ls_driver_test/test_uart2.c
 *
 * UART2 byte send/receive test for LoongArch 2K0300 (Hummingbird board).
 *
 * Function:
 *   - UART2 on GPIO44 (TX) / GPIO45 (RX), muxed to MAIN_FUNC (function=3)
 *   - Uses the kernel serial driver node /dev/ttyS2 (16550, 8N1, 115200)
 *   - Configure the port via termios, send a banner, then echo any received
 *     byte back to the sender and log it on the console
 *
 * Hardware:
 *   - UART2 controller base: 0x16100800 (registered as /dev/ttyS2 by
 *     ls2k0300_serial.c when CONFIG_LS2K0300_UART2 is enabled)
 *   - APB reference clock: 200 MHz (baud divisor handled by the driver)
 *
 * NOTE: Pinmux is asserted from the application layer via /dev/pinctrl0
 *       even though ls2k0300_bringup.c already sets it, because other
 *       drivers may overwrite the GPIO44/45 function selection during boot
 *       (the Skill mandates this double-layer pinmux setup for muxed pins).
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

#include <nuttx/arch.h>
#include <nuttx/pinctrl/pinctrl.h>
#include <nuttx/ioexpander/gpio.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UART2_DEVICE            "/dev/ttyS2"

/* Pin assignment: GPIO44=TX, GPIO45=RX, both MAIN_FUNC (mode 0x3). */

#define UART2_TX_GPIO           44
#define UART2_RX_GPIO           45
#define UART2_FUNCTION          3       /* LS_PINMUX_MODE_AS_MAIN_FUNC */

#define UART2_BAUD              B115200

/* Echo loop cap so the test exits on its own if nothing is connected. */

#define UART2_ECHO_MAX          64

/* KEY2 (GPIO86, active-low) is used to exit the echo loop early. */

#define KEY2_GPIO               86
#define KEY_DEBOUNCE_MS         20

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  uart2_pinctrl_setup(void);
static int  key2_open(void);
static bool key2_is_pressed(int fd);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: uart2_pinctrl_setup
 *
 * Description:
 *   Re-assert GPIO44/GPIO45 to MAIN_FUNC (function=3) for UART2 TX/RX from
 *   the application layer. The bringup pinmux may be overwritten by other
 *   drivers later in the boot sequence, so we set it again here.
 *
 ****************************************************************************/

static int uart2_pinctrl_setup(void)
{
  struct pinctrl_param_s param;
  int fd;
  int ret;

  printf("[UART2] Setting GPIO%d(TX)/GPIO%d(RX) to function %d...\n",
         UART2_TX_GPIO, UART2_RX_GPIO, UART2_FUNCTION);

  fd = open("/dev/pinctrl0", O_RDWR);
  if (fd < 0)
    {
      printf("[UART2] ERROR: open /dev/pinctrl0 failed: %d\n", errno);
      return -errno;
    }

  /* TX pin */

  param.pin = UART2_TX_GPIO;
  param.para.function = UART2_FUNCTION;
  ret = ioctl(fd, PINCTRLC_SETFUNCTION, (unsigned long)&param);
  if (ret < 0)
    {
      printf("[UART2] ERROR: SETFUNCTION GPIO%d failed: %d\n",
             UART2_TX_GPIO, errno);
      close(fd);
      return -errno;
    }

  /* RX pin */

  param.pin = UART2_RX_GPIO;
  param.para.function = UART2_FUNCTION;
  ret = ioctl(fd, PINCTRLC_SETFUNCTION, (unsigned long)&param);
  if (ret < 0)
    {
      printf("[UART2] ERROR: SETFUNCTION GPIO%d failed: %d\n",
             UART2_RX_GPIO, errno);
      close(fd);
      return -errno;
    }

  printf("[UART2] Pinmux OK (GPIO%d/GPIO%d -> func %d)\n",
         UART2_TX_GPIO, UART2_RX_GPIO, UART2_FUNCTION);
  close(fd);
  return OK;
}

/****************************************************************************
 * Name: key2_open
 *
 * Description:
 *   Open KEY2 (GPIO86) as an input pin so the echo loop can poll it to exit.
 *   KEY2 is active-low (pressed = 0). Reuses the shared pinctrl helper from
 *   test_common.c to select GPIO function on the pin.
 *
 ****************************************************************************/

static int key2_open(void)
{
  char path[32];
  int fd;
  int ret;

  ret = pinctrl_set_gpio_function(KEY2_GPIO);
  if (ret < 0)
    {
      printf("[UART2] ERROR: pinctrl for KEY2 GPIO%d failed\n", KEY2_GPIO);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", KEY2_GPIO);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[UART2] ERROR: open %s failed: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_INPUT_PIN);
  if (ret < 0)
    {
      printf("[UART2] ERROR: GPIOC_SETPINTYPE(INPUT) on GPIO%d: %d\n",
             KEY2_GPIO, errno);
      close(fd);
      return -errno;
    }

  return fd;
}

/****************************************************************************
 * Name: key2_is_pressed
 *
 * Description:
 *   Poll KEY2 with a simple falling-edge debounce. Returns true once on the
 *   transition from released (HIGH) to pressed (LOW).
 *
 ****************************************************************************/

static bool key2_is_pressed(int fd)
{
  bool value = true;

  ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&value);

  /* Active-low: pressed when value == false. */

  if (!value)
    {
      up_mdelay(KEY_DEBOUNCE_MS);
      value = true;
      ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&value);
      if (!value)
        {
          return true;
        }
    }

  return false;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_uart2
 *
 * Description:
 *   - Re-assert GPIO44/GPIO45 pinmux to UART2 main function
 *   - Open /dev/ttyS2 and configure 8N1 @ 115200 via termios
 *   - Transmit a banner string over UART2
 *   - Echo loop: every byte received on UART2 is echoed back and also
 *     reported on the console, so you can confirm round-trip with a
 *     USB-TTL connected to GPIO44(TX)/GPIO45(RX)
 *
 ****************************************************************************/

int test_uart2(void)
{
  static const char banner[] =
    "\r\n=== LS2K0300 UART2 Echo Test (GPIO44 TX / GPIO45 RX) ===\r\n"
    "Type characters, they will be echoed back.\r\n";
  struct termios tio;
  const char *p;
  char ch;
  int fd;
  int fd_key;
  int ret;
  int received = 0;
  bool key_exit = false;

  printf("[UART2] === UART2 Byte Send/Receive Test ===\n");
  printf("[UART2] Device: %s\n", UART2_DEVICE);
  printf("[UART2] Pins: GPIO%d(TX) / GPIO%d(RX), function %d\n",
         UART2_TX_GPIO, UART2_RX_GPIO, UART2_FUNCTION);
  printf("[UART2] KEY2 (GPIO%d) -> exit\n", KEY2_GPIO);
  printf("[UART2] Config: 8N1 @ 115200 baud\n\n");

  /* Step 1: pinmux (MUST do this before opening the UART). */

  ret = uart2_pinctrl_setup();
  if (ret < 0)
    {
      printf("[UART2] ERROR: pinctrl setup failed: %d\n", ret);
      return ret;
    }

  /* Step 2: open KEY2 so we can poll it to exit the echo loop. */

  fd_key = key2_open();
  if (fd_key < 0)
    {
      return fd_key;
    }

  /* Step 3: open the kernel serial device non-blocking so that read()
   *         returns EAGAIN when no byte is available, leaving the loop
   *         free to poll KEY2.
   */

  fd = open(UART2_DEVICE, O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      printf("[UART2] ERROR: open %s failed: %d\n", UART2_DEVICE, errno);
      printf("[UART2] Is CONFIG_LS2K0300_UART2 enabled in defconfig?\n");
      close(fd_key);
      return -errno;
    }

  printf("[UART2] %s opened, fd=%d\n", UART2_DEVICE, fd);

  /* Step 3: configure 8N1 @ 115200. Start from current attributes, then
   *         set raw mode (no input/output/local processing), 8 data bits,
   *         no parity, 1 stop bit, ignore modem lines, enable receiver.
   */

  if (tcgetattr(fd, &tio) != 0)
    {
      printf("[UART2] ERROR: tcgetattr failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  tio.c_iflag = 0;                 /* raw input */
  tio.c_oflag = 0;                 /* raw output */
  tio.c_lflag = 0;                 /* no echo / canonical */

  /* 8 data bits, no parity, 1 stop bit, ignore modem, enable RX. */

  tio.c_cflag &= ~CSIZE;
  tio.c_cflag |= CS8;
  tio.c_cflag &= ~(PARENB | CSTOPB);
  tio.c_cflag |= CLOCAL | CREAD;

  if (cfsetspeed(&tio, UART2_BAUD) != 0)
    {
      printf("[UART2] ERROR: cfsetspeed failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  if (tcsetattr(fd, TCSANOW, &tio) != 0)
    {
      printf("[UART2] ERROR: tcsetattr failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[UART2] termios configured (8N1 @ 115200)\n");

  /* Step 4: transmit the banner. */

  printf("[UART2] Sending banner (%d bytes)...\n", (int)sizeof(banner) - 1);
  for (p = banner; *p != '\0'; p++)
    {
      ssize_t n = write(fd, p, 1);
      if (n < 0)
        {
          printf("[UART2] ERROR: write failed: %d\n", errno);
          close(fd);
          return -errno;
        }

      /* Drain one byte at a time; wait for TX to drain so a slow
       * USB-TTL receiver does not overflow the driver's TX buffer.
       */

      tcdrain(fd);
    }

  printf("[UART2] Banner sent. Entering echo loop (send bytes to UART2).\n");
  printf("[UART2] Press KEY2 (GPIO%d) to exit.\n\n", KEY2_GPIO);

  /* Step 5: echo loop. Read one byte, echo it back over UART2, and also
   *         log it on the console. Poll KEY2 each iteration to exit early.
   *         Bounded so the test exits on its own even with no input.
   */

  while (received < UART2_ECHO_MAX)
    {
      /* Exit on KEY2 press. */

      if (key2_is_pressed(fd_key))
        {
          printf("[UART2] KEY2 pressed, exiting...\n");
          key_exit = true;
          break;
        }

      ssize_t n = read(fd, &ch, 1);
      if (n < 0)
        {
          if (errno == EAGAIN || errno == EINTR)
            {
              /* No byte available; yield and re-check KEY2. */

              up_mdelay(10);
              continue;
            }

          printf("[UART2] ERROR: read failed: %d\n", errno);
          break;
        }

      if (n == 0)
        {
          /* No data; yield and re-check KEY2. */

          up_mdelay(10);
          continue;
        }

      printf("[UART2] RX: 0x%02x ('%c')\n", (uint8_t)ch,
             (ch >= 32 && ch < 127) ? ch : '.');

      /* Echo back to the UART2 sender. */

      if (write(fd, &ch, 1) < 0)
        {
          printf("[UART2] ERROR: echo write failed: %d\n", errno);
          break;
        }

      /* Exit on CR / LF so the test terminates cleanly. */

      if (ch == '\r' || ch == '\n')
        {
          char nl = '\n';
          write(fd, &nl, 1);
          break;
        }

      received++;
    }

  printf("\n[UART2] Done. Received %d bytes. %s\n", received,
         key_exit ? "(exited via KEY2)" : "");
  close(fd);
  close(fd_key);
  return OK;
}
