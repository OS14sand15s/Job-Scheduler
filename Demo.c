#include "time.h"
#include <stdio.h>

void main()
{
  time_t timer,timerc;//time_t is equals to long
  int count=1;  
  struct tm *timeinfo;
  time(&timer);//系统开始的时间
  while(count < 8)
  {
     time(&timerc);
     if((timerc-timer)>=1)//每过1秒打印
     {
       printf("程序经过%d秒\n",count++);
         // printf("%d:%d:%d\n",localtime(&timerc)->tm_hour,localtime(&timerc)->tm_min,localtime(&timerc)->tm_sec);//DEBUG
       timer=timerc;
     }
  }
  
}
/*
struct tm
{
  int tm_sec;                   /* Seconds.     [0-60] (1 leap second) *
  int tm_min;                   /* Minutes.     [0-59] *
  int tm_hour;                  /* Hours.       [0-23] *
  int tm_mday;                  /* Day.         [1-31] *
  int tm_mon;                   /* Month.       [0-11] *
  int tm_year;                  /* Year - 1900.  *
  int tm_wday;                  /* Day of week. [0-6] *
  int tm_yday;                  /* Days in year.[0-365] *
  int tm_isdst;                 /* DST.         [-1/0/1]*

#ifdef  __USE_BSD
  long int tm_gmtoff;           /* Seconds east of UTC.  *
  __const char *tm_zone;        /* Timezone abbreviation.  *
#else
  long int __tm_gmtoff;         /* Seconds east of UTC.  *
  __const char *__tm_zone;      /* Timezone abbreviation.  *
#endif
};
*/
