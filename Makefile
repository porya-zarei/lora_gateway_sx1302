### Environment constants

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

.PHONY: all clean install install_conf libtools libloragw packet_forwarder util_net_downlink util_chip_id util_boot util_spectral_scan socket_server lora_control

all: libtools libloragw packet_forwarder util_net_downlink util_chip_id util_boot util_spectral_scan socket_server lora_control

libtools:
	$(MAKE) all -e -C $@

libloragw: libtools
	$(MAKE) all -e -C $@

packet_forwarder: libloragw
	$(MAKE) all -e -C $@

lora_control: libloragw
	$(MAKE) all -e -C $@

util_net_downlink: libtools
	$(MAKE) all -e -C $@

util_chip_id: libloragw
	$(MAKE) all -e -C $@

util_boot: libloragw
	$(MAKE) all -e -C $@

util_spectral_scan: libloragw
	$(MAKE) all -e -C $@

socket_server: libloragw
	$(MAKE) all -e -C $@

clean:
	$(MAKE) clean -e -C libtools
	$(MAKE) clean -e -C libloragw
	$(MAKE) clean -e -C packet_forwarder
	$(MAKE) clean -e -C lora_control
	$(MAKE) clean -e -C util_net_downlink
	$(MAKE) clean -e -C util_chip_id
	$(MAKE) clean -e -C util_boot
	$(MAKE) clean -e -C util_spectral_scan
	$(MAKE) clean -e -C socket_server

install:
	$(MAKE) install -e -C libloragw
	$(MAKE) install -e -C packet_forwarder
	$(MAKE) install -e -C lora_control
	$(MAKE) install -e -C util_net_downlink
	$(MAKE) install -e -C util_chip_id
	$(MAKE) install -e -C util_boot
	$(MAKE) install -e -C util_spectral_scan
	$(MAKE) install -e -C socket_server

install_conf:
	$(MAKE) install_conf -e -C packet_forwarder

### EOF
