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
| R | parameter is a named immediate item, assemble a pointer  |
|   | to it                                                    |
| - | alias for `r`                                            |
| d | parameter is a decimal value, assemble it inline         |
| c | parameter is a comment to be ignored                     |
| : | parameter is a label name                                |
| s | parameter is a string, assemble as length prefixed       |
| z | parameter is a string, assemble as null-terminated       |
| D | parameter is a dictionary entry (name target [*])        |
+---+----------------------------------------------------------+

The pali assembler is a two pass design. The first pass will
scan through the code, recording any labels and their offsets
in the image. The second pass actually assembles the data,
instructions, and resolves any references to labels.

***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*Handler)(char *, int);
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

void strip_comments(char *line) {
  char *hash_pos;
  int len;
  /* Skip comment processing for string directives */
  if (line[0] == 's' || line[0] == 'z') {
    return;
  }
  
  hash_pos = strchr(line, '#');
  /* Only treat # as comment if preceded by whitespace */
  while (hash_pos != NULL) {
    if (hash_pos == line || *(hash_pos - 1) == ' ' || *(hash_pos - 1) == '\t') {
      *hash_pos = '\0';
      break;
    }
    hash_pos = strchr(hash_pos + 1, '#');
  }
  
  /* Remove trailing whitespace */
  len = strlen(line);
  while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t')) {
    line[--len] = '\0';
  }
}

int is_control_flow_instruction(char *inst) {
  /* Check if instruction is one of: ca, cc, ju, cj, re */
  return (strcmp(inst, "ca") == 0 || strcmp(inst, "cc") == 0 || 
          strcmp(inst, "ju") == 0 || strcmp(inst, "cj") == 0 || 
          strcmp(inst, "re") == 0);
}

void validate_instruction_bundle(char *buffer, int line_number) {
  int i;
  if (buffer[0] == 'i' && buffer[1] == ' ') {
    char inst1[3], inst2[3], inst3[3], inst4[3];
    int len = strlen(buffer + 2);
    if (len != 8) {
      red(); printf("Error on line %d: Instruction bundle must be exactly 8 characters (4 instructions x 2 chars each)\n", line_number);
      printf("Found: '"); cyan(); printf("%s", buffer + 2); plain(); printf("' (length %d)\n", len);
      exit(1);
    }
    
    /* Check that all characters are valid instruction characters or dots */
    for (i = 2; i < 10; i++) {
      char c = buffer[i];
      if (c != '.' && (c < 'a' || c > 'z')) {
        red(); printf("Error on line %d: Invalid character in instruction bundle: '", line_number); 
        cyan(); printf("%c", c); plain(); printf("'\n");
        printf("Instruction bundles should contain only lowercase letters and dots\n");
        exit(1);
      }
    }
    
    /* Validate control flow instruction placement
     * Instructions are at positions: 2-3, 4-5, 6-7, 8-9 (0-indexed from buffer start)
     */
    memcpy(inst1, buffer + 2, 2); inst1[2] = '\0';
    memcpy(inst2, buffer + 4, 2); inst2[2] = '\0';
    memcpy(inst3, buffer + 6, 2); inst3[2] = '\0';
    memcpy(inst4, buffer + 8, 2); inst4[2] = '\0';
    
    /* Check each control flow instruction position */
    if (is_control_flow_instruction(inst1)) {
      if (strcmp(inst2, "..") != 0 || strcmp(inst3, "..") != 0 || strcmp(inst4, "..") != 0) {
        red(); printf("Error on line %d: Control flow instruction '", line_number); cyan(); printf("%s", inst1);
        red(); printf("' must be followed by only NOP (..) instructions\n");
        printf("Found bundle: '"); cyan(); printf("%s", buffer + 2); plain(); printf("'\n");
        exit(1);
      }
    }
    
    if (is_control_flow_instruction(inst2)) {
      if (strcmp(inst3, "..") != 0 || strcmp(inst4, "..") != 0) {
        red(); printf("Error on line %d: Control flow instruction '", line_number); cyan(); printf("%s", inst2);
        red(); printf("' must be followed by only NOP (..) instructions\n");
        printf("Found bundle: '"); cyan(); printf("%s", buffer + 2); plain(); printf("'\n");
        exit(1);
      }
    }
    
    if (is_control_flow_instruction(inst3)) {
      if (strcmp(inst4, "..") != 0) {
        red(); printf("Error on line %d: Control flow instruction '", line_number); cyan(); printf("%s", inst3);
        red(); printf("' must be followed by only NOP (..) instructions\n");
        printf("Found bundle: '"); cyan(); printf("%s", buffer + 2); plain(); printf("'\n");
        exit(1);
      }
    }
    
    /* inst4 (last position) can be any control flow instruction without restriction */
  }
}

