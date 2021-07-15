#include <8051.h>
#include <string.h>
#include <stdlib.h>

#define LCD P1
#define RS P3_5
#define RW P3_4
#define E P3_6
#define E1 P3_7

/* //Definição das variaveis globais onde table serve para guardar o estado atual do jogo, usando o endereço correspondente da CGRAM */
unsigned char position = 0, mult = 0;
int time = 0;
unsigned char player = 0x07, winner = 0x00;
unsigned char table[6][7] = { {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0}};
/* // isPlaying é usado para dizer se os perifericos já foram configurados ou não, isGameOn é usado para dizer que o jogo esta rolando */
unsigned char isPlaying = 0, isGameOn = 0;
unsigned char display = 0;

/* //Protótipo de funções */
void lcdWrite(unsigned char valor, unsigned char TipoDados);
void lcdConfig(void);
void lcdCgram(void);
void lcdCursor(unsigned char pos, unsigned char line);
void lcdClear(void);
void inicio(void);
int checkTable(void);
void resetTable(void);
void checkWinner(unsigned char);
void checkDiagonal(int);
void delay_100us(void);
void delay_1ms(void);
void delay_ms(unsigned int _delay);


/* //Função de interrupção externa 1, verifica se o jogo está rolando e atualiza a posição do cursor que seleciona a coluna que
// a peça será largada */
void avanco_isr(void) __interrupt(2)
{
    if(isGameOn == 1)
    {
        lcdCursor(position, 0);
        lcdWrite(0x20, 1);
        position++;
        if(position > 6)	position = 0;
        display = 0;
        lcdCursor(position, 0);
        lcdWrite(player, 1);
    }
}

/* //Função de interrupção externa 0, envia o comando para soltar a peça na coluna selecionada, e verifica as condições do jogo
//com auxilio da função checkTable, caso ela retorne 0 significa que a coluna esta cheia e não pode adicionar outra peça */
void soltar_isr(void) __interrupt(0)
{
    if(isGameOn == 1)
    {
        if(checkTable() == 1)
        {
            display = 0;
            lcdCursor(position, 0);
            lcdWrite(' ', 1);
            position = 0;
            if(player == 0x07)	player = 0x06;
            else	player = 0x07;
            lcdCursor(0,0);
            lcdWrite(player, 1);
            checkWinner(winner);
        }
    }
}

/* //Função de interrupção do timer faz o calculo para que a interrupção ocorra a cada 1 ms e incrementa o valor do time,
//que serve para dizer o tempo de jogo */
void timer_isr(void) __interrupt(3)
{
    unsigned char a = 12;
    if(isGameOn == 1)
    {
        mult++;
        if(mult >= a)
        {
            mult = 0;
            time++;
            display = 1;
            lcdCursor(13,1);
            if(time > 9)
            {
                if(time > 99)
                {
                    if(time > 999)
                    {
                        lcdWrite(time/10 + '0', 1);
                        lcdWrite(time%10 + '0', 1);
                        lcdWrite(time%100 + '0', 1);
                    }
                    else
                    {
                        lcdWrite(time/10 + '0', 1);
                        lcdWrite(time%10 + '0', 1);
                    }
                }
                else
                {
                    lcdWrite(time/10 + '0', 1);
                    lcdWrite(time%10 + '0', 1);
                }
            }
            else	lcdWrite('0' + time, 1);
            display = 0;
        }
    }
}

/* //Função de interrupção serial serve para parar ou iniciar o jogo dependendo do valor recebido no SBUF */
void serial_isr(void) __interrupt(4)
{
    unsigned char dado;
    RI = 0;
    dado = SBUF;
    if(dado == 0x01)
    {
        isPlaying = 0;
        lcdClear();
        isPlaying =1;
        isGameOn = 1;
        display = 0;
        lcdCursor(0,0);
        lcdWrite(player, 1);
        display = 1;
        lcdCursor(13, 1);
        lcdWrite('0', 1);
        display = 0;
    }
    else if(dado == 0x02)
    {
        isGameOn = 0;
        inicio();
        resetTable();
    }
}

