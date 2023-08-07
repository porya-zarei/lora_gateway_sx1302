#include "lora_full_control.h"

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = true;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = true;
    }
    return;
}

static void setup_all_modules() {
    int i, j, number;
    char param_name[32]; /* used to generate variable parameter names */
    const char *str; /* used to store string value from JSON object */

    struct lgw_conf_board_s boardconf;
    struct lgw_conf_rxrf_s rfconf;
    struct lgw_conf_rxif_s ifconf;
    struct lgw_conf_demod_s demodconf;
    struct lgw_conf_ftime_s tsconf;
    struct lgw_conf_sx1261_s sx1261conf;

    uint32_t sf, bw, fdev;
    bool sx1250_tx_lut;
    size_t size;
    /* set board configuration */
    memset(&boardconf, 0, sizeof boardconf); /* initialize configuration structure */
    
    boardconf.com_type = LGW_COM_TYPE;
    strncpy(boardconf.com_path, LGW_COM_PATH, sizeof boardconf.com_path);
    boardconf.com_path[sizeof boardconf.com_path - 1] = '\0'; /* ensure string termination */
    boardconf.lorawan_public = LGW_IS_PUBLIC;
    boardconf.clksrc = (uint8_t)LGW_CLOCK_SOURCE;
    boardconf.full_duplex = LGW_IS_FULL_DUPLEX;
    
    printf("INFO: com_type %s, com_path %s, lorawan_public %d, clksrc %d, full_duplex %d\n", (boardconf.com_type == LGW_COM_SPI) ? "SPI" : "USB", boardconf.com_path, boardconf.lorawan_public, boardconf.clksrc, boardconf.full_duplex);
    
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_board_setconf(&boardconf) != LGW_HAL_SUCCESS) {
        printf("ERROR: Failed to configure board\n");
        return;
    }

    /* set antenna gain configuration */

    antenna_gain = LGW_ANTENNA_GAIN;

    printf("INFO: antenna_gain %d dBi\n", antenna_gain);

    /* set SX1261 configuration */
    memset(&sx1261conf, 0, sizeof sx1261conf); /* initialize configuration structure */

    strncpy(sx1261conf.spi_path,SX1261_COM_PATH, sizeof sx1261conf.spi_path);
    sx1261conf.spi_path[sizeof sx1261conf.spi_path - 1] = '\0'; /* ensure string termination */
    sx1261conf.rssi_offset = (int8_t)SX1261_RSSI_OFFSET;
    spectral_scan_params.enable = (bool)SX1261_SCAN_IS_ENABLE;
    if (spectral_scan_params.enable == true) {
        /* Enable the sx1261 radio hardware configuration to allow spectral scan */
        sx1261conf.enable = true;
        printf("INFO: Spectral Scan with SX1261 is enabled\n");
        spectral_scan_params.freq_hz_start = (uint32_t)SX1261_SCAN_FRQ_START;
        spectral_scan_params.nb_chan = (uint8_t)SX1261_SCAN_NB_CHAN;
        spectral_scan_params.nb_scan = (uint16_t)SX1261_SCAN_NB_SCAN;
        spectral_scan_params.pace_s = (uint32_t)SX1261_SCAN_PACE_S;
    }
    /* LBT configuration */

    sx1261conf.lbt_conf.enable = (bool)SX1261_LBT_IS_ENABLE;

    if (sx1261conf.lbt_conf.enable == true) {
        /* Enable the sx1261 radio hardware configuration to allow spectral scan */
        sx1261conf.enable = true;
        printf("INFO: Listen-Before-Talk with SX1261 is enabled\n");
        sx1261conf.lbt_conf.rssi_target = (int8_t)SX1261_LBT_RSSI_TARGET;
        /* set LBT channels configuration */
        sx1261conf.lbt_conf.nb_channel = SX1261_LBT_NB_CHANNEL;

        for (i = 0; i < (int)sx1261conf.lbt_conf.nb_channel; i++) {
            /* Sanity check */
            if (i >= LGW_LBT_CHANNEL_NB_MAX) {
                printf("ERROR: LBT channel %d not supported, skip it\n", i);
                break;
            }
            sx1261conf.lbt_conf.channels[i].freq_hz = SX1261_LBT_CHANNELS[i].freq_hz;
            /* Channel bandiwdth */
            bw = (uint32_t)SX1261_LBT_CHANNELS[i].bandwidth;
            switch(bw) {
                case 500000: sx1261conf.lbt_conf.channels[i].bandwidth = BW_500KHZ; break;
                case 250000: sx1261conf.lbt_conf.channels[i].bandwidth = BW_250KHZ; break;
                case 125000: sx1261conf.lbt_conf.channels[i].bandwidth = BW_125KHZ; break;
                default: sx1261conf.lbt_conf.channels[i].bandwidth = BW_UNDEFINED;
            }
            // printf("INFO: lbt channel(%d) -> bw:%d selected\r\n",i,sx1261conf.lbt_conf.channels[i].bandwidth);
            /* Channel scan time */
            if (SX1261_LBT_CHANNELS[i].scan_time_us == 128) {
                sx1261conf.lbt_conf.channels[i].scan_time_us = LGW_LBT_SCAN_TIME_128_US;
            } else if (SX1261_LBT_CHANNELS[i].scan_time_us == 5000) {
                sx1261conf.lbt_conf.channels[i].scan_time_us = LGW_LBT_SCAN_TIME_5000_US;
            } else {
                printf("ERROR: scan time not supported for LBT channel %d, must be 128 or 5000\n", i);
                return;
            }
            /* Channel transmit time */
            sx1261conf.lbt_conf.channels[i].transmit_time_ms = SX1261_LBT_CHANNELS[i].transmit_time_ms;
        }
    }
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_sx1261_setconf(&sx1261conf) != LGW_HAL_SUCCESS) {
        printf("ERROR: Failed to configure the SX1261 radio\n");
        return;
    }

    /* set configuration for RF chains */
    for (i = 0; i < LGW_RF_CHAIN_NB; ++i) {
        memset(&rfconf, 0, sizeof rfconf); /* initialize configuration structure */
        /* there is an object to configure that radio, let's parse it */
        rfconf.enable = LGW_RADIOS_CONFIG[i].enable;

        if (rfconf.enable == false) { /* radio disabled, nothing else to parse */
            printf("INFO: radio %i disabled\n", i);
        } else  { /* radio enabled, will parse the other parameters */

            rfconf.freq_hz = (uint32_t)LGW_RADIOS_CONFIG[i].freq;
            rfconf.rssi_offset = (float)LGW_RADIOS_CONFIG[i].rssi_offset;
            rfconf.rssi_tcomp.coeff_a = (float)LGW_RADIOS_CONFIG[i].rssi_tcomp.coeff_a;
            rfconf.rssi_tcomp.coeff_b = (float)LGW_RADIOS_CONFIG[i].rssi_tcomp.coeff_b;
            rfconf.rssi_tcomp.coeff_c = (float)LGW_RADIOS_CONFIG[i].rssi_tcomp.coeff_c;
            rfconf.rssi_tcomp.coeff_d = (float)LGW_RADIOS_CONFIG[i].rssi_tcomp.coeff_d;
            rfconf.rssi_tcomp.coeff_e = (float)LGW_RADIOS_CONFIG[i].rssi_tcomp.coeff_e;

            if (!strncmp(LGW_RADIOS_CONFIG[i].type, "SX1255", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1255;
            } else if (!strncmp(LGW_RADIOS_CONFIG[i].type, "SX1257", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1257;
            } else if (!strncmp(LGW_RADIOS_CONFIG[i].type, "SX1250", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1250;
            } else {
                printf("WARNING: invalid radio type: %s (should be SX1255 or SX1257 or SX1250)\n", str);
            }

            rfconf.single_input_mode = LGW_RADIOS_CONFIG[i].single_input_mode;

            
            rfconf.tx_enable = LGW_RADIOS_CONFIG[i].tx_enable;
            tx_enable[i] = rfconf.tx_enable; /* update global context for later check */
            if (rfconf.tx_enable == true) {
                /* tx is enabled on this rf chain, we need its frequency range */
                tx_freq_min[i] = LGW_RADIOS_CONFIG[i].tx_freq_min;
                tx_freq_max[i] = LGW_RADIOS_CONFIG[i].tx_freq_max;
                if ((tx_freq_min[i] == 0) || (tx_freq_max[i] == 0)) {
                    printf("WARNING: no frequency range specified for TX rf chain %d\n", i);
                }

                /* set configuration for tx gains */
                memset(&txlut[i], 0, sizeof txlut[i]); /* initialize configuration structure */

                txlut[i].size = sizeof(LGW_RADIOS_CONFIG[i].tx_gain_lut)/sizeof(LGW_RADIOS_CONFIG[i].tx_gain_lut[0]);
                sx1250_tx_lut = LGW_RADIOS_CONFIG[i].tx_gain_lut[0].pwr_idx == NULL;
                printf("INFO: Configuring Tx Gain LUT for rf_chain %u with %u indexes for sx1250 (%d)\n", i, txlut[i].size,sx1250_tx_lut);
                /* Parse the table */
                for (j = 0; j < (int)txlut[i].size; j++) {
                        /* Sanity check */
                    if (j >= TX_GAIN_LUT_SIZE_MAX) {
                        printf("ERROR: TX Gain LUT [%u] index %d not supported, skip it\n", i, j);
                        break;
                    }
                    /* Get TX gain object from LUT */
                    txlut[i].lut[j].rf_power = LGW_RADIOS_CONFIG[i].tx_gain_lut[j].rf_power;
                    /* PA gain */
                    txlut[i].lut[j].pa_gain = (uint8_t)LGW_RADIOS_CONFIG[i].tx_gain_lut[j].pa_gain;
                    
                    if (sx1250_tx_lut == false) {
                        txlut[i].lut[j].dig_gain = LGW_RADIOS_CONFIG[i].tx_gain_lut[j].dig_gain;
                        txlut[i].lut[j].dac_gain = LGW_RADIOS_CONFIG[i].tx_gain_lut[j].dac_gain == NULL? 3 : LGW_RADIOS_CONFIG[i].tx_gain_lut[j].dac_gain;
                        txlut[i].lut[j].mix_gain = LGW_RADIOS_CONFIG[i].tx_gain_lut[j].mix_gain;
                    } else {
                        /* TODO: rework this, should not be needed for sx1250 */
                        txlut[i].lut[j].mix_gain = 5;
                    }
                }
                /* all parameters parsed, submitting configuration to the HAL */
                if (txlut[i].size > 0) {
                    if (lgw_txgain_setconf(i, &txlut[i]) != LGW_HAL_SUCCESS) {
                        printf("ERROR: Failed to configure concentrator TX Gain LUT for rf_chain %u\n", i);
                        return;
                    }
                } else {
                    printf("WARNING: No TX gain LUT defined for rf_chain %u\n", i);
                }
            }
            printf("INFO: radio %i enabled (type %s), center frequency %u, RSSI offset %f, tx enabled %d, single input mode %d\n", i, str, rfconf.freq_hz, rfconf.rssi_offset, rfconf.tx_enable, rfconf.single_input_mode);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxrf_setconf(i, &rfconf) != LGW_HAL_SUCCESS) {
            printf("ERROR: invalid configuration for radio %i\n", i);
            return;
        }
    }

    /* set configuration for demodulators */
    memset(&demodconf, 0, sizeof demodconf); /* initialize configuration structure */
    demodconf.multisf_datarate = 0xFF; /* enable all SFs */
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_demod_setconf(&demodconf) != LGW_HAL_SUCCESS) {
        printf("ERROR: invalid configuration for demodulation parameters\n");
        return;
    }

    /* set configuration for Lora multi-SF channels (bandwidth cannot be set) */
    for (i = 0; i < LGW_MULTI_NB; ++i) {
        memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
        ifconf.enable = LGW_CHAN_MULTISFS_CONFIGS[i].enable;
        if (ifconf.enable == false) { /* Lora multi-SF channel disabled, nothing else to parse */
            printf("INFO: Lora multi-SF channel %i disabled\n", i);
        } else  { /* Lora multi-SF channel enabled, will parse the other parameters */
            ifconf.rf_chain = LGW_CHAN_MULTISFS_CONFIGS[i].radio;
            ifconf.freq_hz = LGW_CHAN_MULTISFS_CONFIGS[i].if_hz;
            printf("INFO: Lora multi-SF channel %i>  radio %i, IF %i Hz, 125 kHz bw, SF 5 to 12\n", i, ifconf.rf_chain, ifconf.freq_hz);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxif_setconf(i, &ifconf) != LGW_HAL_SUCCESS) {
            printf("ERROR: invalid configuration for Lora multi-SF channel %i\n", i);
            return;
        }
    }
    /* set configuration for Lora standard channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    ifconf.enable = LGW_CHAN_LORA_STD.enable;
    if (ifconf.enable == false) {
        printf("INFO: Lora standard channel %i disabled\n", i);
    } else  {
        // printf("lora std => %d,%ld,%ld,%d,%d,%d \r\n",LGW_CHAN_LORA_STD.enable,LGW_CHAN_LORA_STD.freq_hz,LGW_CHAN_LORA_STD.bandwidth,LGW_CHAN_LORA_STD.rf_chain,LGW_CHAN_LORA_STD.datarate,LGW_CHAN_LORA_STD.implicit_payload_length);
        ifconf.rf_chain = LGW_CHAN_LORA_STD.rf_chain;
        ifconf.freq_hz = LGW_CHAN_LORA_STD.freq_hz;
        ifconf.bandwidth = LGW_CHAN_LORA_STD.bandwidth;
        ifconf.datarate = LGW_CHAN_LORA_STD.datarate;
        ifconf.implicit_hdr = LGW_CHAN_LORA_STD.implicit_hdr;
        if (ifconf.implicit_hdr == true) {
            ifconf.implicit_payload_length = (uint8_t)LGW_CHAN_LORA_STD.implicit_payload_length;
            ifconf.implicit_crc_en = LGW_CHAN_LORA_STD.implicit_crc_en;
            ifconf.implicit_coderate = LGW_CHAN_LORA_STD.implicit_coderate;
        }
        printf("INFO: Lora std channel> radio %i, IF %i Hz, %u Hz bw, SF %u, %s\n", ifconf.rf_chain, ifconf.freq_hz, ifconf.bandwidth, ifconf.datarate, (ifconf.implicit_hdr == true) ? "Implicit header" : "Explicit header");
    }
    
    if (lgw_rxif_setconf(8, &ifconf) != LGW_HAL_SUCCESS) {
        printf("ERROR: invalid configuration for Lora standard channel\n");
        return;
    }

    /* set configuration for FSK channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
        
    ifconf.enable = LGW_CHAN_FSK_IS_ENABLE;

    if (ifconf.enable == false) {
        printf("INFO: FSK channel %i disabled\n", i);
    } else  {
        ifconf.rf_chain = LGW_CHAN_FSK_RADIO;
        ifconf.freq_hz = LGW_CHAN_FSK_IF;
        bw = LGW_CHAN_FSK_BANDWIDTH;
        fdev = LGW_CHAN_FSK_FRQ_DEVIATION;
        ifconf.datarate = LGW_CHAN_FSK_DATARATE;
        /* if chan_FSK.bandwidth is set, it has priority over chan_FSK.freq_deviation */
        if ((bw == 0) && (fdev != 0)) {
            bw = 2 * fdev + ifconf.datarate;
        }
        if (bw == 0) ifconf.bandwidth = BW_UNDEFINED;
        else if (bw <= 125000) ifconf.bandwidth = BW_125KHZ;
        else if (bw <= 250000) ifconf.bandwidth = BW_250KHZ;
        else if (bw <= 500000) ifconf.bandwidth = BW_500KHZ;
        else ifconf.bandwidth = BW_UNDEFINED;
        printf("INFO: FSK channel> radio %i, IF %i Hz, %u Hz bw, %u bps datarate\n", ifconf.rf_chain, ifconf.freq_hz, bw, ifconf.datarate);
    }
    if (lgw_rxif_setconf(9, &ifconf) != LGW_HAL_SUCCESS) {
        printf("ERROR: invalid configuration for FSK channel\n");
        return;
    }
    printf("INFO: All boards modules configed correctly.\r\n");
}

