deps_config := \
	/home/beam/Pingzee/Pingzee-ESP32/components/aws_iot/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/bt/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/esp32/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/ethernet/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/fatfs/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/freertos/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/log/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/lwip/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/mbedtls/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/openssl/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/spi_flash/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/projects/PingZee-BT_WiFi/main/Kconfig \
	/home/beam/Pingzee/Pingzee-ESP32/components/bootloader/Kconfig.projbuild \
	/home/beam/Pingzee/Pingzee-ESP32/components/esptool_py/Kconfig.projbuild \
	/home/beam/Pingzee/Pingzee-ESP32/components/partition_table/Kconfig.projbuild \
	/home/beam/Pingzee/Pingzee-ESP32/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