/* //Função principal, usada somente no inicio da execução para inicializar todos os perifericos */
void main(void)
{
    lcdConfig();
    lcdCgram();
    isPlaying = 1;
    inicio();
    SCON = 0x50;
    IE = 0x9D;
    IT0 = 1;
    IT1 = 1;
    TMOD = 0x20;
    TH1 = 253;
    TR1 = 1;
    PS = 1;
    while (1);
}

/* //Função auxiliar que retorna a variavel table para seu valor inicial, quando o jogo acaba ou é interrompido */
void resetTable(void)
{
    unsigned char i, j;
    for(i = 0; i < 6; i++)
    {
        for(j = 0; j < 7; j++)
        {
            table[i][j] = 0;
        }
    }
}

/* //Função auxiliar que verifica o vencedor, e caso haja um para o jogo volta para o menu inicial e diz qual jogador que ganhou */
void checkWinner(unsigned char val)
{
    if(val != 0x00)
    {
        resetTable();
        isGameOn = 0;
        inicio();
        display = 0;
        lcdCursor(0, 1);
        lcdWrite(winner, 1);
        lcdWrite(0x20, 1);
        lcdWrite('W', 1);
        lcdWrite('o', 1);
        lcdWrite('n', 1);
        winner = 0x00;
        isGameOn = 0;
    }
}

/* //Função auxiliar, uma das principais, ela checa qual deve ser o caracter a ser escrito e a linha quando a função soltar é
//ativada, ela também checa se existe 4 peças identicas na vertical e/ou horizontal a partir da ultima peça solta para determinar
//se existe um vencedor.
//caso retorne 0 significa que aquela coluna selecionada está cheia e a peça não deve ser solta */
int checkTable(void)
{
    signed char lPieces = 0, rPieces = 0, pieces, j, i = 2;
    for(i = 5; i >= 0; i--)
    {
        if(i > 1)
        {
            display = 1;
            if(i >= 4)	lcdCursor(position, 1);
            else	lcdCursor(position, 0);
        }
        else
        {
            display = 0;
            lcdCursor(position, 1);
        }
        if(table[i][position] == 0 && i % 2 != 0)
        {
            if(player == 0x07)
            {
                table[i][position] = 1;
                for(j = position - 1; j >= 0; j--)
                {
                    if(table[i][j] == 1)
                        lPieces++;
                    else	break;
                }
                for(j = position + 1; j < 7; j++)
                {
                    if(table[i][j] == 1)
                        rPieces++;
                    else	break;
                }
                pieces = lPieces + 1 + rPieces;
                if(pieces >= 4)	winner = 0x07;
                lcdWrite(0x04, 1);
            }
            else if(player == 0x06)
            {
                table[i][position] = 2;
                for(j = position - 1; j >= 0; j--)
                {
                    if(table[i][j] == 2)
                        lPieces++;
                    else	break;
                }
                for(j = position + 1; j < 7; j++)
                {
                    if(table[i][j] == 2)
                        rPieces++;
                    else	break;
                }
                pieces = lPieces + 1 + rPieces;
                if(pieces >= 4)	winner = 0x06;
                lcdWrite(0x05, 1);
            }
            checkDiagonal(i);
            return 1;
        }
        else if(table[i][position] == 0)
        {
            if(table[i+1][position] == 1)
            {
                if(player == 0x07)
                {
                    table[i][position] = 1;
                    for(j = position - 1; j >= 0; j--)
                    {
                        if(table[i][j] == 1)
                            lPieces++;
                        else	break;
                    }
                    for(j = position + 1; j < 7; j++)
                    {
                        if(table[i][j] == 1)
                            rPieces++;
                        else	break;
                    }
                    pieces = lPieces + 1 + rPieces;
                    lcdWrite(0x03, 1);
                    if((i <= 2 && table[i+1][position] == 1 && table[i+2][position] == 1 && table[i+3][position] == 1) || pieces >= 4)
                        winner = 0x07;
                }
                else if(player == 0x06)
                {
                    table[i][position] = 2;
                    for(j = position - 1; j >= 0; j--)
                    {
                        if(table[i][j] == 2)
                            lPieces++;
                        else	break;
                    }
                    for(j = position + 1; j < 7; j++)
                    {
                        if(table[i][j] == 2)
                            rPieces++;
                        else	break;
                    }
                    pieces = lPieces + 1 + rPieces;
                    if(pieces >= 4)	winner = 0x06;
                    lcdWrite(0x01, 1);
                }
                checkDiagonal(i);
                return 1;
            }
            else if(table[i+1][position] == 2)
            {
                if(player == 0x07)
                {
                    table[i][position] = 1;
                    for(j = position - 1; j >= 0; j--)
                    {
                        if(table[i][j] == 1)
                            lPieces++;
                        else	break;
                    }
                    for(j = position + 1; j < 7; j++)
                    {
                        if(table[i][j] == 1)
                            rPieces++;
                        else	break;
                    }
                    pieces = lPieces + 1 + rPieces;
                    if(pieces >= 4)	winner = 0x07;
                    lcdWrite(0x00, 1);
                }
                else if(player == 0x06)
                {
                    table[i][position] = 2;
                    for(j = position - 1; j >= 0; j--)
                    {
                        if(table[i][j] == 2)
                            lPieces++;
                        else	break;
                    }
                    for(j = position + 1; j < 7; j++)
                    {
                        if(table[i][j] == 2)
                            rPieces++;
                        else	break;
                    }
                    pieces = lPieces + 1 + rPieces;
                    lcdWrite(0x02, 1);
                    if((i <= 2 && table[i+1][position] == 2 && table[i+2][position] == 2 && table[i+3][position] == 2) || pieces >= 4)
                        winner = 0x06;
                }
                checkDiagonal(i);
                return 1;
            }
        }
    }
    return 0;
}

