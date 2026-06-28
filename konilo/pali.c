/***************************************************************
                      crc's        _ _
                       _ __   __ _| (_)
                      | '_ \ / _` | | |
                      | |_) | (_| | | |
                      | .__/ \__,_|_|_|
                      |_|     assembler
****************************************************************

The pali assembler is used to create images for the ilo virtual
computer. It use the literate unu format, with the assembly code
in dedicated code blocks, and commentary outside these blocks.

Code blocks start and end with a ~~~ sequence. They contain a
series of lines, each of which consists of a single character
directive, a space, and any parameters the directive requires.

The directives are:

+---+----------------------------------------------------------+
| i | process parameter as instruction bundle                  |
| o | set origin/offset in memory space                        |
| * | reserve parameter cells of data in memory                |
| r | parameter is a named item, assemble a pointer to it      |
| - | alias for `r`                                            |
| d | parameter is a decimal value, assemble it inline         |
| c | parameter is a comment to be ignored                     |
| : | parameter is a label name                                |
| s | parameter is a string, assemble as length prefixed       |
| z | parameter is a string, assemble as null-terminated       |
+---+----------------------------------------------------------+

The pali assembler is a two pass design. The first pass will
scan through the code, recording any labels and their offsets
in the image. The second pass actually assembles the data,
instructions, and resolves any references to labels.

***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*Handler)(char *);
void unu(char *, Handler);

char source[1025];
int np, here, Labels[1024], Pointers[1024], target[65536];

void red()   { printf("\033[0;31m"); }
void cyan()  { printf("\033[0;36m"); }
void plain() { printf("\033[0;0m");  }

void read_line(FILE *file, char *line_buffer) {
  int ch = getc(file);
  int count = 0;
  while ((ch != '\n') && (ch != EOF)) {
    line_buffer[count++] = ch;
    ch = getc(file);
  }
  line_buffer[count] = '\0';
}

void unu(char *fname, Handler handler) {
  int inBlock = 0;
  char buffer[4096];
  FILE *fp;
  fp = fopen(fname, "r");
  if (fp == NULL) {
    red(); printf("Unable to load file\n"); plain();
    exit(2);
  }
  while (!feof(fp)) {
    read_line(fp, buffer);
    if (strcmp(buffer, "~~~") == 0) {
      inBlock = (inBlock == 0 ? 1 : 0);
    } else {
      if (inBlock == 1) {
        handler(buffer);
      }
    }
  }
  fclose(fp);
}

int hash(char *s) {
  int c, h = 5381;
  while ((c = *s++)) h = (h * 33) + c;
  return h;
}

void save(const char *filename, int words) {
  FILE *fp;
  if ((fp = fopen(filename, "wb")) == NULL) {
    red(); printf("Unable to save the image!\n"); plain();
    exit(2);
  }
  /* fwrite(&target, sizeof(int), 65536, fp); */
  fwrite(&target, 1, words*4, fp);
  fclose(fp);
}

int lookup(char *name) {
  int n = np;
  int h = hash(name);
  while (n > 0) {
    n--;
    if (Labels[n] == h) return Pointers[n];
  }
  return -1;
}

void add_label(char *name, int slice) {
  if (lookup(name) == -1) {
    Labels[np] = hash(name);
    Pointers[np] = slice;
    np++;
    return;
  }
  red();   printf("Fatal error: ");
  cyan();  printf("%s", name);
  red();   printf(" already defined\n");
  plain(); exit(0);
}


int encode(char *s) {
  int ops[] = { 5861473, 5863578, 5863326,
                5863323, 5863823, 5863722,
                5863716, 5863524, 5863273,
                5863275, 5863282, 5863772,
                5863355, 5863640, 5863589,
                5863424, 5863376, 5863820,
                5863210, 5863821, 5863623,
                5863314, 5863220, 5863686,
                5863980, 5863812, 5863818,
                5863288, 5863297, 5863485, };
  int op = hash(s);
  int i = 0;
  for (i = 0; i <= 30; i++) if (ops[i] == op) return i;
  return 0;
}

void pass1(char *buffer) {
  switch (buffer[0]) {
    case 'c':                                     break;
    case 'o': here = atoi(buffer+2);              break;
    case '*': here += atoi(buffer+2);             break;
    case 's': here = here + strlen(buffer) - 1;   break;
    case 'z': here = here + strlen(buffer) - 1;   break;
    case ':': add_label(buffer+2, here);          break;
    default:  if (strlen(buffer) > 0) here++;     break;
  }
}

void pass2(char *buffer) {
  unsigned int opcode;
  char inst[3] = { 0, 0, 0 };
  switch (buffer[0]) {
    case 'c':                                     break;
    case 'o': here = atoi(buffer+2);              break;
    case 'i': memcpy(inst, buffer + 8, 2);
              opcode = encode(inst) << 8;
              memcpy(inst, buffer + 6, 2);
              opcode += encode(inst);
              opcode = opcode << 8;
              memcpy(inst, buffer + 4, 2);
              opcode += encode(inst);
              opcode = opcode << 8;
              memcpy(inst, buffer + 2, 2);
              opcode += encode(inst);
              target[here++] = opcode;
              break;
    case 'd': target[here++] = atoi(buffer+2);    break;
    case '*': here += atoi(buffer+2);             break;
    case 's': opcode = 2;
              target[here++] = strlen(buffer) - 2;
              while (opcode < strlen(buffer))
                target[here++] = buffer[opcode++];
                                                  break;
    case 'z': opcode = 2;
              while (opcode < strlen(buffer))
                target[here++] = buffer[opcode++];
              target[here++] = 0;
                                                  break;
    case 'r':
    case '-': target[here++] = lookup(buffer+2);
              if (lookup(buffer+2) == -1) {
                red(); printf("Lookup failed: ");
                cyan(); printf("%s\n", buffer+2);
                plain();
              }
                                                  break;
    case ':':                                     break;
    default:  if (strlen(buffer) > 0) here++;     break;
  }
}

int main(int argc, char **argv) {
  if (argc > 2) {
    np = 0;
    here = 0; unu(argv[1], &pass1);
    here = 0; unu(argv[1], &pass2);
    save(argv[2], here);
    printf("%d words (%d bytes) used\n", here, here * 4);
    return 0;
  }
  red(); printf("Usage: %s file.pali out.rom.\n", argv[0]); plain();
  return -1;
}
