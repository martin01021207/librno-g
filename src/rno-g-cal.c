
#define _GNU_SOURCE
#include "rno-g-cal.h" 

#include <linux/i2c-dev.h> 
#include <linux/i2c.h> 
#include <sys/ioctl.h> 

#include <stdio.h>
#include <errno.h>
#include <stdlib.h> 
#include <ctype.h>
#include <string.h> 
#include <unistd.h> 
#include <fcntl.h> 

struct rno_g_cal_dev
{
  int fd; 
  int bus; 
  char rev; 
  int debug; 
  uint8_t addr; 
  rno_g_cal_channel_t ch; 
  FILE * fgpiodir;
}; 

const char valid_revs[] = "DE"; 

const uint8_t addr0 = 0x38; 
const uint8_t addr1 = 0x3f; 
const uint8_t addr2 = 0x18; //temp sensor
const uint8_t output_reg = 0x01; 
const uint8_t config_reg = 0x03; 
const uint8_t tmp_reg = 0x05; 

const char gpio_dir_format[] = "/sys/class/gpio/gpio%hu/direction"; 


rno_g_cal_dev_t * rno_g_cal_open(uint8_t bus, uint16_t gpio, char rev) 
{
  char fname[20]; 

  if (!rev) //assume revE?
  {
    rev = 'E'; 
  }
  else
  {
    rev = toupper(rev);
  }

  if (!strchr(valid_revs,rev))
  {
    fprintf(stderr,"rev%c is not valid!\n", rev); 
    return NULL; 
  }

  // make sure we have the gpio exported
  char * gpio_dir = 0; 
  asprintf(&gpio_dir,gpio_dir_format, gpio); 
 
// lazy code 
  if (access(gpio_dir, W_OK))
  {
    FILE * f = fopen("/sys/class/gpio/export","w"); 
    fprintf(f,"%d\n", gpio); 
    fclose(f); 
    usleep(50000); // long enough? 
  }
  snprintf(fname,sizeof(fname)-1, "/dev/i2c-%hhu", bus); 

  int fd = open(fname, O_RDWR); 

  if (fd < 0) 
  {
    fprintf(stderr,"Could not open %s.\n", fname); 
    perror("  Reason: ");
    return NULL; 
  }

  rno_g_cal_dev_t * dev = calloc(sizeof(rno_g_cal_dev_t),1); 



  //open the pgio file
  FILE * fgpio = fopen(gpio_dir,"w"); 
  if (!fgpio) 
  {
    fprintf(stderr, "Could not open %s for writing\n", gpio_dir); 
    close(fd); 
    free(gpio_dir); 
    return NULL; 
  }

  free(gpio_dir); 

  if (!dev) 
  {
    fprintf(stderr,"Could not allocate memory for rno_g_cal_dev_t!!!\n"); 
    fclose(fgpio); 
    close(fd); 
    return NULL; 
  }



  dev->fgpiodir = fgpio; 
  dev->fd = fd; 
  dev->rev = rev; 
  dev->bus = bus; 

  return dev; 
}

int rno_g_cal_close(rno_g_cal_dev_t * dev) 
{
  if (!dev) return -1; 
  fclose(dev->fgpiodir); 
  close(dev->fd); 
  free(dev); 
  return 0; 
}

int rno_g_cal_enable(rno_g_cal_dev_t * dev) 
{
  return fprintf(dev->fgpiodir,"high\n") != sizeof("high\n");
}

int rno_g_cal_disable(rno_g_cal_dev_t * dev) 
{
  return fprintf(dev->fgpiodir,"low\n") != sizeof("low\n");
}



static int do_write(rno_g_cal_dev_t * dev, uint8_t addr, uint8_t reg, uint8_t val) 
{

  uint8_t buf[2] = {reg, val}; 
  struct i2c_msg msg = 
  {
    .addr = addr, 
    .flags = 0, 
    .len = sizeof(buf), 
    .buf = buf 
  }; 

  if (dev->debug) 
  {
    printf("DBG: do_write(.addr=0x%02x, .flags=0x%02x, .len=%02x, .buf={0x%02x,0x%02x}\n", msg.addr, msg.flags, msg.len, msg.buf[0], msg.buf[1]); 
  }

  struct i2c_rdwr_ioctl_data i2c_data = {.msgs = &msg, .nmsgs = 1 }; 

  if (ioctl(dev->fd, I2C_RDWR, &i2c_data) < 0 )
  {
    return -errno; 
  }

  return 0; 
}


static int do_readv(rno_g_cal_dev_t * dev, uint8_t addr, uint8_t reg, int N, uint8_t* data)
{

  struct i2c_msg txn[2] = { 
    {.addr = addr, .flags = 0, .len = sizeof(reg), .buf = &reg},
    {.addr = addr, .flags = I2C_M_RD, .len = N, .buf = data} }; 

  if (dev->debug) 
  {
    printf("DBG: do_readv(.addr=0x%02x, .reg=0x%02x, .len=%d)\n", addr, reg, N); 

  }
  struct i2c_rdwr_ioctl_data i2c_data = {.msgs = txn, .nmsgs = 2 }; 

  if (ioctl(dev->fd, I2C_RDWR, &i2c_data) < 0 )
  {
    return -errno; 
  }

  if (dev->debug) 
  {
    printf("  read: "); 
    for (int i = 0; i < N; i++) 
    {
      printf ("0x%02x ", data[i]); 
    }
    printf("\n"); 
  }

  return 0; 
}