/*Função para checar a diagonal da ultima peça solta*/
void checkDiagonal(int i)
{
    unsigned char val;
    if(player == 0x07)	val = 1;
    else	val = 2;
    if((table[i+1][position+1] == val && table[i+2][position+2]  == val && table[i+3][position+3] == val) ||
            (table[i-1][position-1]  == val && table[i-2][position-2]  == val && table[i-3][position-3]  == val) ||
            (table[i+1][position+1] == val && table[i+2][position+2]  == val && table[i-1][position-1] == val) ||
            (table[i-1][position-1] == val && table[i-2][position-2]  == val && table[i+1][position+1]  == val) )
        winner = player;
    else if((table[i-1][position+1] == val && table[i-2][position+2]  == val && table[i-3][position+3] == val) ||
            (table[i+1][position-1]  == val && table[i+2][position-2]  == val && table[i+3][position-3]  == val) ||
            (table[i-1][position+1] == val && table[i-2][position+2]  == val && table[i+1][position-1] == val) ||
            (table[i+1][position-1]  == val && table[i+2][position-2]  == val && table[i-1][position+1]  == val))
        winner = player;
}

/* //Função auxiliar que forma o menu inicial do jogo, o qual mostra o nome do jogo e dos participantes do grupo */
void inicio(void)
{
    if(isGameOn == 0)
    {
        time = 0;
        mult = 0;
        isGameOn = 0;
        isPlaying = 0;
        lcdClear();
        isPlaying = 1;
        display = 0;
        lcdCursor(1,0);
        lcdWrite('J', 1);
        lcdWrite('o', 1);
        lcdWrite('g', 1);
        lcdWrite('o', 1);
        lcdWrite(0x20, 1);
        lcdWrite('4', 1);
        lcdWrite(0x20, 1);
        lcdWrite('e', 1);
        lcdWrite('m', 1);
        lcdWrite(0x20, 1);
        lcdWrite('l', 1);
        lcdWrite('i', 1);
        lcdWrite('n', 1);
        lcdWrite('h', 1);
        lcdWrite('a', 1);
        display = 1;
        lcdCursor(0,0);
        lcdWrite('R', 1);
        lcdWrite('a', 1);
        lcdWrite('f', 1);
        lcdWrite('a', 1);
        lcdWrite('e', 1);
        lcdWrite('l', 1);
        lcdWrite(0x20, 1);
        lcdWrite('F', 1);
        lcdWrite('r', 1);
        lcdWrite('e', 1);
        lcdWrite('i', 1);
        lcdWrite('r', 1);
        lcdWrite('e', 1);
        display = 1;
        lcdCursor(0,1);
        lcdWrite('D', 1);
        lcdWrite('a', 1);
        lcdWrite('n', 1);
        lcdWrite('i', 1);
        lcdWrite('e', 1);
        lcdWrite('l', 1);
        lcdWrite(0x20, 1);
        lcdWrite('F', 1);
        lcdWrite('e', 1);
        lcdWrite('r', 1);
        lcdWrite('n', 1);
        lcdWrite('a', 1);
        lcdWrite('n', 1);
        lcdWrite('d', 1);
        lcdWrite('e', 1);
        lcdWrite('s', 1);
    }
}