void unu(char *fname, Handler handler) {
  int inBlock = 0;
  int line_number = 0;
  char buffer[4096];
  FILE *fp;
  fp = fopen(fname, "r");
  if (fp == NULL) {
    red(); printf("Unable to load file\n"); plain();
    exit(2);
  }
  while (!feof(fp)) {
    read_line(fp, buffer);
    line_number++;
    if (strcmp(buffer, "~~~") == 0) {
      inBlock = (inBlock == 0 ? 1 : 0);
    } else {
      if (inBlock == 1) {
        strip_comments(buffer);
        validate_instruction_bundle(buffer, line_number);
        handler(buffer, line_number);
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

int dict_hash(char *s) {
  int c, h = 5381;
  while ((c = *s++)) {
    if (c == 9 || c == ' ') return h; /* Stop at tab or space */
    h = (h * 33) + c;
  }
  return h;
}

int dict_entry_count = 0;

void save() {
  FILE *fp;
  if ((fp = fopen("ilo.rom", "wb")) == NULL) {
    red(); printf("Unable to save the image!\n"); plain();
    exit(2);
  }
  fwrite(&target, sizeof(int), 65536, fp);
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

void pass1(char *buffer, int line_number) {
  switch (buffer[0]) {
    case 'c':                                     break;
    case 'o': here = atoi(buffer+2);              break;
    case '*': here += atoi(buffer+2);             break;
    case 's': here = here + strlen(buffer) - 1;   break;
    case 'z': here = here + strlen(buffer) - 1;   break;
    case ':': add_label(buffer+2, here);          break;
    case 'D': /* Dictionary entry: link, hash, address */
              {
                char label_name[256];
                snprintf(label_name, sizeof(label_name), "DICT_ENTRY_%d", dict_entry_count);
                add_label(label_name, here);
                dict_entry_count++;
                here += 3; /* 3 cells: link, hash, address */
              }
              break;
    default:  if (strlen(buffer) > 0) here++;     break;
  }
}

void pass2(char *buffer, int line_number) {
  unsigned int opcode;
  int addr;
  char inst[3] = { 0, 0, 0 };
  static int dict_pass2_count = 0;
  
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
    case 'R': target[here++] = lookup(buffer+2) * -1;
              if (lookup(buffer+2) == -1) {
                red(); printf("Lookup failed: ");
                cyan(); printf("%s\n", buffer+2);
                plain();
              }
                                                  break;
    case 'D': /* Parse dictionary entry: name target */
              {
                char name[256], target_name[256], immediate[256];
                char *ptr = buffer + 2; /* Skip "D " */
                int field = 0, pos = 0;
                
                /* Clear buffers */
                name[0] = target_name[0] = immediate[0] = '\0';
                
                /* Parse fields separated by tabs or spaces */
                while (*ptr) {
                  if (*ptr == '\t' || *ptr == ' ') {
                    /* End current field */
                    if (field == 0) {
                      name[pos] = '\0'; field = 1; pos = 0;
                    } else if (field == 1) {
                      target_name[pos] = '\0'; field = 2; pos = 0;
                    }
                    /* Skip multiple separators */
                    while (*ptr == '\t' || *ptr == ' ') ptr++;
                    continue;
                  }
                  
                  /* Add character to current field */
                  if (field == 0 && pos < 255) {
                    name[pos++] = *ptr;
                  } else if (field == 1 && pos < 255) {
                    target_name[pos++] = *ptr;
                  } else if (field == 2 && pos < 255) {
                    immediate[pos++] = *ptr;
                  }
                  ptr++;
                }
                
                /* Finalize last field */
                if (field == 1) target_name[pos] = '\0';
                else if (field == 2) immediate[pos] = '\0';
                
                /* Store dictionary entry */
                /* Link: previous entry or 0 for first */
                if (dict_pass2_count == 0) {
                  target[here++] = 0;
                } else {
                  char prev_label[256];
                  snprintf(prev_label, sizeof(prev_label), "DICT_ENTRY_%d", dict_pass2_count - 1);
                  target[here++] = lookup(prev_label);
                }
                
                /* Hash of name */
                target[here++] = dict_hash(name);
                
                /* Address (negative if immediate) */
                addr = lookup(target_name);
                if (immediate[0] == '*') {
                  target[here++] = addr * -1;
                } else {
                  target[here++] = addr;
                }
                
                dict_pass2_count++;
              }
              break;
    case ':':                                     break;
    default:  if (strlen(buffer) > 0) here++;     break;
  }
}

int main(int argc, char **argv) {
  if (argc > 1) {
    np = 0;
    here = 0; unu(argv[1], &pass1);
    here = 0; unu(argv[1], &pass2);
    save();
    printf("%d words (%d bytes) used\n", here, here * 4);
    return 0;
  }
  red(); printf("No file specified.\n"); plain();
  return -1;
}
