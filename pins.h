/*
Definition of C011 <> RPI GPIO pins
bcm2835 lib uses 40 pin J8 connector pin numbering
*/

#include <bcm2835.h>

#define D0      RPI_V2_GPIO_P1_03    //BLACK     (GPIO bit 2)
#define D1      RPI_V2_GPIO_P1_05    //BROWN     (GPIO bit 3)
#define D2      RPI_V2_GPIO_P1_07    //RED       (GPIO bit 4)
#define D3      RPI_V2_GPIO_P1_29    //ORANGE    (GPIO bit 5)
#define D4      RPI_V2_GPIO_P1_31    //YELLOW    (GPIO bit 6)
#define D5      RPI_V2_GPIO_P1_26    //GREEN     (GPIO bit 7)
#define D6      RPI_V2_GPIO_P1_24    //BLUE      (GPIO bit 8)
#define D7      RPI_V2_GPIO_P1_21    //VIOLET    (GPIO bit 9)

#define RS0     RPI_V2_GPIO_P1_18    //GREY      (GPIO bit 24)
#define RS1     RPI_V2_GPIO_P1_19    //WHITE     (GPIO bit 10)
#define RESET   RPI_V2_GPIO_P1_13    //YELLOW    (GPIO bit 27)
#define CS      RPI_V2_GPIO_P1_22    //GREEN     (GPIO bit 25)
#define RW      RPI_V2_GPIO_P1_23    //BLUE      (GPIO bit 11)
#define ANALYSE RPI_V2_GPIO_P1_15    //VIOLET    (GPIO bit 22)

#define IN_INT  RPI_V2_GPIO_P1_32    //BROWN     (GPIO bit 12)
#define OUT_INT RPI_V2_GPIO_P1_33    //WHITE     (GPIO bit 13)


