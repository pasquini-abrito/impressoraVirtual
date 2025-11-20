#include <stdio.h>
#include <windows.h>
#include "config_manager.h"

// Declarações de funções externas
BOOL install_virtual_printer();
BOOL is_printer_installed();
DWORD WINAPI virtual_port_monitor(LPVOID lpParam);

int main() {
    printf("=== Driver Virtual ZPL2 -> Elgin USB ===\n\n");
    
    // Carrega configurações
    PrinterConfig config;
    load_config(&config);
    
    printf("Configurações carregadas:\n");
    printf("- Impressora Virtual: %s\n", config.virtual_printer_name);
    printf("- Porta Virtual: %s\n", config.virtual_port);
    printf("- Monitor Interval: %dms\n", config.monitor_interval);
    
    // Verifica se já está instalada
    if (is_printer_installed()) {
        printf("\nImpressora virtual já está instalada\n");
    } else {
        // Instala impressora virtual
        printf("\nInstalando impressora virtual...\n");
        if (!install_virtual_printer()) {
            printf("Falha na instalação da impressora virtual\n");
            return 1;
        }
    }
    
    // Inicia monitor de porta virtual
    printf("Iniciando monitor de porta virtual...\n");
    CreateThread(NULL, 0, virtual_port_monitor, NULL, 0, NULL);
    
    printf("\n=== Sistema instalado com sucesso! ===\n");
    printf("Impressora disponível: 'Zebra Virtual (Redirect to Elgin)'\n");
    printf("Porta: VIRTUAL001:\n");
    printf("O sistema está monitorando jobs de impressão...\n\n");
    
    printf("Pressione ESC para sair...\n");
    
    // Loop principal
    while (1) {
        if (GetAsyncKeyState(VK_ESCAPE)) {
            break;
        }
        Sleep(1000);
    }
    
    printf("Encerrando driver virtual...\n");
    return 0;
}