/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    Configure Lora concentrator and forward packets to a server
    Use GPS for packet timestamping.
    Send a becon at a regular interval without server intervention

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef __LORA_FULL_CONTROL_H__
#define __LORA_FULL_CONTROL_H__

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>         /* C99 types */
#include <stdbool.h>        /* bool type */
#include <stdio.h>          /* printf, fprintf, snprintf, fopen, fputs */
#include <inttypes.h>       /* PRIx64, PRIu64... */

#include <string.h>         /* memset */
#include <signal.h>         /* sigaction */
#include <time.h>           /* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>       /* timeval */
#include <unistd.h>         /* getopt, access */
#include <stdlib.h>         /* atoi, exit */
#include <errno.h>          /* error messages */
#include <math.h>           /* modf */

#include "loragw_hal.h"
#include "loragw_aux.h"
#include "loragw_reg.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE TYPES -------------------------------------------------------- */

/* spectral scan */
typedef struct spectral_scan_s {
    bool enable;            /* enable spectral scan thread */
    uint32_t freq_hz_start; /* first channel frequency, in Hz */
    uint8_t nb_chan;        /* number of channels to scan (200kHz between each channel) */
    uint16_t nb_scan;       /* number of scan points for each frequency scan */
    uint32_t pace_s;        /* number of seconds between 2 scans in the thread */
} spectral_scan_t;

struct sx1261_lbt_channel_t {
    uint32_t freq_hz;
    uint32_t bandwidth;
    uint8_t scan_time_us;
    uint16_t transmit_time_ms;
};

struct radio_config_t {
    
    bool enable;
    char* type;

    uint32_t freq;
    int8_t rssi_offset; 
    bool tx_enable;
    uint32_t tx_freq_min;
    uint32_t tx_freq_max;
    bool single_input_mode;

    struct {
        float coeff_a;
        float coeff_b; 
        float coeff_c; 
        float coeff_d;
        float coeff_e;    
    } rssi_tcomp;

    struct lgw_tx_gain_s tx_gain_lut[16];   
};

struct chan_multisf_conf {
    bool enable;
    uint8_t radio;
    int32_t if_hz; 
};

struct chan_lora_conf {
    bool enable;
    uint8_t rf_chain;
    int32_t freq_hz;
    uint32_t bandwidth;
    uint8_t datarate;
    bool implicit_hdr;
    uint8_t implicit_payload_length;
    bool implicit_crc_en; 
    uint8_t implicit_coderate;    
};

static struct sx1261_lbt_channel_t LBT_CHANNELS [] = {     
    { .freq_hz = 433000000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },       
    { .freq_hz = 433200000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },
    { .freq_hz = 433400000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },    
    { .freq_hz = 433600000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },   
    { .freq_hz = 433800000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },    
    { .freq_hz = 434000000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },       
    { .freq_hz = 434200000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },      
    { .freq_hz = 434400000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },          
    { .freq_hz = 434600000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 },    
    { .freq_hz = 434800000, .bandwidth = 125000, .scan_time_us = 128, .transmit_time_ms = 450 }     
};

// lgw_conf_rxrf_s
static struct radio_config_t RADIOS_CONFIG[2] = {
    {
    .enable = true,
    .type = "SX1250",
    .freq = 434000000,
    .rssi_offset = -207,  
    .tx_enable = true,
    .tx_freq_min = 433000000,
    .tx_freq_max = 435000000,
    .single_input_mode = true,
    .rssi_tcomp = {
      .coeff_a = 0,
      .coeff_b = 0,
      .coeff_c = 20.41,
      .coeff_d = 2162.56,  
      .coeff_e = 0   
    },
    .tx_gain_lut = {
      {.rf_power = 27, .pa_gain = 0, .pwr_idx = 0},  
      {.rf_power = 27, .pa_gain = 0, .pwr_idx = 1},
      {.rf_power = 27, .pa_gain = 0, .pwr_idx = 2},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 3},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 4},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 5},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 6}, 
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 7},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 8},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 9},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 10}, 
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 11},   
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 12},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 13},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 14},
      {.rf_power = 27, .pa_gain = 1, .pwr_idx = 15}   
    }            
   }, {    
     .enable = true,   
     .type = "SX1250",
     .freq = 434000000,
     .rssi_offset = -176,      
     .tx_enable = false,      
     .single_input_mode = true,
     .rssi_tcomp = {
       .coeff_a = 0,
       .coeff_b = 0,  
       .coeff_c = 20.41,
       .coeff_d = 2162.56,
       .coeff_e = 0  
     }      
   }
};