/* //Função auxiliar para configurar os displays, ela usa a função lcdWrite para escreve os valores necessários de configuração */
void lcdConfig(void)
{
    RW = 0;
    lcdWrite(0x38, 0);
    lcdWrite(0x0D, 0);
    lcdWrite(0x01, 0);
    lcdWrite(0x06, 0);
    lcdWrite(0x80, 0);
}

/* //Função auxiliar para escrever os caracteres especiais na CGRAM, ela usa a função lcdWrite para realizar seu trabalho */
void lcdCgram(void)
{
    lcdWrite(0x40, 0);
    lcdWrite(0x1F, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x48, 0);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x50, 0);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x58, 0);
    lcdWrite(0x1F, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x60, 0);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x68, 0);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x70, 0);
    lcdWrite(0x00, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x0A, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);

    lcdWrite(0x78, 0);
    lcdWrite(0x00, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x0A, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x11, 1);
    lcdWrite(0x1F, 1);
    lcdWrite(0x00, 1);
    lcdWrite(0x00, 1);
}

/* //Função auxiliar para limpar o display, ela usa a função lcdWrite para enviar o código de limpeza para o display */
void lcdClear()
{
    lcdWrite(0x01,0);
}

/* //Função auxiliar usada para escrever no display, recebe o valor que será escrito no port que está conectado no display e
//recebe o tipo de dados para mudar o estados do pino de controle para escrita ou configuração */
void lcdWrite(unsigned char valor, unsigned char TipoDados)
{
    if (TipoDados == 1)
        RS = 1;
    else
        RS = 0;
    LCD = valor;
    if(isPlaying == 1)
    {
        if(display == 0)
        {
            E = 1;
            delay_ms(1);
            E = 0;
        }
        else
        {
            E1 = 1;
            delay_ms(1);
            E1 = 0;
        }
    }
    else
    {
        E = 1;
        E1 = 1;
        delay_ms(1);
        E = 0;
        E1 = 0;
    }
}

/* //Função auxiliar usada para selecionar a linha e a coluna que será escrito o caracter, usando a lcdWrite */
void lcdCursor(unsigned char pos, unsigned char line)
{
    pos &= 0x1F;
    if(line == 0)
        pos |= 0x80;
    else
        pos |= 0xC0;
    lcdWrite(pos,0);
}

void delay_100us(void)
{
  __asm
  push ar7
  mov r7, #41
loop_delay:
  djnz r7, loop_delay
  pop ar7
  __endasm;
}

void delay_1ms(void)
{
  __asm
  push ar6
  mov r6, #10
wait_delay_1ms:
  __endasm;
  delay_100us();
  __asm
  djnz r6, wait_delay_1ms
  pop ar6
  __endasm;
}

void delay_ms(unsigned int _delay)
{
	if(P0_0 == 1)	while(_delay--) delay_1ms();
}

