#include <stdio.h>
#include "protocol_examples_common.h"
#include "nvs_flash.h"
#include "esp_event.h"

int test_connectivity();
#include <string.h>

#include "linenoise/linenoise.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#define OT_CLI_MAX_LINE_LENGTH      256
#define ESP_CONSOLE_PREFIX          "esp "
#define ESP_CONSOLE_PREFIX_LENGTH   4
static esp_console_repl_t *s_repl = NULL;

typedef struct {
    struct arg_str *text;
    struct arg_end *end;
} my_args_t;
static my_args_t my_args;

static int do_parse(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **) &my_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, my_args.end, argv[0]);
        return 1;
    }
    const char *text = my_args.text->sval[0];
    printf("TEXT=%s\n", text);

    return 0;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    // install console REPL environment
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &s_repl));
    my_args.text = arg_str1(NULL, NULL, "<txt>", "TEXT");
    my_args.end = arg_end(1);
    const esp_console_cmd_t wifi_connect_cmd = {
            .command = "config",
            .help = "conf",
            .hint = NULL,
            .func = &do_parse,
            .argtable = &my_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_connect_cmd) );
//    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    test_connectivity();
}