static struct chan_multisf_conf CHAN_MULTISFS_CONFIGS[] = {
    { .enable = true, .radio =  0, .if_hz = -600000 },  // chan_multiSF_0
    { .enable = true, .radio =  0, .if_hz = -300000 },  // chan_multiSF_1
    { .enable = true, .radio =  0, .if_hz =  300000 },  // chan_multiSF_2
    { .enable = true, .radio =  0, .if_hz =  600000 },  // chan_multiSF_3
    { .enable = true, .radio =  1, .if_hz = -600000 },  // chan_multiSF_4
    { .enable = true, .radio =  1, .if_hz = -300000 },  // chan_multiSF_5
    { .enable = true, .radio =  1, .if_hz =  300000 },  // chan_multiSF_6
    { .enable = true, .radio =  1, .if_hz =  600000 }   // chan_multiSF_7
};

static struct lgw_conf_rxif_s CHAN_LORA_STD = {
    .enable = true,
    .rf_chain = 1,
    .freq_hz = -200000,
    .bandwidth = BW_500KHZ,
    .datarate = DR_LORA_SF10,
    .implicit_hdr = false,
    .implicit_payload_length = 255,    
    .implicit_crc_en = true,
    .implicit_coderate = 5
};

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define STRINGIFY(x)    #x
#define STR(x)          STRINGIFY(x)

#define RAND_RANGE(min, max) (rand() % (max + 1 - min) + min)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#ifndef VERSION_STRING
    #define VERSION_STRING "undefined"
#endif

#define PKT_PUSH_DATA   0
#define PKT_PUSH_ACK    1
#define PKT_PULL_DATA   2
#define PKT_PULL_RESP   3
#define PKT_PULL_ACK    4
#define PKT_TX_ACK      5

#define NB_PKT_MAX      255 /* max number of packets per fetch/send cycle */

#define MIN_LORA_PREAMB 6 /* minimum Lora preamble length for this application */
#define STD_LORA_PREAMB 8
#define MIN_FSK_PREAMB  3 /* minimum FSK preamble length for this application */
#define STD_FSK_PREAMB  5

#define STATUS_SIZE     200
#define TX_BUFF_SIZE    ((540 * NB_PKT_MAX) + 30 + STATUS_SIZE)
#define ACK_BUFF_SIZE   64

#define DEFAULT_BEACON_FREQ_HZ      869525000
#define DEFAULT_BEACON_FREQ_NB      1
#define DEFAULT_BEACON_FREQ_STEP    0
#define DEFAULT_BEACON_DATARATE     9
#define DEFAULT_BEACON_BW_HZ        125000
#define DEFAULT_BEACON_POWER        14
#define DEFAULT_BEACON_INFODESC     0

#define LGW_COM_TYPE LGW_COM_SPI // or LGW_COM_USB
#define LGW_COM_PATH "/dev/spidev0.0" // spi or usb path of raspberry pi
#define LGW_IS_PUBLIC false // boolean
#define LGW_CLOCK_SOURCE 0 // 0 or 1 (uint8_t)
#define LGW_IS_FULL_DUPLEX false // boolean
#define LGW_ANTENNA_GAIN 0 // uint8_t
#define LGW_

#define SX1261_COM_TYPE LGW_COM_SPI
#define SX1261_COM_PATH "/dev/spidev0.1"
#define SX1261_RSSI_OFFSET 0

#define SX1261_SCAN_IS_ENABLE true
#define SX1261_SCAN_FRQ_START 433000000
#define SX1261_SCAN_NB_CHAN 10
#define SX1261_SCAN_NB_SCAN 100
#define SX1261_SCAN_PACE_S 1
#define SX1261_LBT_IS_ENABLE false
#define SX1261_LBT_RSSI_TARGET -70

#define SX1261_LBT_CHANNELS LBT_CHANNELS
#define SX1261_LBT_NB_CHANNEL 10 // (sizeof(SX1261_LBT_CHANNELS) / sizeof(SX1261_LBT_CHANNELS[0]))

#define LGW_RADIOS_CONFIG RADIOS_CONFIG
#define LGW_CHAN_MULTISFS_CONFIGS CHAN_MULTISFS_CONFIGS
#define LGW_CHAN_LORA_STD CHAN_LORA_STD

