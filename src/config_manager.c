#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    char virtual_printer_name[256];
    char real_printer_name[256];
    char virtual_port[32];
    int monitor_interval;
    int auto_reconnect;
} PrinterConfig;

// Configurações padrão
PrinterConfig default_config = {
    "Zebra Virtual (Redirect to Elgin)",
    "ELGIN-I9",
    "VIRTUAL001:",
    1000,
    1
};

// Salva configurações no registro
BOOL save_config(const PrinterConfig* config) {
    HKEY hKey;
    const char* keyPath = "SOFTWARE\\ZebraVirtualDriver";
    
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, keyPath, 0, NULL, 0, 
                       KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        
        RegSetValueEx(hKey, "VirtualPrinterName", 0, REG_SZ, 
                     (BYTE*)config->virtual_printer_name, 
                     strlen(config->virtual_printer_name) + 1);
        
        RegSetValueEx(hKey, "RealPrinterName", 0, REG_SZ,
                     (BYTE*)config->real_printer_name,
                     strlen(config->real_printer_name) + 1);
        
        RegSetValueEx(hKey, "VirtualPort", 0, REG_SZ,
                     (BYTE*)config->virtual_port,
                     strlen(config->virtual_port) + 1);
        
        DWORD value = config->monitor_interval;
        RegSetValueEx(hKey, "MonitorInterval", 0, REG_DWORD, 
                     (BYTE*)&value, sizeof(value));
        
        value = config->auto_reconnect;
        RegSetValueEx(hKey, "AutoReconnect", 0, REG_DWORD, 
                     (BYTE*)&value, sizeof(value));
        
        RegCloseKey(hKey);
        return TRUE;
    }
    
    return FALSE;
}

// Carrega configurações do registro
BOOL load_config(PrinterConfig* config) {
    HKEY hKey;
    const char* keyPath = "SOFTWARE\\ZebraVirtualDriver";
    
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type, size;
        
        // Virtual Printer Name
        size = sizeof(config->virtual_printer_name);
        if (RegQueryValueEx(hKey, "VirtualPrinterName", NULL, &type,
                           (BYTE*)config->virtual_printer_name, &size) != ERROR_SUCCESS) {
            strcpy(config->virtual_printer_name, default_config.virtual_printer_name);
        }
        
        // Real Printer Name
        size = sizeof(config->real_printer_name);
        if (RegQueryValueEx(hKey, "RealPrinterName", NULL, &type,
                           (BYTE*)config->real_printer_name, &size) != ERROR_SUCCESS) {
            strcpy(config->real_printer_name, default_config.real_printer_name);
        }
        
        // Virtual Port
        size = sizeof(config->virtual_port);
        if (RegQueryValueEx(hKey, "VirtualPort", NULL, &type,
                           (BYTE*)config->virtual_port, &size) != ERROR_SUCCESS) {
            strcpy(config->virtual_port, default_config.virtual_port);
        }
        
        // Monitor Interval
        DWORD dwValue;
        size = sizeof(dwValue);
        if (RegQueryValueEx(hKey, "MonitorInterval", NULL, &type, (BYTE*)&dwValue, &size) == ERROR_SUCCESS) {
            config->monitor_interval = dwValue;
        } else {
            config->monitor_interval = default_config.monitor_interval;
        }
        
        // Auto Reconnect
        size = sizeof(dwValue);
        if (RegQueryValueEx(hKey, "AutoReconnect", NULL, &type, (BYTE*)&dwValue, &size) == ERROR_SUCCESS) {
            config->auto_reconnect = dwValue;
        } else {
            config->auto_reconnect = default_config.auto_reconnect;
        }
        
        RegCloseKey(hKey);
        return TRUE;
    }
    
    // Se não existir, usa padrão e salva
    memcpy(config, &default_config, sizeof(PrinterConfig));
    save_config(config);
    return FALSE;
}