static int do_read(rno_g_cal_dev_t *dev, uint8_t addr, uint8_t reg, uint8_t * val) 
{
  return do_readv(dev,addr,reg,1, val); 
}


int rno_g_cal_setup(rno_g_cal_dev_t* dev)
{
  if (do_write(dev, addr0, output_reg,0x00)) return -errno; 
  if (do_write(dev, addr0, config_reg,0x00)) return -errno; 
  if (do_write(dev, addr1, output_reg,0x00)) return -errno; 
  if (do_write(dev, addr1, config_reg,0x00)) return -errno; 
  return 0; 
}


static int do_set(rno_g_cal_dev_t *dev, int pulser) 
{
  uint8_t val; 
  if (do_read(dev, addr1, output_reg, &val)) 
  {
    return -errno; 
  }

  if (pulser) 
  {
    val|=0x40; //turn on pulser
    val&=(~0x08); //turn off VCO? 
  }
  else
  {
    val&=(~0x40); //turn off pulser
    val|=0x08; //turn on VCO? 
  }
                
                  
  if (do_write(dev,addr1,output_reg,val)) 
  {
      return -errno; 
  }
  return 0; 
}

int rno_g_cal_set_pulse(rno_g_cal_dev_t * dev) 
{
  return do_set(dev,1); 

}

int rno_g_cal_set_vco(rno_g_cal_dev_t* dev)
{
  return do_set(dev,0); 
}

int rno_g_cal_select(rno_g_cal_dev_t * dev, rno_g_cal_channel_t ch) 
{
  uint8_t val0; 
  uint8_t val1; 

  if (do_read(dev, addr0, output_reg, &val0)) 
    return -errno; 

  if (do_read(dev, addr1, output_reg, &val1)) 
    return -errno; 

  if (dev->rev == 'E') 
  {
    val1 |= 0xc; 
    if (do_write(dev, addr1, output_reg, val1))
      return -errno; 
  }

  if (ch == RNOG_CAL_CH_COAX)
  {
    val0 &= (~0x02); 
    val1 &= (~0x80); 
  }
  else if ( ch == RNOG_CAL_CH_FIBER0 ) 
  {
    if (dev->rev=='D')
    {
      val0 &= ~0x02; 
      val1 |= 0x80; 
    }
    else
    {
      val0 |= 0x02; 
      val1 &= 0x2f; 
      val1 &= ~0x04; 
    }
  }
  else if ( ch == RNOG_CAL_CH_FIBER1 ) 
  {
    if (dev->rev=='D')
    {
      val0 |= 0x02; 
      val1 |= 0x20; 
    }
    else
    {
      val0 |= 0x02; 
      val1 &= 0x2f; 
      val1 &= ~0x04; 
    }
  }
  else
  {
    //set all switches to 0? 
    val0 &= (~0x02); 
    val1 &= (~0x80); 
    val1 &= (~0x20); 
  }


  if (do_write(dev, addr0, output_reg, val0)) return -errno; 
  if (do_write(dev, addr1, output_reg, val1)) return -errno; 
  return 0; 
}

int rno_g_cal_set_atten(rno_g_cal_dev_t *dev, uint8_t atten) 
{
  //read addr0
  uint8_t val;
  if (do_read(dev, addr0, output_reg, &val))
  {
    return -errno; 
  }
  val &=0x02; 

  if (atten > 63) atten = 63; 
  atten = 63-atten; 

  if (dev->debug) printf("atten (inv): 0x%02x\n", atten); 

  //bit reverse 
  uint8_t reversed = (atten & 1) << 5 ; 
  reversed |=  ((atten & 2) >> 1 ) << 4; 
  reversed |=  ((atten & 4) >> 2)  << 3; 
  reversed |=  ((atten & 8) >> 3)  << 2; 
  reversed |=  ((atten & 16) >> 4)  << 1; 
  reversed |=  (atten & 32) >> 5; 
  if (dev->debug) printf("reversed: 0x%02x\n", reversed); 

  val |= (reversed << 2); 

  if (do_write(dev, addr0, output_reg, val)) return -errno; 
  if (do_write(dev, addr0, output_reg, val | 0x1)) return -errno; //load enable
  if (do_write(dev, addr0, output_reg, val)) return -errno; 

  return 0; 
}

int rno_g_cal_read_temp(rno_g_cal_dev_t *dev, float *Tout) 
{
  uint8_t data[2]; 

  if (do_readv(dev, addr2, tmp_reg, sizeof(data), data)) return -errno; 

  float T = data[1]/16.; 
  T+= (data[0] & 0xf) * 16; 

  if (data[0] & 0x10) 
    T-=256; 

  if (Tout) 
    *Tout = T; 

  return 0; 

}

void rno_g_cal_enable_dbg(rno_g_cal_dev_t * dev, int dbg) 
{
  dev->debug = dbg; 

}
