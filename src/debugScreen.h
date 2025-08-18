#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

// Inclui a biblioteca de argumentos variáveis, necessária para psvDebugScreenPrintf
#include <stdarg.h>

/**
 * @brief Inicializa a tela de depuração.
 *
 * Esta função deve ser chamada uma vez no início do seu programa antes de usar
 * qualquer outra função de psvDebugScreen. Ela configura o framebuffer e
 * o ambiente necessário para renderizar texto na tela.
 *
 * @return Retorna um valor inteiro, geralmente 0 em caso de sucesso e um valor
 * negativo em caso de falha.
 */
int psvDebugScreenInit();

/**
 * @brief Imprime texto formatado na tela de depuração.
 *
 * Funciona de forma análoga à função printf padrão da biblioteca C, permitindo
 * que você imprima texto e variáveis formatadas na tela. A saída de texto
 * será automaticamente quebrada para a próxima linha se exceder a largura da tela.
 *
 * @param fmt A string de formato, que contém o texto a ser impresso e os
 * especificadores de formato (como %d, %s, %f).
 * @param ... Os argumentos variáveis que correspondem aos especificadores de
 * formato na string fmt.
 */
void psvDebugScreenPrintf(const char *fmt, ...);

#endif // DEBUG_SCREEN_H