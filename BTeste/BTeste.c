#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

// --- Configurações para o DISPOSITIVO A ---
#define WIFI_SSID "brisa-BrunnoIza"
#define WIFI_PASSWORD "bslyhcb6"
#define UDP_PORT 4444
#define DEBOUNCE_DELAY_MS 250

const int BUTTON_PIN = 5;
const int LED_PIN = 13;

#define OTHER_PICO_IP "192.168.0.14" // <-- IP do Dispositivo A

volatile bool button_action_pending = false;
volatile uint32_t last_press_time = 0;

void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        printf("Pacote UDP recebido de %s\n", ip4addr_ntoa(addr));
        if (strncmp((char *)p->payload, "TOGGLE", p->len) == 0) {
            gpio_put(LED_PIN, !gpio_get_out_level(LED_PIN)); // Inverte o estado do LED
        }
        pbuf_free(p);
    }
}

void send_udp_message(struct udp_pcb *pcb, const char *message) {
    ip_addr_t dest_addr;
    ip4addr_aton(OTHER_PICO_IP, &dest_addr);
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(message), PBUF_RAM);
    if (p) {
        memcpy(p->payload, message, strlen(message));
        udp_sendto(pcb, p, &dest_addr, UDP_PORT);
        pbuf_free(p);
    }
}

void button_callback(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if ((current_time - last_press_time) > DEBOUNCE_DELAY_MS) {
        button_action_pending = true;
        last_press_time = current_time;
    }
}

int main() {
    stdio_init_all();

    // =================================================================
    //  ATRASO ESTRATÉGICO: ESPERA 5 SEGUNDOS ANTES DE CONTINUAR
    //  Isso lhe dá tempo para abrir o Monitor Serial.
    // =================================================================
    for (int i = 5; i > 0; i--) {
        printf("Aguardando para iniciar... %d\n", i);
        sleep_ms(1000);
    }

    printf("\n--- Iniciando Dispositivo A ---\n");

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    if (cyw43_arch_init()) {
        printf("ERRO: Falha ao inicializar o módulo Wi-Fi.\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Tentando conectar à rede: %s\n", WIFI_SSID);

    // Tenta conectar por 30 segundos
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("ERRO: Falha ao conectar ao Wi-Fi. Verifique o nome/senha da rede.\n");
        // Pisca o LED rapidamente para indicar erro de conexão
        while(true) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(150);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(150);
        }
    }
    printf("SUCESSO: Conectado! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    // Acende o LED onboard para indicar que a conexão foi um sucesso
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    struct udp_pcb *pcb = udp_new();
    if (pcb) {
        udp_bind(pcb, IP_ADDR_ANY, UDP_PORT);
        udp_recv(pcb, udp_recv_callback, NULL);
    }

    while (true) {
        if (button_action_pending) {
            printf("Botao pressionado! Enviando 'TOGGLE' para %s\n", OTHER_PICO_IP);
            send_udp_message(pcb, "TOGGLE");
            button_action_pending = false;
        }
        cyw43_arch_poll(); // Mantém a conexão ativa
        // sleep_ms(1); // Não é estritamente necessário aqui
    }
}