static int parse_gateway_configuration() {
    unsigned long long ull = 0;
    /* gateway unique identifier (aka MAC address) (optional) */
    if (GW_ID != NULL) {
        sscanf(GW_ID, "%llx", &ull);
        lgwm = GW_ID_UINT64;
        printf("INFO: gateway MAC address is configured to %016llX\n", ull);
    }

    /* Beacon signal period (optional) */
    if (BEACON_PERIOD != NULL) {
        beacon_period = (uint32_t)BEACON_PERIOD;
        printf("INFO: Beaconing period is configured to %u seconds\n", beacon_period);
    }

    /* Beacon TX frequency (optional) */
    if (BEACON_FREQ != NULL) {
        beacon_freq_hz = (uint32_t)BEACON_FREQ;
        printf("INFO: Beaconing signal will be emitted at %u Hz\n", beacon_freq_hz);
    }

    /* Number of beacon channels (optional) */
    if (BEACIN_FREQ_NB != NULL) {
        beacon_freq_nb = (uint8_t)BEACIN_FREQ_NB;
        printf("INFO: Beaconing channel number is set to %u\n", beacon_freq_nb);
    }

    /* Frequency step between beacon channels (optional) */
    if (BEACIN_FREQ_STEP != NULL) {
        beacon_freq_step = (uint32_t)BEACIN_FREQ_STEP;
        printf("INFO: Beaconing channel frequency step is set to %uHz\n", beacon_freq_step);
    }

    /* Beacon datarate (optional) */
    if (BEACON_DR != NULL) {
        beacon_datarate = (uint8_t)BEACON_DR;
        printf("INFO: Beaconing datarate is set to SF%d\n", beacon_datarate);
    }

    /* Beacon modulation bandwidth (optional) */
    if (BEACON_BW != NULL) {
        beacon_bw_hz = (uint32_t)BEACON_BW;
        printf("INFO: Beaconing modulation bandwidth is set to %dHz\n", beacon_bw_hz);
    }

    /* Beacon TX power (optional) */
    if (BEACON_PWR != NULL) {
        beacon_power = (int8_t)BEACON_PWR;
        printf("INFO: Beaconing TX power is set to %ddBm\n", beacon_power);
    }

    /* Beacon information descriptor (optional) */
    if (BEACON_INFO != NULL) {
        beacon_infodesc = (uint8_t)BEACON_INFO;
        printf("INFO: Beaconing information descriptor is set to %u\n", beacon_infodesc);
    }

    printf("INFO: gateway configes set correctly.\r\n");
    return 0;
}

