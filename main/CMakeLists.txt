set(srcs 
	"main.c"
	"gpio_lib.c"
	"mqtt.c"
	"mqtt_client.c"
	"cstr.c"
	"network_wifi.c"
	"network_tcpclient.c"
	"network_webserver"
	)


idf_component_register(SRCS ${srcs} INCLUDE_DIRS ".")