#define LGW_CHAN_FSK_IS_ENABLE false
#define LGW_CHAN_FSK_RADIO 0
#define LGW_CHAN_FSK_IF 10000
#define LGW_CHAN_FSK_BANDWIDTH 125E3
#define LGW_CHAN_FSK_FRQ_DEVIATION 100
#define LGW_CHAN_FSK_DATARATE 1000

#define GW_ID "0016C001FF1F5632"
#define GW_ID_UINT64 0x0016C001FF1F5632U

#define SERVER_ADDR "127.0.0.1"

#define SERV_PORT_UP 8080
#define SERV_PORT_DOWN 8080

#define KEEPALIVE_INTERVAL 10

#define STAT_INTERVAL 30

#define PUSH_TIMEOUT_MS 100

#define FWDR_CRC_VALID true
#define FWDR_CRC_ERROR false
#define FWDR_CRC_DISABLED false

#define BEACON_PERIOD 0
#define BEACON_FREQ 433000000
#define BEACON_DR 9
#define BEACON_BW 125000
#define BEACON_PWR 27
#define BEACON_INFO 0
#define BEACIN_FREQ_NB 1
#define BEACIN_FREQ_STEP 200000

#define AUTO_QUIT_THRESHOLD 10000

#define LGW_SERVERS_CONFIG servers_conf

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* network configuration variables */
static uint64_t lgwm = 0; /* Lora gateway MAC address */

/* gateway <-> MAC protocol variables */
static uint32_t net_mac_h; /* Most Significant Nibble, network order */
static uint32_t net_mac_l; /* Least Significant Nibble, network order */

/* beacon parameters */
static uint32_t beacon_period = 0; /* set beaconing period, must be a sub-multiple of 86400, the nb of sec in a day */
static uint32_t beacon_freq_hz = DEFAULT_BEACON_FREQ_HZ; /* set beacon TX frequency, in Hz */
static uint8_t beacon_freq_nb = DEFAULT_BEACON_FREQ_NB; /* set number of beaconing channels beacon */
static uint32_t beacon_freq_step = DEFAULT_BEACON_FREQ_STEP; /* set frequency step between beacon channels, in Hz */
static uint8_t beacon_datarate = DEFAULT_BEACON_DATARATE; /* set beacon datarate (SF) */
static uint32_t beacon_bw_hz = DEFAULT_BEACON_BW_HZ; /* set beacon bandwidth, in Hz */
static int8_t beacon_power = DEFAULT_BEACON_POWER; /* set beacon TX power, in dBm */
static uint8_t beacon_infodesc = DEFAULT_BEACON_INFODESC; /* set beacon information descriptor */

/* Gateway specificities */
static int8_t antenna_gain = 0;

/* TX capabilities */
static struct lgw_tx_gain_lut_s txlut[LGW_RF_CHAIN_NB]; /* TX gain table */
static uint32_t tx_freq_min[LGW_RF_CHAIN_NB]; /* lowest frequency supported by TX chain */
static uint32_t tx_freq_max[LGW_RF_CHAIN_NB]; /* highest frequency supported by TX chain */
static bool tx_enable[LGW_RF_CHAIN_NB] = {false}; /* Is TX enabled for a given RF chain ? */

static uint32_t nb_pkt_log[LGW_IF_CHAIN_NB][8]; /* [CH][SF] */
static uint32_t nb_pkt_received_lora = 0;
static uint32_t nb_pkt_received_fsk = 0;

static uint32_t nb_pkt_received_ref[16];

/* Interface type */
static lgw_com_type_t com_type = LGW_COM_SPI;

/* Spectral Scan */
static spectral_scan_t spectral_scan_params = {
    .enable = true,
    .freq_hz_start = 4330E5,
    .nb_chan = 10,
    .nb_scan = 10,
    .pace_s = 1
};

struct lgw_pkt_tx_s txpkt;
int32_t spectral_scan_freq_hz = SX1261_SCAN_FRQ_START;


static void sig_handler(int sigio);
static int parse_gateway_configuration();
static int get_tx_gain_lut_index(uint8_t rf_chain, int8_t rf_power, uint8_t * lut_index);

static void setup_all_modules();
void lgw_send_packet();
void lgw_receive_packets();
void sx1261_spectral_scan();

int main();

#endif

// EOF