static int get_tx_gain_lut_index(uint8_t rf_chain, int8_t rf_power, uint8_t * lut_index) {
    uint8_t pow_index;
    int current_best_index = -1;
    uint8_t current_best_match = 0xFF;
    int diff;
    // printf("%s => rf_chain:%d, rf_power:%d",__FUNCTION__,rf_chain,rf_power);
    /* Check input parameters */
    if (lut_index == NULL) {
        printf("ERROR: %s - wrong parameter\n", __FUNCTION__);
        return -1;
    }

    /* Search requested power in TX gain LUT */
    for (pow_index = 0; pow_index < txlut[rf_chain].size; pow_index++) {
        diff = rf_power - txlut[rf_chain].lut[pow_index].rf_power;
        if (diff < 0) {
            /* The selected power must be lower or equal to requested one */
            continue;
        } else {
            /* Record the index corresponding to the closest rf_power available in LUT */
            if ((current_best_index == -1) || (diff < current_best_match)) {
                current_best_match = diff;
                current_best_index = pow_index;
            }
        }
    }

    /* Return corresponding index */
    if (current_best_index > -1) {
        *lut_index = (uint8_t)current_best_index;
    } else {
        *lut_index = 0;
        printf("ERROR: %s - failed to find tx gain lut index\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

void lgw_send_packet() {
    int i, result;
    uint8_t x0,x1;

    uint8_t tx_lut_idx = 0;
    uint32_t pkt_sf = DR_LORA_SF10; // (uint32_t)(5+(rand() % 8)); // DR_LORA_SF5;
    uint8_t pkt_bw = BW_125KHZ;
    uint8_t pkt_coderate = CR_LORA_4_5; // "4/5" or "4/6" or "4/7" or "4/8"
    uint16_t pkt_preamble_length = (uint16_t)0b0000000000010001;
    uint32_t pkt_freq_hz = (uint32_t)4342E5;
    
    char *pkt_modulation_mode = MOD_LORA; // "LORA" or "FSK"
    char pkt_data[255] = "HiHodHodHowHighHeHis"; // in server mode it must be in base64 format
    char text_data[] = "HiHodHodHowHighHeHis";
    // struct lgw_pkt_tx_s txpkt;
    bool sent_immediate = false; /* option to sent the packet immediately */
    /* local timekeeping variables */
    struct timespec send_time; /* time of the pull request */
    struct timespec recv_time; /* time of return from recv socket call */

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */
    
    uint8_t tx_status;

    /* initialize TX struct and try to parse JSON */
    memset(&txpkt, 0, sizeof(txpkt));
    
    wait_ms(10);

    /* look for JSON sub-object 'txpk' */
    
    /* Parse "immediate" tag, or target timestamp, or UTC time to be converted by GPS (mandatory) */

    /* TX procedure: send immediately */
    sent_immediate = true;
    printf("INFO: [down] a packet will be sent in \"immediate\" mode\n");
    
    
    /* Parse "No CRC" flag (optional field) */
    txpkt.no_crc = false;
    /* Parse "No header" flag (optional field) */
    txpkt.no_header = false;
    /* parse target frequency (mandatory) */
    txpkt.freq_hz = (uint32_t)(pkt_freq_hz);

    /* parse RF chain used for TX (mandatory) */
    txpkt.rf_chain = (uint8_t)0;
    if (tx_enable[txpkt.rf_chain] == false) {
        printf("WARNING: [down] TX is not enabled on RF chain %u, TX aborted\n", txpkt.rf_chain);
    }
    /* parse TX power (optional field) */
    txpkt.rf_power = (int8_t)27 - antenna_gain;

    if (pkt_modulation_mode == MOD_LORA)
    {
        /* Lora modulation */
        txpkt.modulation = MOD_LORA;
        /* Parse Lora spreading-factor and modulation bandwidth (mandatory) */
        txpkt.datarate = pkt_sf;
        txpkt.bandwidth = pkt_bw;

        /* Parse ECC coding rate (optional field) */
        txpkt.coderate = pkt_coderate;
        /* Parse signal polarity switch (optional field) */
        txpkt.invert_pol = (bool)false;

        /* parse Lora preamble length (optional field, optimum min value enforced) */
        txpkt.preamble = pkt_preamble_length;
    }
    else if (pkt_modulation_mode == MOD_FSK)
    {
        /* FSK modulation */
        txpkt.modulation = MOD_FSK;

        /* parse FSK bitrate (mandatory) */
        txpkt.datarate = (uint32_t)(1000000);

        /* parse frequency deviation (mandatory) */
        txpkt.f_dev = (uint8_t)(433E6 / 1000.0); /* JSON value in Hz, txpkt.f_dev in kHz */

        /* parse FSK preamble length (optional field, optimum min value enforced) */
        txpkt.preamble = (uint16_t)STD_FSK_PREAMB;
    }

    /* Parse payload length (mandatory) */
    txpkt.size = (uint16_t)(sizeof(text_data) / sizeof(text_data[0]));
    strncpy((char *)txpkt.payload, (char *)pkt_data, txpkt.size - 1);
    txpkt.payload[txpkt.size] = '\0';

    printf("INFO: payload => first_char:%d, size:%d, dst:%s,src:%s\n",txpkt.payload[0],txpkt.size,(char *)txpkt.payload,(char *)pkt_data);
    
    /* select TX mode */
    if (sent_immediate) {
        txpkt.tx_mode = IMMEDIATE;
    } else {
        txpkt.tx_mode = TIMESTAMPED;
    }

    /* check TX frequency before trying to queue packet */
    if ((txpkt.freq_hz < tx_freq_min[txpkt.rf_chain]) || (txpkt.freq_hz > tx_freq_max[txpkt.rf_chain])) {
        printf("ERROR: Packet REJECTED, unsupported frequency - %u (min:%u,max:%u)\n", txpkt.freq_hz, tx_freq_min[txpkt.rf_chain], tx_freq_max[txpkt.rf_chain]);
    }

    /* check TX power before trying to queue packet, send a warning if not supported */
    i = get_tx_gain_lut_index(txpkt.rf_chain, txpkt.rf_power, &tx_lut_idx);
    if ((i < 0) || (txlut[txpkt.rf_chain].lut[tx_lut_idx].rf_power != txpkt.rf_power)) {
        /* this RF power is not supported, throw a warning, and use the closest lower power supported */
        printf("WARNING: Requested TX power is not supported (%ddBm)\n", txpkt.rf_power);
        txpkt.rf_power = txlut[txpkt.rf_chain].lut[tx_lut_idx].rf_power;
    }
    
    
    /* insert packet to be sent into JIT queue */
   
    result = lgw_status(txpkt.rf_chain, TX_STATUS, &tx_status);
    if (result == LGW_HAL_ERROR) {
        printf("WARNING: lgw_status failed\n", i);
    } else {
        if (tx_status == TX_EMITTING) {
            printf("ERROR: concentrator is currently emitting on rf_chain %d\n", i);
        } else if (tx_status == TX_SCHEDULED) {
            printf("WARNING: a downlink was already scheduled on rf_chain %d, overwritting it...\n", i);
        } else if(tx_status == TX_FREE) {
            printf("INFO: TX is free\r\n");
            result = lgw_send(&txpkt);
            if( result == LGW_HAL_SUCCESS ) {
                printf("INFO: packet sended successfully => sf:%d, \r\n",txpkt.datarate);
            }
        } else {
            /* Nothing to do */
        }
    }
    // result = lgw_send(&txpkt);
}

void lgw_receive_packets() {
    int i, j, k; /* loop variables */
    unsigned pkt_in_dgram; /* nb on Lora packet in the current datagram */
    char stat_timestamp[24];
    time_t t;

    /* allocate memory for packet fetching and processing */
    struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX]; /* array containing inbound packets + metadata */
    struct lgw_pkt_rx_s *p; /* pointer on a RX packet */
    int nb_pkt;
    /* data buffers */
    uint8_t buff_up[TX_BUFF_SIZE]; /* buffer to compose the upstream packet */
    int buff_index;
    uint8_t buff_ack[32]; /* buffer to receive acknowledges */

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */

    /* mote info variables */
    uint32_t mote_addr = 0;
    uint16_t mote_fcnt = 0;

    /* fetch packets */
    
    nb_pkt = lgw_receive(NB_PKT_MAX, rxpkt); // NB_PKT_MAX
    
    if (nb_pkt == LGW_HAL_ERROR) {
        printf("ERROR: [up] failed packet fetch, exiting\n");
        return;
    }

    /* start composing datagram with the header */
    token_h = (uint8_t)rand(); /* random token */
    token_l = (uint8_t)rand(); /* random token */
    buff_up[1] = token_h;
    buff_up[2] = token_l;
    buff_index = 12; /* 12-byte header */

    /* start of JSON structure */

    /* serialize Lora packets metadata and payload */
    pkt_in_dgram = 0;
    for (i = 0; i < nb_pkt; ++i) {
        p = &rxpkt[i];
        /* Get mote information from current packet (addr, fcnt) */
        /* FHDR - DevAddr */
        if (p->size >= 8) {
            mote_addr  = p->payload[1];
            mote_addr |= p->payload[2] << 8;
            mote_addr |= p->payload[3] << 16;
            mote_addr |= p->payload[4] << 24;
            /* FHDR - FCnt */
            mote_fcnt  = p->payload[6];
            mote_fcnt |= p->payload[7] << 8;
        } else {
            mote_addr = 0;
            mote_fcnt = 0;
        }

        switch(p->status) {
            case STAT_CRC_OK:
                break;
            case STAT_CRC_BAD:
                break;
            case STAT_NO_CRC:
                break;
            default:
                break;
        }
        printf("INFO: Received pkt from mote: %08X (fcnt=%u)\n", mote_addr, mote_fcnt);
        printf("INFO: packet => freq:%ld, sf:%d, bw:%d,if_chain:%d, payload:%s ,status:%d\r\n",p->freq_hz, p->datarate, p->bandwidth, p->if_chain, (char *)p->payload, p->status);
    }
}

void sx1261_spectral_scan() {
    int i, x;
    // uint32_t freq_hz = spectral_scan_params.freq_hz_start;
    uint32_t freq_hz_stop = spectral_scan_params.freq_hz_start + spectral_scan_params.nb_chan * 200E3;
    int16_t levels[LGW_SPECTRAL_SCAN_RESULT_SIZE];
    uint16_t results[LGW_SPECTRAL_SCAN_RESULT_SIZE];
    struct timeval tm_start;
    lgw_spectral_scan_status_t status;
    uint8_t tx_status = TX_FREE;
    bool spectral_scan_started;
    bool exit_thread = false;

    /* -- Check if there is a downlink programmed */
    for (i = 0; i < LGW_RF_CHAIN_NB; i++) {
        if (tx_enable[i] == true) {
            x = lgw_status((uint8_t)i, TX_STATUS, &tx_status);
            if (x != LGW_HAL_SUCCESS) {
                printf("ERROR: failed to get TX status on chain %d\n", i);
            } else {
                if (tx_status == TX_SCHEDULED || tx_status == TX_EMITTING) {
                    printf("INFO: skip spectral scan (downlink programmed on RF chain %d)\n", i);
                    /* exit for loop */
                    spectral_scan_started = false;
                    return;
                }
            }
        }
    }

    if (tx_status != TX_SCHEDULED && tx_status != TX_EMITTING) {
        x = lgw_spectral_scan_start(spectral_scan_freq_hz, spectral_scan_params.nb_scan);
        if (x != 0) {
            printf("ERROR: spectral scan start failed\n");
            /* main while loop */
        }
        spectral_scan_started = true;
    }

    if (spectral_scan_started == true) {
        /* Wait for scan to be completed */
        status = LGW_SPECTRAL_SCAN_STATUS_UNKNOWN;

        do {
            /* handle timeout */
            /* get spectral scan status */           
            x = lgw_spectral_scan_get_status(&status);
            if (x != 0) {
                printf("ERROR: spectral scan status failed\n");
                break; /* do while */
            }
            /* wait a bit before checking status again */
            wait_ms(10);
        } while (status != LGW_SPECTRAL_SCAN_STATUS_COMPLETED && status != LGW_SPECTRAL_SCAN_STATUS_ABORTED);

        if (status == LGW_SPECTRAL_SCAN_STATUS_COMPLETED) {
            /* Get spectral scan results */
            memset(levels, 0, sizeof levels);
            memset(results, 0, sizeof results);
            x = lgw_spectral_scan_get_results(levels, results);
            if (x != 0) {
                printf("ERROR: spectral scan get results failed\n");
                /* main while loop */
            }

            /* print results */
            printf("SPECTRAL SCAN - %u Hz: ", spectral_scan_freq_hz);
            for (i = 0; i < LGW_SPECTRAL_SCAN_RESULT_SIZE; i++) {
                printf("%u ", results[i]);
            }
            printf("\r\n");

            /* Next frequency to scan */
            spectral_scan_freq_hz += 200E3; /* 200kHz channels */
            if (spectral_scan_freq_hz >= freq_hz_stop) {
                spectral_scan_freq_hz = spectral_scan_params.freq_hz_start;
            }
        } else if (status == LGW_SPECTRAL_SCAN_STATUS_ABORTED) {
            printf("INFO: %s: spectral scan has been aborted\n", __FUNCTION__);
        } else {
            printf("ERROR: %s: spectral scan status us unexpected 0x%02X\n", __FUNCTION__, status);
        }
    }
    printf("\nINFO: End of Spectral Scan thread\n");
}

int main() {
    struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
    int i; /* loop variable and temporary variable for return value */
    int x;
    int l, m;

    /* GPS coordinates variables */

    /* SX1302 data variables */
    uint32_t trig_tstamp;
    uint32_t inst_tstamp;
    uint64_t eui;
    float temperature;

    /* statistics variable */

    setup_all_modules();

    parse_gateway_configuration();

    net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (lgwm>>32)));
    net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  lgwm  ));

    if (com_type == LGW_COM_SPI) {
        /* Board reset */
        if (system("./reset_lgw.sh start") != 0) {
            printf("ERROR: failed to reset SX1302, check your reset_lgw.sh script\n");
            exit(EXIT_FAILURE);
        }
    }

    /* starting the concentrator */
    i = lgw_start();
    if (i == LGW_HAL_SUCCESS) {
        printf("INFO: [main] concentrator started, packet can now be received\n");
    } else {
        printf("ERROR: [main] failed to start the concentrator\n");
        exit(EXIT_FAILURE);
    }

    /* get the concentrator EUI */
    i = lgw_get_eui(&eui);
    if (i != LGW_HAL_SUCCESS) {
        printf("ERROR: failed to get concentrator EUI\n");
    } else {
        printf("INFO: concentrator EUI: 0x%016" PRIx64 "\n", eui);
    }

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
    sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
    sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */

    /* main loop task : statistics collection */
    while (!exit_sig && !quit_sig) {
        /* wait for next reporting interval */

        i  = lgw_get_instcnt(&inst_tstamp);
        i |= lgw_get_trigcnt(&trig_tstamp);

        if (i != LGW_HAL_SUCCESS) {
            printf("# SX1302 counter unknown\n");
        } else {
            printf("# SX1302 counter (INST): %u\n", inst_tstamp);
            printf("# SX1302 counter (PPS):  %u\n", trig_tstamp);
        }
        /* get timestamp captured on PPM pulse  */

        i = lgw_get_temperature(&temperature);

        if (i != LGW_HAL_SUCCESS) {
            printf("### Concentrator temperature unknown ###\n");
        } else {
            printf("### Concentrator temperature: %.0f C ###\n", temperature);
        }
        printf("##### END #####\n");

        /* generate a JSON report (will be sent to server by upstream thread) */

        // do some work

        // send
        printf("INFO: start sending ...\r\n");
        lgw_send_packet();

        // wait
        wait_ms(10);

        // receive
        // printf("INFO: start receiving ...\r\n");
        // lgw_receive_packets();
        
        // scanning ...
        wait_ms(10);
        printf("INFO: start scanning ...\r\n");
        sx1261_spectral_scan();

        printf("INFO: wait start\r\n");
        wait_ms(1000);
    }

    /* if an exit signal was received, try to quit properly */
    if (exit_sig) {
        /* stop the hardware */
        i = lgw_stop();
        if (i == LGW_HAL_SUCCESS) {
            printf("INFO: concentrator stopped successfully\n");
        } else {
            printf("WARNING: failed to stop concentrator successfully\n");
        }
    }

    if (com_type == LGW_COM_SPI) {
        /* Board reset */
        if (system("./reset_lgw.sh stop") != 0) {
            printf("ERROR: failed to reset SX1302, check your reset_lgw.sh script\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("INFO: Exiting packet forwarder program\n");
    exit(EXIT_SUCCESS);

    return 0;
}

// EOF