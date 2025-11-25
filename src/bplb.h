// bplb.h - Biblioteca para geração de comandos BPLB para impressora Elgin L42
#ifndef BPLB_H
#define BPLB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definições gerais
#define BPLB_LF "\n"  // Line Feed obrigatório no final de cada comando
#define BPLB_STX "\x02"  // Start of Text para comandos especiais
#define BPLB_UNIT_MM 8   // 8 pontos = 1mm

// Enumerações para parâmetros comuns
typedef enum {
    ROTATION_0 = 0,    // Sem rotação
    ROTATION_90 = 1,   // 90° horário
    ROTATION_180 = 2,  // 180° horário
    ROTATION_270 = 3   // 270° horário
} bplb_rotation_t;

typedef enum {
    ALIGN_LEFT = 'L',
    ALIGN_RIGHT = 'R',
    ALIGN_CENTER = 'C',
    ALIGN_NONE = 'N'
} bplb_align_t;

typedef enum {
    REVERSE_NO = 'N',
    REVERSE_YES = 'R'
} bplb_reverse_t;

typedef enum {
    HUMAN_READABLE_NO = 'N',
    HUMAN_READABLE_YES = 'B'
} bplb_human_readable_t;

typedef enum {
    DIRECTION_TOP = 'T',    // Impressão de cima para baixo
    DIRECTION_BOTTOM = 'B'  // Impressão de baixo para cima
} bplb_direction_t;

// Estrutura para configuração básica da impressora
typedef struct {
    int density;        // 0-15
    int speed;          // 1-3 (polegadas/segundo)
    int label_height;   // em pontos
    int label_gap;      // em pontos (0 para contínuo)
    int label_width;    // em pontos
    bplb_direction_t direction;
} bplb_config_t;

// Protótipos das funções principais
void bplb_init(bplb_config_t *config);
void bplb_clear_buffer(FILE *fp);
void bplb_set_density(FILE *fp, int density);
void bplb_set_speed(FILE *fp, int speed);
void bplb_set_label_size(FILE *fp, int height, int gap);
void bplb_set_label_width(FILE *fp, int width);
void bplb_set_direction(FILE *fp, bplb_direction_t direction);
void bplb_enable_backfeed(FILE *fp);
void bplb_disable_backfeed(FILE *fp);

// Comandos de impressão
void bplb_text(FILE *fp, int x, int y, bplb_rotation_t rotation, 
               char font, int h_mult, int v_mult, 
               bplb_reverse_t reverse, const char *text);

void bplb_barcode_1d(FILE *fp, int x, int y, bplb_rotation_t rotation,
                     const char *symbology, int thin_width, int thick_width,
                     int height, bplb_human_readable_t human_readable,
                     const char *data);

void bplb_barcode_2d_pdf417(FILE *fp, int x, int y, int max_width, int max_height,
                           int error_level, int compression, int module_width,
                           int module_height, int max_rows, int max_cols,
                           int truncate, bplb_rotation_t rotation,
                           const char *data);

void bplb_print(FILE *fp, int copies, int identical_copies);

// Comandos de formulários e variáveis
void bplb_form_save_begin(FILE *fp, const char *form_name);
void bplb_form_save_end(FILE *fp);
void bplb_form_load(FILE *fp, const char *form_name);
void bplb_form_delete(FILE *fp, const char *form_name);
void bplb_form_delete_all(FILE *fp);

void bplb_counter(FILE *fp, int counter_id, int max_digits, 
                  bplb_align_t align, const char *increment, 
                  const char *message);

void bplb_variable(FILE *fp, int var_id, int max_chars, 
                   bplb_align_t align, const char *message);

void bplb_enable_variable_substitution(FILE *fp);

// Comandos de gráficos
void bplb_image_print(FILE *fp, int x, int y, const char *image_name);
void bplb_image_store_begin(FILE *fp, const char *image_name, int size_bytes);
void bplb_image_delete(FILE *fp, const char *image_name);

// Comandos de desenho
void bplb_line_non_overlap(FILE *fp, int x, int y, int width, int height);
void bplb_line_overlap(FILE *fp, int x, int y, int width, int height);
void bplb_white_line(FILE *fp, int x, int y, int width, int height);
void bplb_rectangle(FILE *fp, int x1, int y1, int thickness, int x2, int y2);

// Comandos especiais
void bplb_calibrate(FILE *fp);
void bplb_reset_counter(FILE *fp);
void bplb_factory_reset(FILE *fp);
void bplb_save_as_default(FILE *fp);

// Utilitários
int bplb_mm_to_points(float mm);
float bplb_points_to_mm(int points);

#endif // BPLB_H