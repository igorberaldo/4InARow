#include <8051.h>
#include <string.h>
#include <stdlib.h>

#define LCD P1
#define RS P3_5
#define RW P3_4
#define E P3_6
#define E1 P3_7

/* //Variable declaration */
unsigned char position = 0, leds;
unsigned char player = 0x07, winner = 0x00;
unsigned char table[6][7] = { {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0},
                              {0, 0, 0, 0, 0, 0, 0}};
/* // isPlaying is used to make sure that all hardware is configured, and isGameOn is used to check whetheer the game is on or not */
unsigned char isPlaying = 0, isGameOn = 0;
unsigned char display = 0;

/* //Function prototypes */
void lcdWrite(unsigned char valor, unsigned char TipoDados);
void lcdConfig(void);
void lcdCgram(void);
void lcdCursor(unsigned char pos, unsigned char line);
void lcdClear(void);
void inicio(void);
int checkTable(void);
void resetTable(void);
void checkWinner(void);
void checkDiagonal(int);
void delay_100us(void);
void delay_1ms(void);
void delay_ms(unsigned int _delay);

/* //Extern interrupt function 1, verifies wheter the game is on, updates the cursor position and the current column  */
void avanco_isr(void) __interrupt(2)
{
    if(isGameOn == 1)
    {
    	P2 ^= 0xFF;
        lcdCursor(position, 0);
        lcdWrite(0x20, 1);
        position++;
        if(position > 6)	position = 0;
        display = 0;
        lcdCursor(position, 0);
        lcdWrite(player, 1);
    }
}

/* //Extern interrupt function 0, sends the command to drop the piece in the current column if there is space for it, and verifies whether there is a winner using the checkWinner function */
void soltar_isr(void) __interrupt(0)
{
    if(isGameOn == 1)
    {
    	P2 ^= 0xFF;
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
            checkWinner();
        }
    }
}

/* //Serial interrupt function it is used to start and stop the game sending any command through the serial interface */
void serial_isr(void) __interrupt(4)
{
    unsigned char dado;
    RI = 0;
    dado = SBUF;
    if(isGameOn == 0)
    {
        isPlaying = 0;
        lcdClear();
        isPlaying =1;
        isGameOn = 1;
        display = 0;
        lcdCursor(0,0);
        lcdWrite(player, 1);
        display = 1;
        display = 0;
    }
    else
    {
        isGameOn = 0;
        inicio();
        resetTable();
    }
}

/* //Main function it is used to initialize all hardware needed */
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
    while (1)
    {
    	if(!isPlaying)
    	{
    	    P2 = ~leds;
    	    leds = leds << 1;
    	    if(leds == 0x00) leds = 0x01;
    	}
    	delay_ms(20);
    }
}

/* //This function is used to clean the memory table when the game is finished or interrupted */
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

/* //This function is responsible to check whether or not there is a winner in the game and if so it set the winner message and stops the game */
void checkWinner(void)
{
    if(winner != 0x00)
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
        isPlaying = 0;
        leds = 0x01;
    }
}

/* //This function checks Função auxiliar, uma das principais, ela checa qual deve ser o caracter a ser escrito e a linha quando a função soltar é
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

/*This function checks the diagonal from the last dropped piece, and it receives the piece's row number */
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

/* //This function is responsible to configure and show the main menu, which shows name of the game and the developer */
void inicio(void)
{
    if(isGameOn == 0)
    {
    	P2 = 0xFF;
        isGameOn = 0;
        isPlaying = 0;
        lcdClear();
        isPlaying = 1;
        display = 0;
        lcdCursor(1,0);
        lcdWrite('4', 1);
        lcdWrite(0x20, 1);
        lcdWrite('i', 1);
        lcdWrite('n', 1);
        lcdWrite(0x20, 1);
        lcdWrite('a', 1);
        lcdWrite(0x20, 1);
        lcdWrite('r', 1);
        lcdWrite('o', 1);
        lcdWrite('w', 1);
        lcdWrite(0x20, 1);
        lcdWrite('g', 1);
        lcdWrite('a', 1);
        lcdWrite('m', 1);
        lcdWrite('e', 1);
        display = 1;
        lcdCursor(0,0);
        lcdWrite('D', 1);
        lcdWrite('e', 1);
        lcdWrite('v', 1);
        lcdWrite('e', 1);
        lcdWrite('l', 1);
        lcdWrite('o', 1);
        lcdWrite('p', 1);
        lcdWrite('e', 1);
        lcdWrite('d', 1);
        lcdWrite(0x20, 1);
        lcdWrite('b', 1);
        lcdWrite('y', 1);
        display = 1;
        lcdCursor(0,1);
        lcdWrite('I', 1);
        lcdWrite('g', 1);
        lcdWrite('o', 1);
        lcdWrite('r', 1);
        lcdWrite(0x20, 1);
        lcdWrite('R', 1);
        lcdWrite('.', 1);
        lcdWrite(0x20, 1);
        lcdWrite('B', 1);
        lcdWrite('e', 1);
        lcdWrite('r', 1);
        lcdWrite('a', 1);
        lcdWrite('l', 1);
        lcdWrite('d', 1);
        lcdWrite('o', 1);
    }
}

/* //This function is used to configure both LDC displays */
void lcdConfig(void)
{
    RW = 0;
    lcdWrite(0x38, 0);
    lcdWrite(0x0D, 0);
    lcdWrite(0x01, 0);
    lcdWrite(0x06, 0);
    lcdWrite(0x80, 0);
}

/* //This function is used to write the special characteres in the CGRAM memory of both displays */
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

/* //This function send the command to the display to clean it */
void lcdClear()
{
    lcdWrite(0x01,0);
}

/* //this function is used to write in the display
@Param: val is used to receive the value that will be written in the port that is connected to the displays
@Param: cntrl is used to receive the value that will be attributed to the control pin
*/
void lcdWrite(unsigned char val, unsigned char cntrl)
{
    if (cntrl == 1)
        RS = 1;
    else
        RS = 0;
    LCD = val;
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

/* //This function is used to select a position in the display 
@Param: pos receives the column value
@Param: line receives the line value */
void lcdCursor(unsigned char pos, unsigned char line)
{
    pos &= 0x1F;
    if(line == 0)
        pos |= 0x80;
    else
        pos |= 0xC0;
    lcdWrite(pos,0);
}

/*This function uses assembly registers to create a delay of a 100 microseconds*/
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

/*This function uses assembly registers and the delay_100us to create a delay of a 1 milisecond*/
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

/*This function uses the delay_1ms to create a custom delay
@Param: _delay receives the amount of miliseconds that will be delayed*/
void delay_ms(unsigned int _delay)
{
	if(P0_0 == 1)	while(_delay--) delay_1ms();
}

